/**
 * =============================================================================
 * ASTERON SELF-HEALING RUNTIME - IMPLEMENTAÇÃO
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "self_healing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// =============================================================================
// UTILITÁRIOS
// =============================================================================

static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// =============================================================================
// INICIALIZAÇÃO E DESTRUIÇÃO
// =============================================================================

SelfHealingRuntime* self_healing_init(UnifiedGraph* graph, Scheduler* scheduler) {
    if (graph == NULL || scheduler == NULL) return NULL;
    
    SelfHealingRuntime* rt = (SelfHealingRuntime*)calloc(1, sizeof(SelfHealingRuntime));
    if (rt == NULL) return NULL;
    
    rt->graph = graph;
    rt->scheduler = scheduler;
    rt->auto_parallelize_enabled = true;
    rt->profile_guided_enabled = true;
    rt->is_running = true;
    
    // Critérios padrão de re-otimização
    rt->criteria.cache_miss_threshold = 0.3;
    rt->criteria.branch_mispredict_threshold = 0.2;
    rt->criteria.memory_stall_threshold = 0.25;
    rt->criteria.min_executions = 100;
    rt->criteria.performance_degradation = 0.15; // 15% de degradação
    
    // Inicializa métricas globais
    memset(&rt->global_metrics, 0, sizeof(PerformanceMetrics));
    rt->global_metrics.last_update_ns = get_timestamp_ns();
    
    // Inicializa cache de análises
    rt->parallelization_cache_size = 64;
    rt->parallelization_cache = (ParallelizationAnalysis*)calloc(
        rt->parallelization_cache_size, sizeof(ParallelizationAnalysis));
    
    printf("[Self-Healing] Runtime inicializado\n");
    return rt;
}

void self_healing_destroy(SelfHealingRuntime* rt) {
    if (rt == NULL) return;
    
    // Libera cache de análises
    for (size_t i = 0; i < rt->parallelization_cache_size; i++) {
        parallelization_analysis_destroy(&rt->parallelization_cache[i]);
    }
    free(rt->parallelization_cache);
    
    free(rt);
}

void self_healing_set_auto_parallelize(SelfHealingRuntime* rt, bool enabled) {
    if (rt == NULL) return;
    rt->auto_parallelize_enabled = enabled;
    printf("[Self-Healing] Auto-paralelização %s\n", enabled ? "habilitada" : "desabilitada");
}

void self_healing_set_profile_guided(SelfHealingRuntime* rt, bool enabled) {
    if (rt == NULL) return;
    rt->profile_guided_enabled = enabled;
    printf("[Self-Healing] Profile-guided optimization %s\n", 
           enabled ? "habilitado" : "desabilitado");
}

void self_healing_set_reoptimization_criteria(SelfHealingRuntime* rt, 
                                               ReoptimizationCriteria* criteria) {
    if (rt == NULL || criteria == NULL) return;
    rt->criteria = *criteria;
}

// =============================================================================
// ANÁLISE DE COMPARTILHAMENTO DE ESTADO
// =============================================================================

bool self_healing_share_state(SelfHealingRuntime* rt, UnifiedNode* node1, 
                               UnifiedNode* node2) {
    if (rt == NULL || node1 == NULL || node2 == NULL) return false;
    
    // Verifica se há arestas de dependência entre os nós
    for (size_t i = 0; i < node1->edge_out_count; i++) {
        UnifiedEdge* edge = node1->edges_out[i];
        if (edge->to == node2 && 
            (edge->edge_type == EDGE_DATA_FLOW || 
             edge->edge_type == EDGE_DEPENDENCY)) {
            return true; // Compartilham estado via dependência
        }
    }
    
    // Verifica dependências reversas
    for (size_t i = 0; i < node2->edge_out_count; i++) {
        UnifiedEdge* edge = node2->edges_out[i];
        if (edge->to == node1 && 
            (edge->edge_type == EDGE_DATA_FLOW || 
             edge->edge_type == EDGE_DEPENDENCY)) {
            return true;
        }
    }
    
    // Verifica se ambos dependem do mesmo nó (compartilham estado indiretamente)
    for (size_t i = 0; i < node1->dep_count; i++) {
        UnifiedNode* dep1 = node1->dependencies[i];
        for (size_t j = 0; j < node2->dep_count; j++) {
            UnifiedNode* dep2 = node2->dependencies[j];
            if (dep1 == dep2 && dep1->type == UNIFIED_NODE_VARIABLE) {
                return true; // Compartilham a mesma variável
            }
        }
    }
    
    return false;
}

void self_healing_get_shared_variables(SelfHealingRuntime* rt, 
                                         UnifiedNode* node1,
                                         UnifiedNode* node2,
                                         char*** variables,
                                         size_t* count) {
    if (rt == NULL || node1 == NULL || node2 == NULL || variables == NULL || count == NULL) {
        return;
    }
    
    *variables = NULL;
    *count = 0;
    
    // Encontra variáveis compartilhadas
    char** shared = NULL;
    size_t shared_count = 0;
    size_t shared_capacity = 16;
    shared = (char**)malloc(sizeof(char*) * shared_capacity);
    
    for (size_t i = 0; i < node1->dep_count; i++) {
        UnifiedNode* dep1 = node1->dependencies[i];
        if (dep1->type != UNIFIED_NODE_VARIABLE) continue;
        
        for (size_t j = 0; j < node2->dep_count; j++) {
            UnifiedNode* dep2 = node2->dependencies[j];
            if (dep2 == dep1) {
                // Variável compartilhada encontrada
                if (shared_count >= shared_capacity) {
                    shared_capacity *= 2;
                    shared = (char**)realloc(shared, sizeof(char*) * shared_capacity);
                }
                shared[shared_count++] = strdup(dep1->name);
                break;
            }
        }
    }
    
    *variables = shared;
    *count = shared_count;
}

// =============================================================================
// ANÁLISE DE PARALELIZAÇÃO
// =============================================================================

ParallelizationAnalysis* self_healing_analyze_parallelization(
    SelfHealingRuntime* rt,
    UnifiedNode* node1,
    UnifiedNode* node2) {
    
    if (rt == NULL || node1 == NULL || node2 == NULL) return NULL;
    
    ParallelizationAnalysis* analysis = (ParallelizationAnalysis*)calloc(
        1, sizeof(ParallelizationAnalysis));
    if (analysis == NULL) return NULL;
    
    // Verifica compartilhamento de estado
    bool share_state = self_healing_share_state(rt, node1, node2);
    
    if (share_state) {
        analysis->can_parallelize = false;
        analysis->dep_type = DEP_SHARED_STATE;
        analysis->confidence = 1.0;
        
            // Obtém variáveis compartilhadas (simplificado - em produção, converter para UnifiedNode**)
            // Por enquanto, apenas marca como bloqueado
            analysis->blocking_count = 0;
            analysis->blocking_nodes = NULL;
        return analysis;
    }
    
    // Verifica dependências de fluxo de dados
    bool has_data_flow = false;
    for (size_t i = 0; i < node1->edge_out_count; i++) {
        UnifiedEdge* edge = node1->edges_out[i];
        if (edge->to == node2 && edge->edge_type == EDGE_DATA_FLOW) {
            has_data_flow = true;
            break;
        }
    }
    
    if (has_data_flow) {
        analysis->can_parallelize = false;
        analysis->dep_type = DEP_DATA_FLOW;
        analysis->confidence = 0.9;
        return analysis;
    }
    
    // Verifica dependências de controle
    bool has_control_flow = false;
    for (size_t i = 0; i < node1->edge_out_count; i++) {
        UnifiedEdge* edge = node1->edges_out[i];
        if (edge->to == node2 && edge->edge_type == EDGE_CONTROL_FLOW) {
            has_control_flow = true;
            break;
        }
    }
    
    if (has_control_flow) {
        analysis->can_parallelize = false;
        analysis->dep_type = DEP_CONTROL_FLOW;
        analysis->confidence = 0.8;
        return analysis;
    }
    
    // Verifica I/O (pode paralelizar com cuidado)
    bool has_io = (node1->type == UNIFIED_NODE_IO || node2->type == UNIFIED_NODE_IO);
    if (has_io) {
        analysis->can_parallelize = true;
        analysis->dep_type = DEP_IO;
        analysis->confidence = 0.7;
        return analysis;
    }
    
    // Nenhuma dependência detectada - pode paralelizar!
    analysis->can_parallelize = true;
    analysis->dep_type = DEP_NONE;
    analysis->confidence = 0.95;
    
    return analysis;
}

void self_healing_analyze_parallelization_batch(
    SelfHealingRuntime* rt,
    UnifiedNode** nodes,
    size_t node_count,
    ParallelizationAnalysis** results) {
    
    if (rt == NULL || nodes == NULL || results == NULL) return;
    
    *results = (ParallelizationAnalysis*)calloc(
        node_count * node_count, sizeof(ParallelizationAnalysis));
    if (*results == NULL) return;
    
    // Analisa todos os pares
    for (size_t i = 0; i < node_count; i++) {
        for (size_t j = i + 1; j < node_count; j++) {
            size_t idx = i * node_count + j;
            ParallelizationAnalysis* analysis = 
                self_healing_analyze_parallelization(rt, nodes[i], nodes[j]);
            if (analysis != NULL) {
                (*results)[idx] = *analysis;
                free(analysis);
            }
        }
    }
}

bool self_healing_can_move_to_thread(SelfHealingRuntime* rt, UnifiedNode* node) {
    if (rt == NULL || node == NULL) return false;
    
    // Verifica se o nó tem dependências que impedem paralelização
    for (size_t i = 0; i < node->dep_count; i++) {
        UnifiedNode* dep = node->dependencies[i];
        
        // Se depende de variável compartilhada, não pode mover
        if (dep->type == UNIFIED_NODE_VARIABLE) {
            // Verifica se outra task está usando essa variável
            // (simplificado - em produção, verificar tasks ativas)
            return false;
        }
    }
    
    // Verifica métricas - se é hot path, pode valer a pena paralelizar
    if (node->metrics.is_hot && node->metrics.execution_count > 100) {
        return true;
    }
    
    return false;
}

int self_healing_auto_parallelize(SelfHealingRuntime* rt, UnifiedNode** nodes, 
                                    size_t node_count) {
    if (rt == NULL || !rt->auto_parallelize_enabled || nodes == NULL) return 1;
    
    int parallelized = 0;
    
    // Analisa cada nó
    for (size_t i = 0; i < node_count; i++) {
        UnifiedNode* node = nodes[i];
        
        if (!self_healing_can_move_to_thread(rt, node)) {
            continue;
        }
        
        // Cria task para este nó
        // (simplificado - em produção, integrar com scheduler)
        printf("[Self-Healing] Auto-paralelizando nó '%s'\n", node->name);
        parallelized++;
        rt->auto_parallelizations++;
    }
    
    return parallelized > 0 ? 0 : 1;
}

// =============================================================================
// PROFILE-GUIDED RE-OPTIMIZATION
// =============================================================================

void self_healing_record_metrics(SelfHealingRuntime* rt, UnifiedNode* node,
                                  PerformanceMetrics* metrics) {
    if (rt == NULL || node == NULL || metrics == NULL) return;
    
    // Atualiza métricas do nó
    node->metrics.cache_hits += metrics->cache_hits;
    node->metrics.cache_misses += metrics->cache_misses;
    
    // Calcula taxa de cache miss
    uint64_t total_cache = metrics->cache_hits + metrics->cache_misses;
    if (total_cache > 0) {
        node->metrics.cache_hit_rate = (double)metrics->cache_hits / total_cache;
    }
    
    // Atualiza métricas globais
    rt->global_metrics.cache_hits += metrics->cache_hits;
    rt->global_metrics.cache_misses += metrics->cache_misses;
    rt->global_metrics.branch_mispredictions += metrics->branch_mispredictions;
    rt->global_metrics.execution_count += metrics->execution_count;
    rt->global_metrics.last_update_ns = get_timestamp_ns();
}

ReoptimizationDecision* self_healing_analyze_reoptimization(
    SelfHealingRuntime* rt,
    UnifiedNode* node) {
    
    if (rt == NULL || node == NULL || !rt->profile_guided_enabled) return NULL;
    
    ReoptimizationDecision* decision = (ReoptimizationDecision*)calloc(
        1, sizeof(ReoptimizationDecision));
    if (decision == NULL) return NULL;
    
    // Verifica critérios de re-otimização
    uint64_t total_cache = node->metrics.cache_hits + node->metrics.cache_misses;
    double cache_miss_rate = 0.0;
    if (total_cache > 0) {
        cache_miss_rate = (double)node->metrics.cache_misses / total_cache;
    }
    
    // Verifica cache miss
    if (cache_miss_rate > rt->criteria.cache_miss_threshold &&
        node->metrics.execution_count >= rt->criteria.min_executions) {
        decision->should_reoptimize = true;
        decision->new_tier = JIT_TIER_OPTIMIZING;
        decision->urgency = cache_miss_rate;
        decision->reason = "Alta taxa de cache miss";
        decision->metrics.cache_miss_rate = cache_miss_rate;
        return decision;
    }
    
    // Verifica branch misprediction
    // (simplificado - em produção, coletaria branch_mispredictions do nó)
    uint64_t total_branches = node->metrics.execution_count;
    double branch_mispredict_rate = 0.0;
    if (total_branches > 0) {
        // Assumindo que temos branch_mispredictions nas métricas globais
        branch_mispredict_rate = (double)rt->global_metrics.branch_mispredictions / total_branches;
    }
    
    if (branch_mispredict_rate > rt->criteria.branch_mispredict_threshold &&
        node->metrics.execution_count >= rt->criteria.min_executions) {
        decision->should_reoptimize = true;
        decision->new_tier = JIT_TIER_OPTIMIZING;
        decision->urgency = branch_mispredict_rate;
        decision->reason = "Alta taxa de branch misprediction";
        return decision;
    }
    
    // Verifica degradação de performance
    if (node->metrics.avg_execution_time > node->metrics.max_execution_time * 
        (1.0 - rt->criteria.performance_degradation)) {
        decision->should_reoptimize = true;
        decision->new_tier = JIT_TIER_OPTIMIZING;
        decision->urgency = 0.8;
        decision->reason = "Degradação de performance detectada";
        return decision;
    }
    
    // Não precisa re-otimizar
    decision->should_reoptimize = false;
    return decision;
}

int self_healing_reoptimize_node(SelfHealingRuntime* rt, UnifiedNode* node,
                                   ReoptimizationDecision* decision) {
    if (rt == NULL || node == NULL || decision == NULL || !decision->should_reoptimize) {
        return 1;
    }
    
    printf("[Self-Healing] Re-otimizando nó '%s' (tier %d, razão: %s)\n",
           node->name, decision->new_tier, decision->reason);
    
    // Marca nó para re-otimização
    node->metrics.is_optimized = false;
    node->metrics.is_hot = true; // Marca como hot para forçar recompilação
    
    // Em produção, aqui chamaria tiered_jit_compile_task com o novo tier
    // Por enquanto, apenas marca
    
    rt->reoptimizations++;
    return 0;
}

void self_healing_process_reoptimizations(SelfHealingRuntime* rt) {
    if (rt == NULL || rt->graph == NULL || !rt->profile_guided_enabled) return;
    
    // Processa todos os nós do grafo
    for (size_t i = 0; i < rt->graph->node_count; i++) {
        UnifiedNode* node = rt->graph->nodes[i];
        
        // Analisa re-otimização
        ReoptimizationDecision* decision = 
            self_healing_analyze_reoptimization(rt, node);
        
        if (decision != NULL && decision->should_reoptimize) {
            self_healing_reoptimize_node(rt, node, decision);
        }
        
        reoptimization_decision_destroy(decision);
    }
}

// =============================================================================
// UTILITÁRIOS
// =============================================================================

PerformanceMetrics* self_healing_get_node_metrics(SelfHealingRuntime* rt, 
                                                   UnifiedNode* node) {
    if (rt == NULL || node == NULL) return NULL;
    
    PerformanceMetrics* metrics = (PerformanceMetrics*)malloc(sizeof(PerformanceMetrics));
    if (metrics == NULL) return NULL;
    
    // Converte NodeMetrics para PerformanceMetrics
    metrics->cache_hits = node->metrics.cache_hits;
    metrics->cache_misses = node->metrics.cache_misses;
    metrics->cache_miss_rate = 1.0 - node->metrics.cache_hit_rate;
    metrics->execution_count = node->metrics.execution_count;
    metrics->avg_execution_time_us = node->metrics.avg_execution_time;
    metrics->max_execution_time_us = node->metrics.max_execution_time;
    metrics->last_update_ns = get_timestamp_ns();
    
    return metrics;
}

PerformanceMetrics* self_healing_get_global_metrics(SelfHealingRuntime* rt) {
    if (rt == NULL) return NULL;
    
    PerformanceMetrics* metrics = (PerformanceMetrics*)malloc(sizeof(PerformanceMetrics));
    if (metrics == NULL) return NULL;
    
    *metrics = rt->global_metrics;
    
    // Calcula taxas
    uint64_t total_cache = metrics->cache_hits + metrics->cache_misses;
    if (total_cache > 0) {
        metrics->cache_miss_rate = (double)metrics->cache_misses / total_cache;
    }
    
    return metrics;
}

void self_healing_print_stats(SelfHealingRuntime* rt) {
    if (rt == NULL) return;
    
    printf("\n=== Self-Healing Runtime Statistics ===\n");
    printf("Auto-paralelizações: %llu\n", (unsigned long long)rt->auto_parallelizations);
    printf("Re-otimizações: %llu\n", (unsigned long long)rt->reoptimizations);
    printf("Melhorias de performance: %llu\n", 
           (unsigned long long)rt->performance_improvements);
    printf("\nMétricas Globais:\n");
    printf("  Cache hits: %llu\n", (unsigned long long)rt->global_metrics.cache_hits);
    printf("  Cache misses: %llu\n", (unsigned long long)rt->global_metrics.cache_misses);
    printf("  Execuções: %llu\n", (unsigned long long)rt->global_metrics.execution_count);
    printf("========================================\n\n");
}

void parallelization_analysis_destroy(ParallelizationAnalysis* analysis) {
    if (analysis == NULL) return;
    
    // Libera blocking_nodes (se for array de strings)
    if (analysis->blocking_nodes != NULL) {
        // Em produção, verificar se é array de strings ou nós
        // Por enquanto, assumimos que não precisa liberar (são referências)
    }
    
    // Não libera analysis - é gerenciado pelo chamador
}

void reoptimization_decision_destroy(ReoptimizationDecision* decision) {
    if (decision == NULL) return;
    free(decision);
}

