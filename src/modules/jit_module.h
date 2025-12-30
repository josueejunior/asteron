/**
 * =============================================================================
 * ASTERON JIT CROSS-MODULE v1.0
 * =============================================================================
 * 
 * Sistema de JIT que funciona através de módulos.
 * 
 * Características:
 * - Inlining cross-module
 * - Especialização de tipos entre módulos
 * - Cache de código nativo compartilhado
 * - Deoptimização coordenada
 * 
 * =============================================================================
 */

#ifndef ASTERON_JIT_MODULE_H
#define ASTERON_JIT_MODULE_H

#include "astm.h"
#include "cache.h"
#include "../core/jit/jit.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * TIPOS DE LINK JIT
 * ============================================================================= */

typedef enum {
    JIT_LINK_CALL,          /* Chamada de função */
    JIT_LINK_INLINE,        /* Código inlined */
    JIT_LINK_CONSTANT,      /* Constante propagada */
    JIT_LINK_TYPE_GUARD     /* Guarda de tipo */
} JitLinkType;

/* =============================================================================
 * LINK ENTRE MÓDULOS JIT
 * ============================================================================= */

typedef struct JitLink {
    JitLinkType type;
    
    char* source_module;        /* Módulo que contém o JIT */
    char* target_module;        /* Módulo referenciado */
    char* target_symbol;        /* Símbolo referenciado */
    
    void* patch_address;        /* Endereço para patching */
    size_t patch_size;          /* Tamanho do patch */
    
    ModuleVersion target_version;   /* Versão esperada do target */
    int is_valid;               /* Link ainda válido? */
    
    struct JitLink* next;
} JitLink;

/* =============================================================================
 * CÓDIGO JIT DE MÓDULO
 * ============================================================================= */

typedef struct {
    char* function_name;        /* Nome da função */
    
    void* code;                 /* Código nativo */
    size_t code_size;           /* Tamanho do código */
    
    int tier;                   /* Nível de otimização (0-2) */
    int exec_count;             /* Contador de execuções */
    
    /* Especialização */
    ValueType* arg_types;       /* Tipos dos argumentos observados */
    int arg_count;
    ValueType return_type;      /* Tipo de retorno observado */
    int type_stable;            /* Tipos estão estáveis? */
    
    /* Links para outros módulos */
    JitLink* links;
    int link_count;
    
    /* Deoptimização */
    void* deopt_data;           /* Dados para deopt */
    int needs_deopt;            /* Precisa deoptimizar? */
} JitFunctionCode;

/* =============================================================================
 * CACHE JIT GLOBAL
 * ============================================================================= */

typedef struct {
    /* Funções JIT compiladas */
    JitFunctionCode** functions;
    size_t function_count;
    size_t function_capacity;
    
    /* Índice por módulo */
    struct {
        char* module_name;
        JitFunctionCode** funcs;
        size_t count;
    }* module_index;
    size_t module_count;
    
    /* Estatísticas */
    size_t total_code_size;
    size_t compilations;
    size_t deopts;
    size_t inlines;
} JitModuleCache;

/* =============================================================================
 * API JIT CROSS-MODULE
 * ============================================================================= */

/**
 * Inicializa sistema JIT cross-module
 */
void jit_module_init(void);

/**
 * Finaliza sistema JIT cross-module
 */
void jit_module_shutdown(void);

/**
 * Obtém cache JIT global
 */
JitModuleCache* jit_module_get_cache(void);

/**
 * Compila função para JIT
 * @param module Módulo contendo a função
 * @param function_name Nome da função
 * @param tier Nível de otimização desejado
 * @return Código JIT ou NULL
 */
JitFunctionCode* jit_module_compile(CompiledModule* module, 
                                     const char* function_name, int tier);

/**
 * Busca código JIT existente
 */
JitFunctionCode* jit_module_lookup(const char* module_name, 
                                    const char* function_name);

/**
 * Invalida código JIT de um módulo
 */
void jit_module_invalidate(const char* module_name);

/**
 * Adiciona link entre módulos
 */
void jit_module_add_link(JitFunctionCode* code, JitLinkType type,
                          const char* target_module, const char* target_symbol);

/**
 * Valida todos os links de um código JIT
 */
int jit_module_validate_links(JitFunctionCode* code);

/**
 * Tenta inline de função cross-module
 */
int jit_module_try_inline(JitFunctionCode* caller,
                           const char* callee_module,
                           const char* callee_name);

/**
 * Registra tipos observados para especialização
 */
void jit_module_record_types(const char* module_name, const char* function_name,
                              ValueType* arg_types, int arg_count,
                              ValueType return_type);

/**
 * Verifica se função está quente (pronta para JIT)
 */
int jit_module_is_hot(const char* module_name, const char* function_name);

/**
 * Dispara deoptimização coordenada
 */
void jit_module_deoptimize(const char* module_name, const char* function_name,
                            const char* reason);

/**
 * Imprime estatísticas JIT
 */
void jit_module_print_stats(void);

/* =============================================================================
 * MACROS DE THRESHOLD
 * ============================================================================= */

#define JIT_HOT_THRESHOLD       100     /* Execuções para considerar hot */
#define JIT_INLINE_SIZE_LIMIT   64      /* Máximo de instruções para inline */
#define JIT_TYPE_STABLE_COUNT   10      /* Execuções para considerar tipo estável */
#define JIT_TIER2_THRESHOLD     1000    /* Execuções para tier 2 */

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_JIT_MODULE_H */

