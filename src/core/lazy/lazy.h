#ifndef LAZY_H
#define LAZY_H

#include "../vm/vm.h"
#include "../ast/ast.h"
#include <stddef.h>

/* =============================================================================
 * LAZY EVALUATION E SPECULATIVE EXECUTION
 * =============================================================================
 * Sistema de avaliação preguiçosa que calcula apenas o que será usado,
 * com suporte a execução especulativa para prever execuções futuras.
 * ============================================================================= */

// Estado de uma expressão lazy
typedef enum {
    LAZY_UNEVALUATED,      // Ainda não foi avaliada
    LAZY_EVALUATING,       // Sendo avaliada agora
    LAZY_EVALUATED,        // Já foi avaliada
    LAZY_ERROR             // Erro durante avaliação
} LazyState;

// Estratégia de avaliação
typedef enum {
    LAZY_STRATEGY_STRICT,      // Avaliação estrita (normal)
    LAZY_STRATEGY_LAZY,        // Avaliação preguiçosa
    LAZY_STRATEGY_SPECULATIVE  // Execução especulativa
} LazyStrategy;

// Cache de resultado lazy
typedef struct LazyCache {
    Value cached_value;         // Valor em cache
    int is_valid;               // Cache é válido?
    size_t hit_count;           // Quantas vezes foi usado
    size_t miss_count;          // Quantas vezes foi recalculado
} LazyCache;

// Expressão lazy
typedef struct LazyExpr {
    ASTNode* ast;               // AST da expressão
    LazyState state;            // Estado atual
    LazyStrategy strategy;      // Estratégia de avaliação
    LazyCache cache;            // Cache de resultado
    
    // Dependências (quais variáveis esta expressão depende)
    char** dependencies;       // Nomes das variáveis dependentes
    size_t dep_count;
    size_t dep_capacity;
    
    // Dependentes (quem depende desta expressão)
    struct LazyExpr** dependents;
    size_t dependent_count;
    size_t dependent_capacity;
    
    // Execução especulativa
    struct {
        int enabled;            // Execução especulativa habilitada?
        double confidence;      // Confiança na previsão (0.0-1.0)
        Value predicted_value;  // Valor previsto
        int prediction_hits;     // Quantas vezes a previsão acertou
        int prediction_misses;  // Quantas vezes a previsão errou
    } speculative;
    
    // Metadados
    void* user_data;
    int is_dirty;               // Precisa reavaliar?
} LazyExpr;

// Runtime de lazy evaluation
typedef struct LazyRuntime {
    LazyExpr** expressions;    // Todas as expressões lazy
    size_t expr_count;
    size_t expr_capacity;
    
    // Especulação
    struct {
        int enabled;            // Especulação habilitada globalmente?
        double threshold;       // Threshold de confiança para especular
        size_t max_speculative;  // Máximo de execuções especulativas simultâneas
        size_t current_speculative; // Execuções especulativas ativas
    } spec_config;
    
    // Estatísticas
    struct {
        size_t total_evaluations;
        size_t cache_hits;
        size_t cache_misses;
        size_t speculative_executions;
        size_t speculative_hits;
        size_t speculative_misses;
    } stats;
    
    // Callbacks
    Value (*evaluator)(struct LazyRuntime* rt, LazyExpr* expr, VM* vm);
    void (*on_evaluated)(struct LazyRuntime* rt, LazyExpr* expr, Value result);
} LazyRuntime;

/* =============================================================================
 * API - CRIAÇÃO E DESTRUIÇÃO
 * ============================================================================= */

/**
 * Cria runtime de lazy evaluation
 */
LazyRuntime* lazy_runtime_create(void);

/**
 * Destrói runtime
 */
void lazy_runtime_destroy(LazyRuntime* rt);

/* =============================================================================
 * API - EXPRESSÕES LAZY
 * ============================================================================= */

/**
 * Cria expressão lazy
 */
LazyExpr* lazy_create_expr(LazyRuntime* rt, ASTNode* ast, LazyStrategy strategy);

/**
 * Destrói expressão lazy
 */
void lazy_destroy_expr(LazyRuntime* rt, LazyExpr* expr);

/**
 * Avalia expressão lazy (força avaliação se necessário)
 */
Value lazy_evaluate(LazyRuntime* rt, LazyExpr* expr, VM* vm);

/**
 * Obtém valor da expressão (usa cache se disponível)
 */
Value lazy_get_value(LazyRuntime* rt, LazyExpr* expr, VM* vm);

/**
 * Marca expressão como suja (invalida cache)
 */
void lazy_invalidate(LazyRuntime* rt, LazyExpr* expr);

/**
 * Adiciona dependência entre expressões
 */
void lazy_add_dependency(LazyExpr* expr, LazyExpr* dep);

/* =============================================================================
 * API - EXECUÇÃO ESPECULATIVA
 * ============================================================================= */

/**
 * Habilita execução especulativa para uma expressão
 */
void lazy_enable_speculation(LazyRuntime* rt, LazyExpr* expr, double confidence);

/**
 * Executa especulativamente uma expressão
 */
Value lazy_speculate(LazyRuntime* rt, LazyExpr* expr, VM* vm);

/**
 * Verifica se previsão especulativa estava correta
 */
void lazy_verify_speculation(LazyRuntime* rt, LazyExpr* expr, Value actual_value);

/* =============================================================================
 * API - CACHE E MEMOIZAÇÃO
 * ============================================================================= */

/**
 * Limpa cache de uma expressão
 */
void lazy_clear_cache(LazyExpr* expr);

/**
 * Limpa todos os caches
 */
void lazy_clear_all_caches(LazyRuntime* rt);

/**
 * Obtém estatísticas de cache
 */
void lazy_get_cache_stats(LazyRuntime* rt, size_t* hits, size_t* misses, 
                          double* hit_rate);

/* =============================================================================
 * API - ANÁLISE E OTIMIZAÇÃO
 * ============================================================================= */

/**
 * Analisa dependências de uma expressão
 */
void lazy_analyze_dependencies(LazyRuntime* rt, LazyExpr* expr);

/**
 * Identifica expressões que podem ser avaliadas em paralelo
 */
void lazy_identify_parallel(LazyRuntime* rt, LazyExpr*** parallel_exprs, 
                            size_t* count);

/**
 * Otimiza estratégias de avaliação baseado em uso
 */
void lazy_optimize_strategies(LazyRuntime* rt);

#endif // LAZY_H

