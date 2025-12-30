/**
 * =============================================================================
 * ASTERON MODULE SYSTEM - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "module.h"
#include "../loader/loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * ESTADO GLOBAL
 * ============================================================================= */

static ModuleRegistry g_registry = {0};
static int g_module_system_initialized = 0;

/* =============================================================================
 * FUNÇÕES INTERNAS
 * ============================================================================= */

static ModuleInstance* find_module_by_name(const char* name) {
    ModuleInstance* mod = g_registry.modules;
    while (mod != NULL) {
        if (strcmp(mod->name, name) == 0) {
            return mod;
        }
        mod = mod->next;
    }
    return NULL;
}

static NativeModuleDesc* find_builtin_by_name(const char* name) {
    for (size_t i = 0; i < g_registry.builtin_count; i++) {
        if (strcmp(g_registry.builtins[i]->name, name) == 0) {
            return g_registry.builtins[i];
        }
    }
    return NULL;
}

static ModuleInstance* create_module_instance(const char* name, ModuleType type) {
    ModuleInstance* instance = (ModuleInstance*)malloc(sizeof(ModuleInstance));
    if (instance == NULL) return NULL;
    
    instance->name = strdup(name);
    instance->type = type;
    instance->handle = NULL;
    instance->native_desc = NULL;
    instance->is_loaded = 0;
    instance->ref_count = 1;
    instance->next = NULL;
    
    return instance;
}

/* =============================================================================
 * API PÚBLICA
 * ============================================================================= */

ModuleResult module_system_init(void) {
    if (g_module_system_initialized) {
        return MODULE_OK;
    }
    
    /* Inicializa registry */
    memset(&g_registry, 0, sizeof(ModuleRegistry));
    
    /* Aloca array de builtins */
    g_registry.builtin_capacity = 16;
    g_registry.builtins = (NativeModuleDesc**)malloc(
        sizeof(NativeModuleDesc*) * g_registry.builtin_capacity);
    if (g_registry.builtins == NULL) {
        return MODULE_INIT_ERROR;
    }
    
    /* Aloca array de search paths */
    g_registry.path_capacity = 8;
    g_registry.search_paths = (char**)malloc(
        sizeof(char*) * g_registry.path_capacity);
    if (g_registry.search_paths == NULL) {
        free(g_registry.builtins);
        return MODULE_INIT_ERROR;
    }
    
    /* IMPORTANTE: Marca como inicializado ANTES de adicionar paths
     * para evitar recursão infinita com module_add_search_path() */
    g_module_system_initialized = 1;
    
    /* Adiciona path padrão */
    module_add_search_path("./modules");
    module_add_search_path("./lib");
    
    printf("[Modules] Sistema de módulos inicializado\n");
    
    return MODULE_OK;
}

void module_system_shutdown(void) {
    if (!g_module_system_initialized) {
        return;
    }
    
    /* Descarrega todos os módulos */
    ModuleInstance* mod = g_registry.modules;
    while (mod != NULL) {
        ModuleInstance* next = mod->next;
        module_unload(mod);
        mod = next;
    }
    
    /* Libera builtins */
    if (g_registry.builtins != NULL) {
        free(g_registry.builtins);
    }
    
    /* Libera search paths */
    for (size_t i = 0; i < g_registry.path_count; i++) {
        free(g_registry.search_paths[i]);
    }
    if (g_registry.search_paths != NULL) {
        free(g_registry.search_paths);
    }
    
    memset(&g_registry, 0, sizeof(ModuleRegistry));
    g_module_system_initialized = 0;
    
    printf("[Modules] Sistema de módulos finalizado\n");
}

ModuleRegistry* module_get_registry(void) {
    return &g_registry;
}

ModuleResult module_register_builtin(NativeModuleDesc* desc) {
    if (desc == NULL || desc->name == NULL) {
        return MODULE_INIT_ERROR;
    }
    
    /* Garante que o sistema está inicializado */
    if (!g_module_system_initialized) {
        module_system_init();
    }
    
    /* Verifica se já existe */
    if (find_builtin_by_name(desc->name) != NULL) {
        printf("[Modules] Módulo '%s' já registrado\n", desc->name);
        return MODULE_OK;
    }
    
    /* Expande array se necessário */
    if (g_registry.builtin_count >= g_registry.builtin_capacity) {
        size_t new_cap = g_registry.builtin_capacity * 2;
        NativeModuleDesc** new_arr = (NativeModuleDesc**)realloc(
            g_registry.builtins, sizeof(NativeModuleDesc*) * new_cap);
        if (new_arr == NULL) {
            return MODULE_INIT_ERROR;
        }
        g_registry.builtins = new_arr;
        g_registry.builtin_capacity = new_cap;
    }
    
    /* Adiciona módulo */
    g_registry.builtins[g_registry.builtin_count++] = desc;
    
    printf("[Modules] Módulo builtin '%s' v%s registrado (%zu exports)\n",
           desc->name, desc->version, desc->export_count);
    
    return MODULE_OK;
}

ModuleInstance* module_load(const char* name) {
    if (name == NULL) return NULL;
    
    /* Garante que o sistema está inicializado */
    if (!g_module_system_initialized) {
        module_system_init();
    }
    
    /* Verifica se já está carregado */
    ModuleInstance* existing = find_module_by_name(name);
    if (existing != NULL) {
        existing->ref_count++;
        return existing;
    }
    
    /* Procura em builtins primeiro */
    NativeModuleDesc* builtin = find_builtin_by_name(name);
    if (builtin != NULL) {
        ModuleInstance* instance = create_module_instance(name, MODULE_BUILTIN);
        if (instance == NULL) return NULL;
        
        instance->native_desc = builtin;
        instance->is_loaded = 1;
        
        /* Chama init se existir */
        if (builtin->init != NULL) {
            if (builtin->init() != 0) {
                free(instance->name);
                free(instance);
                return NULL;
            }
        }
        
        /* Adiciona à lista */
        instance->next = g_registry.modules;
        g_registry.modules = instance;
        g_registry.module_count++;
        
        printf("[Modules] Módulo '%s' carregado (builtin)\n", name);
        return instance;
    }
    
    /* TODO: Procurar em arquivos .ast ou .so/.dll */
    printf("[Modules] Módulo '%s' não encontrado\n", name);
    return NULL;
}

void module_unload(ModuleInstance* module) {
    if (module == NULL) return;
    
    module->ref_count--;
    
    if (module->ref_count > 0) {
        return; /* Ainda em uso */
    }
    
    /* Chama cleanup se existir */
    if (module->native_desc != NULL && module->native_desc->cleanup != NULL) {
        module->native_desc->cleanup();
    }
    
    /* Remove da lista */
    if (g_registry.modules == module) {
        g_registry.modules = module->next;
    } else {
        ModuleInstance* prev = g_registry.modules;
        while (prev != NULL && prev->next != module) {
            prev = prev->next;
        }
        if (prev != NULL) {
            prev->next = module->next;
        }
    }
    
    g_registry.module_count--;
    
    printf("[Modules] Módulo '%s' descarregado\n", module->name);
    
    free(module->name);
    free(module);
}

ModuleExport* module_find_symbol(ModuleInstance* module, const char* symbol_name) {
    if (module == NULL || symbol_name == NULL) return NULL;
    
    if (module->native_desc == NULL) return NULL;
    
    for (size_t i = 0; i < module->native_desc->export_count; i++) {
        if (strcmp(module->native_desc->exports[i].name, symbol_name) == 0) {
            return &module->native_desc->exports[i];
        }
    }
    
    return NULL;
}

ModuleResult module_import(AsteronRuntime* rt, const char* module_name, const char* symbol_name) {
    if (rt == NULL || module_name == NULL) {
        return MODULE_LOAD_ERROR;
    }
    
    /* Carrega o módulo */
    ModuleInstance* mod = module_load(module_name);
    if (mod == NULL) {
        return MODULE_NOT_FOUND;
    }
    
    if (mod->native_desc == NULL) {
        return MODULE_LOAD_ERROR;
    }
    
    /* Se symbol_name é NULL, importa tudo */
    if (symbol_name == NULL) {
        for (size_t i = 0; i < mod->native_desc->export_count; i++) {
            ModuleExport* exp = &mod->native_desc->exports[i];
            if (exp->is_function && exp->fn != NULL) {
                AsteronNativeDesc desc = {
                    .name = exp->name,
                    .fn = exp->fn,
                    .min_args = exp->arity,
                    .max_args = exp->arity,
                    .signature = exp->signature,
                    .doc = NULL
                };
                asteron_register_native(rt, &desc);
            }
        }
        printf("[Modules] Importado: %s (todos os símbolos)\n", module_name);
    } else {
        /* Importa símbolo específico */
        ModuleExport* exp = module_find_symbol(mod, symbol_name);
        if (exp == NULL) {
            return MODULE_SYMBOL_NOT_FOUND;
        }
        
        if (exp->is_function && exp->fn != NULL) {
            AsteronNativeDesc desc = {
                .name = exp->name,
                .fn = exp->fn,
                .min_args = exp->arity,
                .max_args = exp->arity,
                .signature = exp->signature,
                .doc = NULL
            };
            asteron_register_native(rt, &desc);
        }
        printf("[Modules] Importado: %s.%s\n", module_name, symbol_name);
    }
    
    return MODULE_OK;
}

void module_add_search_path(const char* path) {
    if (path == NULL) return;
    
    if (!g_module_system_initialized) {
        module_system_init();
    }
    
    /* Expande array se necessário */
    if (g_registry.path_count >= g_registry.path_capacity) {
        size_t new_cap = g_registry.path_capacity * 2;
        char** new_arr = (char**)realloc(
            g_registry.search_paths, sizeof(char*) * new_cap);
        if (new_arr == NULL) return;
        g_registry.search_paths = new_arr;
        g_registry.path_capacity = new_cap;
    }
    
    g_registry.search_paths[g_registry.path_count++] = strdup(path);
}

void module_list_loaded(void) {
    printf("\n=== Módulos Carregados ===\n");
    
    if (g_registry.module_count == 0) {
        printf("  (nenhum módulo carregado)\n");
        return;
    }
    
    ModuleInstance* mod = g_registry.modules;
    while (mod != NULL) {
        const char* type_str = "unknown";
        switch (mod->type) {
            case MODULE_NATIVE:  type_str = "native"; break;
            case MODULE_ASTERON: type_str = "asteron"; break;
            case MODULE_BUILTIN: type_str = "builtin"; break;
        }
        printf("  - %s (%s, refs: %d)\n", mod->name, type_str, mod->ref_count);
        mod = mod->next;
    }
    
    printf("==========================\n\n");
}

void module_print_info(ModuleInstance* module) {
    if (module == NULL) return;
    
    printf("\n=== Módulo: %s ===\n", module->name);
    
    if (module->native_desc != NULL) {
        NativeModuleDesc* desc = module->native_desc;
        printf("  Versão: %s\n", desc->version ? desc->version : "N/A");
        printf("  Descrição: %s\n", desc->description ? desc->description : "N/A");
        printf("  Exports (%zu):\n", desc->export_count);
        
        for (size_t i = 0; i < desc->export_count; i++) {
            ModuleExport* exp = &desc->exports[i];
            if (exp->is_function) {
                printf("    - %s(%s)\n", exp->name, 
                       exp->signature ? exp->signature : "...");
            } else {
                printf("    - %s (const)\n", exp->name);
            }
        }
    }
    
    printf("=========================\n\n");
}

