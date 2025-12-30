/**
 * =============================================================================
 * ASTERON MODULE INTERFACE - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "module_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* =============================================================================
 * GLOBALS
 * ============================================================================= */

static ModuleManager* g_default_manager = NULL;
static Module* g_builtin_modules[64];
static uint32_t g_builtin_count = 0;

/* =============================================================================
 * HELPERS
 * ============================================================================= */

static uint64_t get_time_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/* =============================================================================
 * MODULE MANAGER
 * ============================================================================= */

ModuleManager* module_manager_create(void) {
    ModuleManager* mgr = (ModuleManager*)calloc(1, sizeof(ModuleManager));
    if (!mgr) return NULL;
    
    mgr->next_module_id = 1;
    
    /* Índice de nomes */
    mgr->index_capacity = 64;
    mgr->name_index = calloc(mgr->index_capacity, sizeof(*mgr->name_index));
    
    /* Search paths */
    mgr->search_paths = calloc(16, sizeof(char*));
    mgr->search_path_count = 0;
    
    /* Paths padrão */
    module_manager_add_path(mgr, ".");
    module_manager_add_path(mgr, "./modules");
    module_manager_add_path(mgr, "/usr/lib/asteron/modules");
    
    /* Cria runtime reativo */
    mgr->reactive_rt = reactive_runtime_create();
    
    mgr->poll_interval_ms = 10;
    
    /* Carrega módulos built-in */
    for (uint32_t i = 0; i < g_builtin_count; i++) {
        Module* mod = g_builtin_modules[i];
        mod->manager = mgr;
        mod->id = mgr->next_module_id++;
        mod->reactive_rt = mgr->reactive_rt;
        
        /* Adiciona à lista */
        mod->next = mgr->modules;
        mgr->modules = mod;
        mgr->module_count++;
        
        /* Indexa por nome */
        uint32_t idx = hash_string(mod->name) % mgr->index_capacity;
        mgr->name_index[idx].name = mod->name;
        mgr->name_index[idx].module = mod;
        
        /* Inicializa */
        if (mod->init) {
            uint64_t start = get_time_ns();
            AsteronResult res = mod->init(mod);
            mod->stats.load_time_ns = get_time_ns() - start;
            
            if (res == ASTERON_OK) {
                mod->state = MODULE_STATE_READY;
                printf("[ModuleManager] Loaded: %s v%s\n", mod->name, mod->version);
            } else {
                mod->state = MODULE_STATE_ERROR;
                printf("[ModuleManager] Error loading: %s\n", mod->name);
            }
        } else {
            mod->state = MODULE_STATE_READY;
        }
        
        /* Conecta ao grafo reativo */
        module_connect_reactive(mod, mgr->reactive_rt);
        
        if (mgr->on_module_load) {
            mgr->on_module_load(mod);
        }
    }
    
    g_default_manager = mgr;
    
    return mgr;
}

void module_manager_destroy(ModuleManager* mgr) {
    if (!mgr) return;
    
    /* Descarrega todos os módulos */
    Module* mod = mgr->modules;
    while (mod) {
        Module* next = mod->next;
        
        if (mod->cleanup) {
            mod->cleanup(mod);
        }
        
        /* Não libera módulos built-in aqui */
        mod = next;
    }
    
    if (mgr->reactive_rt) {
        reactive_runtime_destroy(mgr->reactive_rt);
    }
    
    free(mgr->name_index);
    free((void*)mgr->search_paths);
    free(mgr);
    
    if (g_default_manager == mgr) {
        g_default_manager = NULL;
    }
}

void module_manager_add_path(ModuleManager* mgr, const char* path) {
    if (!mgr || !path) return;
    
    /* Verifica se já existe */
    for (uint32_t i = 0; i < mgr->search_path_count; i++) {
        if (strcmp(mgr->search_paths[i], path) == 0) return;
    }
    
    mgr->search_paths[mgr->search_path_count++] = strdup(path);
}

Module* module_manager_load(ModuleManager* mgr, const char* name) {
    if (!mgr || !name) return NULL;
    
    /* Verifica se já está carregado */
    Module* existing = module_manager_get(mgr, name);
    if (existing) return existing;
    
    /* TODO: Carregar de arquivo .astm */
    printf("[ModuleManager] Module not found: %s\n", name);
    
    return NULL;
}

void module_manager_unload(ModuleManager* mgr, Module* mod) {
    if (!mgr || !mod) return;
    
    mod->state = MODULE_STATE_UNLOADING;
    
    if (mod->cleanup) {
        mod->cleanup(mod);
    }
    
    if (mgr->on_module_unload) {
        mgr->on_module_unload(mod);
    }
    
    /* Remove da lista */
    Module** pp = &mgr->modules;
    while (*pp) {
        if (*pp == mod) {
            *pp = mod->next;
            break;
        }
        pp = &(*pp)->next;
    }
    
    mgr->module_count--;
    mod->state = MODULE_STATE_UNLOADED;
}

AsteronResult module_manager_reload(ModuleManager* mgr, Module* mod) {
    if (!mgr || !mod) return ASTERON_ERROR_INVALID;
    
    mod->state = MODULE_STATE_RELOADING;
    mod->stats.reload_count++;
    
    /* Salva estado */
    Module old_mod = *mod;
    
    /* Limpa e reinicializa */
    if (mod->cleanup) {
        mod->cleanup(mod);
    }
    
    if (mod->init) {
        AsteronResult res = mod->init(mod);
        if (res != ASTERON_OK) {
            mod->state = MODULE_STATE_ERROR;
            return res;
        }
    }
    
    /* Notifica hot reload */
    if (mod->on_reload) {
        mod->on_reload(mod, &old_mod);
    }
    
    mod->state = MODULE_STATE_READY;
    
    printf("[ModuleManager] Reloaded: %s\n", mod->name);
    
    return ASTERON_OK;
}

Module* module_manager_get(ModuleManager* mgr, const char* name) {
    if (!mgr || !name) return NULL;
    
    uint32_t idx = hash_string(name) % mgr->index_capacity;
    
    /* Linear probing simples */
    for (uint32_t i = 0; i < mgr->index_capacity; i++) {
        uint32_t probe = (idx + i) % mgr->index_capacity;
        if (mgr->name_index[probe].name == NULL) return NULL;
        if (strcmp(mgr->name_index[probe].name, name) == 0) {
            return mgr->name_index[probe].module;
        }
    }
    
    return NULL;
}

void module_manager_poll(ModuleManager* mgr) {
    if (!mgr) return;
    
    Module* mod = mgr->modules;
    while (mod) {
        if (mod->state == MODULE_STATE_READY && mod->poll) {
            if (mod->poll(mod) && mod->process_events) {
                mod->process_events(mod);
                mod->stats.total_events++;
            }
        }
        mod = mod->next;
    }
    
    /* Propaga mudanças reativas */
    reactive_propagate(mgr->reactive_rt);
}

void module_manager_run(ModuleManager* mgr) {
    if (!mgr) return;
    
    mgr->running = true;
    
    printf("[ModuleManager] Event loop started\n");
    
    while (mgr->running) {
        module_manager_poll(mgr);
        
        /* Sleep para não queimar CPU */
#ifdef _WIN32
        Sleep(mgr->poll_interval_ms);
#else
        struct timespec ts = {
            .tv_sec = 0,
            .tv_nsec = mgr->poll_interval_ms * 1000000
        };
        nanosleep(&ts, NULL);
#endif
    }
    
    printf("[ModuleManager] Event loop stopped\n");
}

void module_manager_stop(ModuleManager* mgr) {
    if (mgr) {
        mgr->running = false;
    }
}

void module_manager_register_builtin(Module* mod) {
    if (g_builtin_count < 64) {
        g_builtin_modules[g_builtin_count++] = mod;
    }
}

/* =============================================================================
 * MODULE
 * ============================================================================= */

Module* module_create(const char* name, const char* version) {
    Module* mod = (Module*)calloc(1, sizeof(Module));
    if (!mod) return NULL;
    
    mod->name = strdup(name);
    mod->version = strdup(version);
    mod->state = MODULE_STATE_UNLOADED;
    
    return mod;
}

ModuleFunction* module_add_function(Module* mod, const char* name,
                                     AsteronNativeFn fn, int min_args, int max_args,
                                     const char* signature, FunctionFlags flags) {
    if (!mod || !name || !fn) return NULL;
    
    ModuleFunction* func = (ModuleFunction*)calloc(1, sizeof(ModuleFunction));
    func->name = strdup(name);
    func->id = mod->function_count;
    func->native_fn = fn;
    func->min_args = min_args;
    func->max_args = max_args;
    func->signature = signature ? strdup(signature) : NULL;
    func->flags = flags;
    func->owner = mod;
    
    /* Adiciona à lista */
    func->next = mod->functions;
    mod->functions = func;
    mod->function_count++;
    
    return func;
}

ModuleVariable* module_add_variable(Module* mod, const char* name,
                                     AsteronValue initial, VariableFlags flags) {
    if (!mod || !name) return NULL;
    
    ModuleVariable* var = (ModuleVariable*)calloc(1, sizeof(ModuleVariable));
    var->name = strdup(name);
    var->id = mod->variable_count;
    var->value = initial;
    var->prev_value = initial;
    var->flags = flags;
    var->owner = mod;
    
    /* Adiciona à lista */
    var->next = mod->variables;
    mod->variables = var;
    mod->variable_count++;
    
    return var;
}

ModuleType* module_add_type(Module* mod, const char* name, size_t size) {
    if (!mod || !name) return NULL;
    
    ModuleType* type = (ModuleType*)calloc(1, sizeof(ModuleType));
    type->name = strdup(name);
    type->id = mod->type_count;
    type->size = size;
    type->owner = mod;
    
    type->next = mod->types;
    mod->types = type;
    mod->type_count++;
    
    return type;
}

void module_add_dependency(Module* mod, Module* dep) {
    if (!mod || !dep) return;
    
    /* Expande array se necessário */
    mod->dependencies = realloc(mod->dependencies,
        (mod->dependency_count + 1) * sizeof(Module*));
    mod->dependencies[mod->dependency_count++] = dep;
}

ModuleFunction* module_get_function(Module* mod, const char* name) {
    if (!mod || !name) return NULL;
    
    ModuleFunction* func = mod->functions;
    while (func) {
        if (strcmp(func->name, name) == 0) return func;
        func = func->next;
    }
    
    return NULL;
}

ModuleVariable* module_get_variable(Module* mod, const char* name) {
    if (!mod || !name) return NULL;
    
    ModuleVariable* var = mod->variables;
    while (var) {
        if (strcmp(var->name, name) == 0) return var;
        var = var->next;
    }
    
    return NULL;
}

void module_set_variable(Module* mod, const char* name, AsteronValue value) {
    ModuleVariable* var = module_get_variable(mod, name);
    if (!var) return;
    
    var->prev_value = var->value;
    var->value = value;
    
    /* Dispara reatividade */
    if ((var->flags & VAR_FLAG_REACTIVE) && var->reactive_node) {
        reactive_set(mod->reactive_rt, var->reactive_node, value);
    }
}

/* =============================================================================
 * INTEGRAÇÃO REATIVA
 * ============================================================================= */

void module_connect_reactive(Module* mod, ReactiveRuntime* rt) {
    if (!mod || !rt) return;
    
    mod->reactive_rt = rt;
    
    /* Cria nó raiz do módulo */
    char name[128];
    snprintf(name, sizeof(name), "module:%s", mod->name);
    mod->module_node = reactive_context(rt, name);
    
    /* Cria nós para variáveis reativas */
    ModuleVariable* var = mod->variables;
    while (var) {
        if (var->flags & VAR_FLAG_REACTIVE) {
            snprintf(name, sizeof(name), "%s.%s", mod->name, var->name);
            var->reactive_node = reactive_state(rt, name, var->value);
        }
        var = var->next;
    }
    
    /* Cria nós para funções no grafo */
    ModuleFunction* func = mod->functions;
    while (func) {
        if (func->flags & FUNC_FLAG_REACTIVE) {
            snprintf(name, sizeof(name), "%s.%s", mod->name, func->name);
            func->graph_node = reactive_effect(rt, name, NULL, func);
        }
        func = func->next;
    }
}

ReactiveNode* module_when_variable(Module* mod, const char* var_name,
                                    ComputeFn effect) {
    if (!mod || !var_name || !effect) return NULL;
    
    ModuleVariable* var = module_get_variable(mod, var_name);
    if (!var || !var->reactive_node) return NULL;
    
    char name[128];
    snprintf(name, sizeof(name), "when:%s.%s", mod->name, var_name);
    
    ReactiveNode* node = reactive_effect(mod->reactive_rt, name, effect, var);
    reactive_add_dep(node, var->reactive_node);
    
    return node;
}

void module_make_event_source(Module* mod, const char* var_name) {
    ModuleVariable* var = module_get_variable(mod, var_name);
    if (var) {
        var->flags |= VAR_FLAG_EVENT_SOURCE | VAR_FLAG_REACTIVE;
        
        /* Cria nó reativo se não existir */
        if (!var->reactive_node && mod->reactive_rt) {
            char name[128];
            snprintf(name, sizeof(name), "%s.%s", mod->name, var_name);
            var->reactive_node = reactive_state(mod->reactive_rt, name, var->value);
        }
    }
}

/* =============================================================================
 * INTEGRAÇÃO COM VM
 * ============================================================================= */

AsteronResult module_register_in_vm(Module* mod, void* vm) {
    /* TODO: Implementar quando VM estiver pronta para integração */
    (void)mod;
    (void)vm;
    return ASTERON_OK;
}

void* module_compile_call(Module* mod, ModuleFunction* fn) {
    /* TODO: Gerar bytecode para chamar função */
    (void)mod;
    (void)fn;
    return NULL;
}

void module_add_to_symbols(Module* mod, void* symbol_table) {
    /* TODO: Adicionar símbolos à tabela */
    (void)mod;
    (void)symbol_table;
}

