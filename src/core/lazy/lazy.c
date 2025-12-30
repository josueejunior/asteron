#define _POSIX_C_SOURCE 200809L
#include "lazy.h"
#include "../../utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * FUNÇÕES AUXILIARES
 * ============================================================================= */

static LazyExpr* create_lazy_expr(ASTNode* ast, LazyStrategy strategy) {
    LazyExpr* expr = (LazyExpr*)calloc(1, sizeof(LazyExpr));
    if (expr == NULL) return NULL;
    
    expr->ast = ast;
    expr->state = LAZY_UNEVALUATED;
    expr->strategy = strategy;
    expr->cache.is_valid = 0;
    expr->is_dirty = 0;
    expr->speculative.enabled = 0;
    expr->speculative.confidence = 0.0;
    
    expr->dep_capacity = 8;
    expr->dependencies = (char**)calloc(expr->dep_capacity, sizeof(char*));
    
    expr->dependent_capacity = 8;
    expr->dependents = (LazyExpr**)calloc(expr->dependent_capacity, sizeof(LazyExpr*));
    
    return expr;
}

static void destroy_lazy_expr(LazyExpr* expr) {
    if (expr == NULL) return;
    
    if (expr->cache.is_valid) {
        value_destroy(expr->cache.cached_value);
    }
    if (expr->speculative.predicted_value.type != VAL_NIL) {
        value_destroy(expr->speculative.predicted_value);
    }
    
    if (expr->dependencies) {
        for (size_t i = 0; i < expr->dep_count; i++) {
            free(expr->dependencies[i]);
        }
        free(expr->dependencies);
    }
    
    if (expr->dependents) {
        free(expr->dependents);
    }
    
    free(expr);
}

/* =============================================================================
 * CRIAÇÃO E DESTRUIÇÃO
 * ============================================================================= */

LazyRuntime* lazy_runtime_create(void) {
    LazyRuntime* rt = (LazyRuntime*)calloc(1, sizeof(LazyRuntime));
    if (rt == NULL) return NULL;
    
    rt->expr_capacity = 64;
    rt->expressions = (LazyExpr**)calloc(rt->expr_capacity, sizeof(LazyExpr*));
    if (rt->expressions == NULL) {
        free(rt);
        return NULL;
    }
    
    rt->spec_config.enabled = 1;
    rt->spec_config.threshold = 0.7;
    rt->spec_config.max_speculative = 4;
    rt->spec_config.current_speculative = 0;
    
    return rt;
}

void lazy_runtime_destroy(LazyRuntime* rt) {
    if (rt == NULL) return;
    
    for (size_t i = 0; i < rt->expr_count; i++) {
        destroy_lazy_expr(rt->expressions[i]);
    }
    free(rt->expressions);
    free(rt);
}

/* =============================================================================
 * EXPRESSÕES LAZY
 * ============================================================================= */

LazyExpr* lazy_create_expr(LazyRuntime* rt, ASTNode* ast, LazyStrategy strategy) {
    if (rt == NULL || ast == NULL) return NULL;
    
    if (rt->expr_count >= rt->expr_capacity) {
        rt->expr_capacity *= 2;
        LazyExpr** new_exprs = (LazyExpr**)realloc(
            rt->expressions, sizeof(LazyExpr*) * rt->expr_capacity);
        if (new_exprs == NULL) return NULL;
        rt->expressions = new_exprs;
    }
    
    LazyExpr* expr = create_lazy_expr(ast, strategy);
    if (expr == NULL) return NULL;
    
    rt->expressions[rt->expr_count++] = expr;
    return expr;
}

void lazy_destroy_expr(LazyRuntime* rt, LazyExpr* expr) {
    if (rt == NULL || expr == NULL) return;
    
    // Remove da lista
    for (size_t i = 0; i < rt->expr_count; i++) {
        if (rt->expressions[i] == expr) {
            rt->expressions[i] = rt->expressions[rt->expr_count - 1];
            rt->expr_count--;
            break;
        }
    }
    
    destroy_lazy_expr(expr);
}

Value lazy_evaluate(LazyRuntime* rt, LazyExpr* expr, VM* vm) {
    if (rt == NULL || expr == NULL || vm == NULL) {
        return value_nil();
    }
    
    // Se já está avaliada e cache é válido, retorna cache
    if (expr->state == LAZY_EVALUATED && expr->cache.is_valid && !expr->is_dirty) {
        expr->cache.hit_count++;
        rt->stats.cache_hits++;
        return expr->cache.cached_value;
    }
    
    // Se está sendo avaliada (ciclo detectado), retorna nil
    if (expr->state == LAZY_EVALUATING) {
        return value_nil();
    }
    
    expr->state = LAZY_EVALUATING;
    rt->stats.cache_misses++;
    
    // Avalia usando callback ou método padrão
    Value result;
    if (rt->evaluator != NULL) {
        result = rt->evaluator(rt, expr, vm);
    } else {
        // Avaliação padrão simplificada
        result = value_nil();
    }
    
    // Armazena no cache
    if (expr->cache.is_valid) {
        value_destroy(expr->cache.cached_value);
    }
    expr->cache.cached_value = result;
    expr->cache.is_valid = 1;
    expr->state = LAZY_EVALUATED;
    expr->is_dirty = 0;
    
    rt->stats.total_evaluations++;
    
    if (rt->on_evaluated != NULL) {
        rt->on_evaluated(rt, expr, result);
    }
    
    return result;
}

Value lazy_get_value(LazyRuntime* rt, LazyExpr* expr, VM* vm) {
    if (expr == NULL) return value_nil();
    
    // Se tem cache válido, retorna
    if (expr->cache.is_valid && !expr->is_dirty) {
        expr->cache.hit_count++;
        rt->stats.cache_hits++;
        return expr->cache.cached_value;
    }
    
    // Caso contrário, avalia
    return lazy_evaluate(rt, expr, vm);
}

void lazy_invalidate(LazyRuntime* rt, LazyExpr* expr) {
    if (expr == NULL) return;
    
    expr->is_dirty = 1;
    expr->cache.is_valid = 0;
    
    // Invalida dependentes recursivamente
    for (size_t i = 0; i < expr->dependent_count; i++) {
        lazy_invalidate(rt, expr->dependents[i]);
    }
}

void lazy_add_dependency(LazyExpr* expr, LazyExpr* dep) {
    if (expr == NULL || dep == NULL) return;
    
    // Adiciona à lista de dependências
    if (expr->dep_count >= expr->dep_capacity) {
        expr->dep_capacity *= 2;
        char** new_deps = (char**)realloc(
            expr->dependencies, sizeof(char*) * expr->dep_capacity);
        if (new_deps == NULL) return;
        expr->dependencies = new_deps;
    }
    
    // (Simplificado - em produção, extrairia nome da variável do AST)
    expr->dependencies[expr->dep_count++] = strdup("dep");
    
    // Adiciona como dependente
    if (dep->dependent_count >= dep->dependent_capacity) {
        dep->dependent_capacity *= 2;
        LazyExpr** new_dependents = (LazyExpr**)realloc(
            dep->dependents, sizeof(LazyExpr*) * dep->dependent_capacity);
        if (new_dependents == NULL) return;
        dep->dependents = new_dependents;
    }
    
    dep->dependents[dep->dependent_count++] = expr;
}

/* =============================================================================
 * EXECUÇÃO ESPECULATIVA
 * ============================================================================= */

void lazy_enable_speculation(LazyRuntime* rt, LazyExpr* expr, double confidence) {
    if (rt == NULL || expr == NULL) return;
    
    expr->speculative.enabled = 1;
    expr->speculative.confidence = confidence;
}

Value lazy_speculate(LazyRuntime* rt, LazyExpr* expr, VM* vm) {
    if (rt == NULL || expr == NULL || vm == NULL) return value_nil();
    
    if (!rt->spec_config.enabled || !expr->speculative.enabled) {
        return value_nil();
    }
    
    if (expr->speculative.confidence < rt->spec_config.threshold) {
        return value_nil();
    }
    
    if (rt->spec_config.current_speculative >= rt->spec_config.max_speculative) {
        return value_nil();
    }
    
    rt->spec_config.current_speculative++;
    rt->stats.speculative_executions++;
    
    // Executa especulativamente
    Value predicted = lazy_evaluate(rt, expr, vm);
    
    if (expr->speculative.predicted_value.type != VAL_NIL) {
        value_destroy(expr->speculative.predicted_value);
    }
    expr->speculative.predicted_value = predicted;
    
    rt->spec_config.current_speculative--;
    
    return predicted;
}

void lazy_verify_speculation(LazyRuntime* rt, LazyExpr* expr, Value actual_value) {
    if (rt == NULL || expr == NULL) return;
    
    if (!expr->speculative.enabled) return;
    
    // Compara valor previsto com real
    int match = 0;
    if (expr->speculative.predicted_value.type == actual_value.type) {
        if (actual_value.type == VAL_NUMBER) {
            match = (expr->speculative.predicted_value.as.number == actual_value.as.number);
        } else if (actual_value.type == VAL_BOOL) {
            match = (expr->speculative.predicted_value.as.boolean == actual_value.as.boolean);
        }
        // (Simplificado - em produção, compararia todos os tipos)
    }
    
    if (match) {
        expr->speculative.prediction_hits++;
        rt->stats.speculative_hits++;
        
        // Aumenta confiança
        expr->speculative.confidence = 
            (expr->speculative.confidence * 0.9) + 0.1;
        if (expr->speculative.confidence > 1.0) {
            expr->speculative.confidence = 1.0;
        }
    } else {
        expr->speculative.prediction_misses++;
        rt->stats.speculative_misses++;
        
        // Diminui confiança
        expr->speculative.confidence = 
            (expr->speculative.confidence * 0.9) - 0.1;
        if (expr->speculative.confidence < 0.0) {
            expr->speculative.confidence = 0.0;
        }
    }
}

/* =============================================================================
 * CACHE E MEMOIZAÇÃO
 * ============================================================================= */

void lazy_clear_cache(LazyExpr* expr) {
    if (expr == NULL) return;
    
    if (expr->cache.is_valid) {
        value_destroy(expr->cache.cached_value);
        expr->cache.is_valid = 0;
    }
    expr->cache.hit_count = 0;
    expr->cache.miss_count = 0;
}

void lazy_clear_all_caches(LazyRuntime* rt) {
    if (rt == NULL) return;
    
    for (size_t i = 0; i < rt->expr_count; i++) {
        lazy_clear_cache(rt->expressions[i]);
    }
}

void lazy_get_cache_stats(LazyRuntime* rt, size_t* hits, size_t* misses, 
                          double* hit_rate) {
    if (rt == NULL) return;
    
    size_t total_hits = 0;
    size_t total_misses = 0;
    
    for (size_t i = 0; i < rt->expr_count; i++) {
        total_hits += rt->expressions[i]->cache.hit_count;
        total_misses += rt->expressions[i]->cache.miss_count;
    }
    
    if (hits) *hits = total_hits;
    if (misses) *misses = total_misses;
    
    size_t total = total_hits + total_misses;
    if (hit_rate && total > 0) {
        *hit_rate = (double)total_hits / total;
    }
}

/* =============================================================================
 * ANÁLISE E OTIMIZAÇÃO
 * ============================================================================= */

void lazy_analyze_dependencies(LazyRuntime* rt, LazyExpr* expr) {
    // Implementação simplificada
    // Em produção, analisaria a AST para extrair dependências reais
}

void lazy_identify_parallel(LazyRuntime* rt, LazyExpr*** parallel_exprs, 
                            size_t* count) {
    // Implementação simplificada
    if (parallel_exprs) *parallel_exprs = NULL;
    if (count) *count = 0;
}

void lazy_optimize_strategies(LazyRuntime* rt) {
    if (rt == NULL) return;
    
    // Otimiza estratégias baseado em estatísticas
    for (size_t i = 0; i < rt->expr_count; i++) {
        LazyExpr* expr = rt->expressions[i];
        
        // Se tem muitas cache hits, mantém lazy
        double hit_rate = 0.0;
        size_t total = expr->cache.hit_count + expr->cache.miss_count;
        if (total > 0) {
            hit_rate = (double)expr->cache.hit_count / total;
        }
        
        if (hit_rate > 0.8 && expr->strategy == LAZY_STRATEGY_STRICT) {
            expr->strategy = LAZY_STRATEGY_LAZY;
        }
    }
}

