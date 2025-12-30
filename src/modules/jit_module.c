/**
 * =============================================================================
 * ASTERON JIT CROSS-MODULE - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "jit_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * ESTADO GLOBAL
 * ============================================================================= */

static JitModuleCache g_jit_cache = {0};
static int g_jit_initialized = 0;

/* =============================================================================
 * FUNÇÕES AUXILIARES
 * ============================================================================= */

static uint32_t hash_name(const char* module, const char* func) {
    uint32_t hash = 5381;
    const char* s = module;
    while (*s) hash = ((hash << 5) + hash) + *s++;
    hash = ((hash << 5) + hash) + '.';
    s = func;
    while (*s) hash = ((hash << 5) + hash) + *s++;
    return hash;
}

static JitFunctionCode* create_function_code(const char* name) {
    JitFunctionCode* code = (JitFunctionCode*)calloc(1, sizeof(JitFunctionCode));
    if (code == NULL) return NULL;
    
    code->function_name = strdup(name);
    code->code = NULL;
    code->code_size = 0;
    code->tier = 0;
    code->exec_count = 0;
    code->arg_types = NULL;
    code->arg_count = 0;
    code->return_type = VAL_NIL;
    code->type_stable = 0;
    code->links = NULL;
    code->link_count = 0;
    code->deopt_data = NULL;
    code->needs_deopt = 0;
    
    return code;
}

static void free_function_code(JitFunctionCode* code) {
    if (code == NULL) return;
    
    free(code->function_name);
    free(code->code);
    free(code->arg_types);
    
    /* Libera links */
    JitLink* link = code->links;
    while (link) {
        JitLink* next = link->next;
        free(link->source_module);
        free(link->target_module);
        free(link->target_symbol);
        free(link);
        link = next;
    }
    
    free(code->deopt_data);
    free(code);
}

/* =============================================================================
 * INICIALIZAÇÃO
 * ============================================================================= */

void jit_module_init(void) {
    if (g_jit_initialized) return;
    
    memset(&g_jit_cache, 0, sizeof(JitModuleCache));
    
    g_jit_cache.function_capacity = 64;
    g_jit_cache.functions = (JitFunctionCode**)calloc(
        g_jit_cache.function_capacity, sizeof(JitFunctionCode*));
    
    g_jit_cache.module_index = NULL;
    g_jit_cache.module_count = 0;
    
    g_jit_initialized = 1;
    
    printf("[JIT-Module] Sistema JIT cross-module inicializado\n");
}

void jit_module_shutdown(void) {
    if (!g_jit_initialized) return;
    
    /* Libera funções */
    for (size_t i = 0; i < g_jit_cache.function_count; i++) {
        free_function_code(g_jit_cache.functions[i]);
    }
    free(g_jit_cache.functions);
    
    /* Libera índice de módulos */
    for (size_t i = 0; i < g_jit_cache.module_count; i++) {
        free(g_jit_cache.module_index[i].module_name);
        free(g_jit_cache.module_index[i].funcs);
    }
    free(g_jit_cache.module_index);
    
    memset(&g_jit_cache, 0, sizeof(JitModuleCache));
    g_jit_initialized = 0;
    
    printf("[JIT-Module] Sistema JIT cross-module finalizado\n");
}

JitModuleCache* jit_module_get_cache(void) {
    if (!g_jit_initialized) {
        jit_module_init();
    }
    return &g_jit_cache;
}

/* =============================================================================
 * COMPILAÇÃO JIT
 * ============================================================================= */

JitFunctionCode* jit_module_compile(CompiledModule* module,
                                     const char* function_name, int tier) {
    if (module == NULL || function_name == NULL) return NULL;
    
    if (!g_jit_initialized) {
        jit_module_init();
    }
    
    printf("[JIT-Module] Compilando %s.%s (tier %d)\n",
           module->name, function_name, tier);
    
    /* Verifica se já existe */
    JitFunctionCode* existing = jit_module_lookup(module->name, function_name);
    if (existing != NULL && existing->tier >= tier) {
        return existing;
    }
    
    /* Busca símbolo na tabela */
    SymbolEntry* sym = symbol_table_lookup(module->symbols, function_name);
    if (sym == NULL || sym->type != SYMBOL_FUNCTION) {
        printf("[JIT-Module] Função '%s' não encontrada\n", function_name);
        return NULL;
    }
    
    /* Cria novo código JIT */
    JitFunctionCode* code = create_function_code(function_name);
    if (code == NULL) return NULL;
    
    code->tier = tier;
    
    /* TODO: Compilação real para código nativo */
    /* Por enquanto, apenas marca como compilado */
    code->code = malloc(64);
    code->code_size = 64;
    memset(code->code, 0x90, 64); /* NOP slide como placeholder */
    
    /* Adiciona ao cache */
    if (g_jit_cache.function_count >= g_jit_cache.function_capacity) {
        g_jit_cache.function_capacity *= 2;
        g_jit_cache.functions = realloc(g_jit_cache.functions,
            sizeof(JitFunctionCode*) * g_jit_cache.function_capacity);
    }
    g_jit_cache.functions[g_jit_cache.function_count++] = code;
    g_jit_cache.total_code_size += code->code_size;
    g_jit_cache.compilations++;
    
    printf("[JIT-Module] Compilado: %s.%s (%zu bytes)\n",
           module->name, function_name, code->code_size);
    
    return code;
}

JitFunctionCode* jit_module_lookup(const char* module_name,
                                    const char* function_name) {
    if (module_name == NULL || function_name == NULL) return NULL;
    if (!g_jit_initialized) return NULL;
    
    /* Busca linear (TODO: usar hash) */
    for (size_t i = 0; i < g_jit_cache.function_count; i++) {
        JitFunctionCode* code = g_jit_cache.functions[i];
        if (code && strcmp(code->function_name, function_name) == 0) {
            /* TODO: Verificar módulo também */
            return code;
        }
    }
    
    return NULL;
}

void jit_module_invalidate(const char* module_name) {
    if (module_name == NULL || !g_jit_initialized) return;
    
    printf("[JIT-Module] Invalidando JIT para módulo: %s\n", module_name);
    
    /* Marca todos os códigos do módulo como inválidos */
    /* TODO: Implementar índice por módulo */
    
    /* Invalida links que dependem deste módulo */
    for (size_t i = 0; i < g_jit_cache.function_count; i++) {
        JitFunctionCode* code = g_jit_cache.functions[i];
        if (code == NULL) continue;
        
        JitLink* link = code->links;
        while (link) {
            if (strcmp(link->target_module, module_name) == 0) {
                link->is_valid = 0;
                code->needs_deopt = 1;
            }
            link = link->next;
        }
    }
}

/* =============================================================================
 * LINKS ENTRE MÓDULOS
 * ============================================================================= */

void jit_module_add_link(JitFunctionCode* code, JitLinkType type,
                          const char* target_module, const char* target_symbol) {
    if (code == NULL || target_module == NULL || target_symbol == NULL) return;
    
    JitLink* link = (JitLink*)calloc(1, sizeof(JitLink));
    if (link == NULL) return;
    
    link->type = type;
    link->source_module = NULL; /* TODO: Adicionar módulo fonte */
    link->target_module = strdup(target_module);
    link->target_symbol = strdup(target_symbol);
    link->patch_address = NULL;
    link->patch_size = 0;
    link->is_valid = 1;
    
    /* Adiciona à lista */
    link->next = code->links;
    code->links = link;
    code->link_count++;
    
    printf("[JIT-Module] Link adicionado: -> %s.%s\n",
           target_module, target_symbol);
}

int jit_module_validate_links(JitFunctionCode* code) {
    if (code == NULL) return 1;
    
    int all_valid = 1;
    JitLink* link = code->links;
    
    while (link) {
        if (!link->is_valid) {
            all_valid = 0;
            break;
        }
        
        /* Verifica se target ainda existe */
        JitFunctionCode* target = jit_module_lookup(link->target_module,
                                                     link->target_symbol);
        if (target == NULL || target->needs_deopt) {
            link->is_valid = 0;
            all_valid = 0;
        }
        
        link = link->next;
    }
    
    return all_valid;
}

/* =============================================================================
 * INLINING CROSS-MODULE
 * ============================================================================= */

int jit_module_try_inline(JitFunctionCode* caller,
                           const char* callee_module,
                           const char* callee_name) {
    if (caller == NULL || callee_module == NULL || callee_name == NULL) {
        return 0;
    }
    
    /* Busca código do callee */
    JitFunctionCode* callee = jit_module_lookup(callee_module, callee_name);
    if (callee == NULL) {
        printf("[JIT-Module] Inline falhou: %s.%s não encontrado\n",
               callee_module, callee_name);
        return 0;
    }
    
    /* Verifica tamanho */
    if (callee->code_size > JIT_INLINE_SIZE_LIMIT * 4) {
        printf("[JIT-Module] Inline falhou: %s.%s muito grande (%zu bytes)\n",
               callee_module, callee_name, callee->code_size);
        return 0;
    }
    
    /* Verifica estabilidade de tipos */
    if (!callee->type_stable) {
        printf("[JIT-Module] Inline falhou: %s.%s tipos instáveis\n",
               callee_module, callee_name);
        return 0;
    }
    
    /* TODO: Realizar inline real */
    printf("[JIT-Module] Inline: %s.%s\n", callee_module, callee_name);
    
    /* Registra link */
    jit_module_add_link(caller, JIT_LINK_INLINE, callee_module, callee_name);
    
    g_jit_cache.inlines++;
    
    return 1;
}

/* =============================================================================
 * ESPECIALIZAÇÃO DE TIPOS
 * ============================================================================= */

void jit_module_record_types(const char* module_name, const char* function_name,
                              ValueType* arg_types, int arg_count,
                              ValueType return_type) {
    if (module_name == NULL || function_name == NULL) return;
    
    JitFunctionCode* code = jit_module_lookup(module_name, function_name);
    if (code == NULL) return;
    
    code->exec_count++;
    
    /* Verifica se tipos são consistentes */
    if (code->arg_types == NULL && arg_count > 0) {
        /* Primeira observação */
        code->arg_types = malloc(sizeof(ValueType) * arg_count);
        memcpy(code->arg_types, arg_types, sizeof(ValueType) * arg_count);
        code->arg_count = arg_count;
        code->return_type = return_type;
        code->type_stable = 1;
    } else if (code->arg_count == arg_count) {
        /* Verifica consistência */
        int consistent = 1;
        for (int i = 0; i < arg_count; i++) {
            if (code->arg_types[i] != arg_types[i]) {
                consistent = 0;
                break;
            }
        }
        if (code->return_type != return_type) {
            consistent = 0;
        }
        
        if (!consistent) {
            code->type_stable = 0;
            code->needs_deopt = 1;
        }
    } else {
        /* Aridade diferente */
        code->type_stable = 0;
        code->needs_deopt = 1;
    }
}

int jit_module_is_hot(const char* module_name, const char* function_name) {
    JitFunctionCode* code = jit_module_lookup(module_name, function_name);
    
    if (code == NULL) return 0;
    
    return code->exec_count >= JIT_HOT_THRESHOLD;
}

/* =============================================================================
 * DEOPTIMIZAÇÃO
 * ============================================================================= */

void jit_module_deoptimize(const char* module_name, const char* function_name,
                            const char* reason) {
    printf("[JIT-Module] DEOPT: %s.%s - %s\n",
           module_name ? module_name : "?",
           function_name ? function_name : "?",
           reason ? reason : "unknown");
    
    JitFunctionCode* code = jit_module_lookup(module_name, function_name);
    if (code == NULL) return;
    
    /* Invalida código */
    free(code->code);
    code->code = NULL;
    code->code_size = 0;
    code->tier = 0;
    code->needs_deopt = 0;
    
    /* Reseta tipos observados */
    free(code->arg_types);
    code->arg_types = NULL;
    code->arg_count = 0;
    code->type_stable = 0;
    
    g_jit_cache.deopts++;
    
    /* Propaga para quem depende desta função */
    for (size_t i = 0; i < g_jit_cache.function_count; i++) {
        JitFunctionCode* other = g_jit_cache.functions[i];
        if (other == NULL || other == code) continue;
        
        JitLink* link = other->links;
        while (link) {
            if (link->target_module && link->target_symbol &&
                strcmp(link->target_module, module_name) == 0 &&
                strcmp(link->target_symbol, function_name) == 0) {
                
                link->is_valid = 0;
                other->needs_deopt = 1;
                
                /* Recursivamente deoptimiza */
                /* TODO: Prevenir loops infinitos */
            }
            link = link->next;
        }
    }
}

/* =============================================================================
 * ESTATÍSTICAS
 * ============================================================================= */

void jit_module_print_stats(void) {
    printf("\n=== Estatísticas JIT Cross-Module ===\n");
    printf("  Funções compiladas: %zu\n", g_jit_cache.function_count);
    printf("  Tamanho total: %zu bytes\n", g_jit_cache.total_code_size);
    printf("  Compilações: %zu\n", g_jit_cache.compilations);
    printf("  Inlines: %zu\n", g_jit_cache.inlines);
    printf("  Deoptimizações: %zu\n", g_jit_cache.deopts);
    
    if (g_jit_cache.function_count > 0) {
        printf("\n  Funções:\n");
        for (size_t i = 0; i < g_jit_cache.function_count && i < 10; i++) {
            JitFunctionCode* code = g_jit_cache.functions[i];
            if (code) {
                printf("    - %s (tier %d, %zu bytes, %d execs)\n",
                       code->function_name, code->tier,
                       code->code_size, code->exec_count);
            }
        }
        if (g_jit_cache.function_count > 10) {
            printf("    ... e mais %zu\n", g_jit_cache.function_count - 10);
        }
    }
    
    printf("=====================================\n\n");
}

