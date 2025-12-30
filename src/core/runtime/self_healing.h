/**
 * =============================================================================
 * ASTERON SELF-HEALING RUNTIME v1.0
 * =============================================================================
 * 
 * Sistema de runtime auto-adaptativo que usa o grafo unificado para:
 * 
 * 1. AUTO-PARALELIZAÇÃO BASEADA EM GRAFO
 *    - Analisa dependências do grafo para detectar nós independentes
 *    - Move automaticamente para threads sem intervenção do usuário
 *    - Detecta compartilhamento de estado e previne race conditions
 * 
 * 2. PROFILE-GUIDED RE-OPTIMIZATION
 *    - Coleta métricas em tempo real (cache misses, branch mispredictions)
 *    - Dispara recompilação JIT mais agressiva baseada em métricas
 *    - Ajusta otimizações dinamicamente
 * 
 * =============================================================================
 */

#ifndef SELF_HEALING_H
#define SELF_HEALING_H

#include "../../graph/unified_graph.h"
#include "../../scheduler/scheduler.h"
#include "../jit/tiered_jit.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// ANÁLISE DE DEPENDÊNCIAS
// =============================================================================

// Tipo de dependência entre nós
typedef enum {
    DEP_NONE,              // Nenhuma dependência (pode paralelizar)
    DEP_DATA_FLOW,         // Dependência de fluxo de dados
    DEP_CONTROL_FLOW,      // Dependência de fluxo de controle
    DEP_SHARED_STATE,      // Compartilham estado (NÃO pode paralelizar)
    DEP_MEMORY,            // Compartilham memória (NÃO pode paralelizar)
    DEP_IO,                // Dependência I/O (pode paralelizar com cuidado)
    DEP_NETWORK            // Dependência de rede (pode paralelizar)
} DependencyType;

// Resultado da análise de paralelização
typedef struct {
    bool can_parallelize;      // Pode ser paralelizado?
    DependencyType dep_type;    // Tipo de dependência detectada
    double confidence;          // Confiança na análise (0.0 a 1.0)
    char** blocking_variables; // Variáveis que bloqueiam paralelização
    size_t blocking_count;
} ParallelizationAnalysis;

// =============================================================================
// MÉTRICAS DE PERFORMANCE
// =============================================================================

// Métricas coletadas em tempo real
typedef struct {
    // Cache
    uint64_t cache_hits;
    uint64_t cache_misses;
    double cache_miss_rate;     // Taxa de cache miss (0.0 a 1.0)
    
    // Branch prediction
    uint64_t branch_taken;
    uint64_t branch_not_taken;
    uint64_t branch_mispredictions;
    double branch_mispredict_rate; // Taxa de misprediction (0.0 a 1.0)
    
    // Memory
    uint64_t memory_accesses;
    uint64_t memory_stalls;
    double memory_stall_rate;
    
    // Execution
    double avg_execution_time_us;
    double max_execution_time_us;
    uint64_t execution_count;
    
    // JIT
    uint64_t jit_compilations;
    uint64_t deoptimizations;
    double jit_overhead_us;     // Overhead de compilação JIT
    
    // Timestamp
    uint64_t last_update_ns;
} PerformanceMetrics;

// =============================================================================
// DECISÃO DE RE-OTIMIZAÇÃO
// =============================================================================

// Critérios para re-otimização
typedef struct {
    double cache_miss_threshold;        // Threshold de cache miss (padrão: 0.3)
    double branch_mispredict_threshold; // Threshold de branch mispredict (padrão: 0.2)
    double memory_stall_threshold;      // Threshold de memory stall (padrão: 0.25)
    uint64_t min_executions;            // Mínimo de execuções antes de re-otimizar
    double performance_degradation;     // Degradação de performance que justifica re-otimização
} ReoptimizationCriteria;

// Resultado da análise de re-otimização
typedef struct {
    bool should_reoptimize;     // Deve re-otimizar?
    int new_tier;               // Novo tier JIT recomendado
    double urgency;             // Urgência (0.0 a 1.0)
    const char* reason;         // Razão da re-otimização
    PerformanceMetrics metrics;  // Métricas que justificam
} ReoptimizationDecision;

// =============================================================================
// SELF-HEALING RUNTIME
// =============================================================================

// Estado do runtime auto-adaptativo
typedef struct {
    UnifiedGraph* graph;                // Grafo unificado
    Scheduler* scheduler;                // Scheduler para paralelização
    ReoptimizationCriteria criteria;     // Critérios de re-otimização
    
    // Métricas globais
    PerformanceMetrics global_metrics;
    
    // Análises em cache
    ParallelizationAnalysis* parallelization_cache;
    size_t parallelization_cache_size;
    
    // Flags
    bool auto_parallelize_enabled;       // Auto-paralelização habilitada?
    bool profile_guided_enabled;         // Profile-guided habilitado?
    bool is_running;                     // Runtime está ativo?
    
    // Estatísticas
    uint64_t auto_parallelizations;     // Número de paralelizações automáticas
    uint64_t reoptimizations;           // Número de re-otimizações
    uint64_t performance_improvements;   // Melhorias de performance detectadas
} SelfHealingRuntime;

// =============================================================================
// API PÚBLICA
// =============================================================================

// Inicializa o Self-Healing Runtime
SelfHealingRuntime* self_healing_init(UnifiedGraph* graph, Scheduler* scheduler);

// Destrói o runtime
void self_healing_destroy(SelfHealingRuntime* rt);

// Habilita/desabilita auto-paralelização
void self_healing_set_auto_parallelize(SelfHealingRuntime* rt, bool enabled);

// Habilita/desabilita profile-guided optimization
void self_healing_set_profile_guided(SelfHealingRuntime* rt, bool enabled);

// Configura critérios de re-otimização
void self_healing_set_reoptimization_criteria(SelfHealingRuntime* rt, 
                                               ReoptimizationCriteria* criteria);

// =============================================================================
// AUTO-PARALELIZAÇÃO
// =============================================================================

// Analisa se dois nós podem ser paralelizados
ParallelizationAnalysis* self_healing_analyze_parallelization(
    SelfHealingRuntime* rt,
    UnifiedNode* node1,
    UnifiedNode* node2
);

// Analisa um conjunto de nós para paralelização
void self_healing_analyze_parallelization_batch(
    SelfHealingRuntime* rt,
    UnifiedNode** nodes,
    size_t node_count,
    ParallelizationAnalysis** results
);

// Paraleliza automaticamente nós independentes
int self_healing_auto_parallelize(SelfHealingRuntime* rt, UnifiedNode** nodes, 
                                    size_t node_count);

// Verifica se um nó pode ser movido para thread separada
bool self_healing_can_move_to_thread(SelfHealingRuntime* rt, UnifiedNode* node);

// =============================================================================
// PROFILE-GUIDED RE-OPTIMIZATION
// =============================================================================

// Registra métricas de execução de um nó
void self_healing_record_metrics(SelfHealingRuntime* rt, UnifiedNode* node,
                                  PerformanceMetrics* metrics);

// Analisa se um nó precisa ser re-otimizado
ReoptimizationDecision* self_healing_analyze_reoptimization(
    SelfHealingRuntime* rt,
    UnifiedNode* node
);

// Re-otimiza um nó baseado em métricas
int self_healing_reoptimize_node(SelfHealingRuntime* rt, UnifiedNode* node,
                                   ReoptimizationDecision* decision);

// Processa todas as re-otimizações pendentes
void self_healing_process_reoptimizations(SelfHealingRuntime* rt);

// =============================================================================
// ANÁLISE DE COMPARTILHAMENTO DE ESTADO
// =============================================================================

// Verifica se dois nós compartilham estado
bool self_healing_share_state(SelfHealingRuntime* rt, UnifiedNode* node1, 
                               UnifiedNode* node2);

// Obtém variáveis compartilhadas entre nós
void self_healing_get_shared_variables(SelfHealingRuntime* rt, 
                                         UnifiedNode* node1,
                                         UnifiedNode* node2,
                                         char*** variables,
                                         size_t* count);

// =============================================================================
// UTILITÁRIOS
// =============================================================================

// Obtém métricas de um nó
PerformanceMetrics* self_healing_get_node_metrics(SelfHealingRuntime* rt, 
                                                   UnifiedNode* node);

// Obtém métricas globais
PerformanceMetrics* self_healing_get_global_metrics(SelfHealingRuntime* rt);

// Imprime estatísticas do runtime
void self_healing_print_stats(SelfHealingRuntime* rt);

// Libera análise de paralelização
void parallelization_analysis_destroy(ParallelizationAnalysis* analysis);

// Libera decisão de re-otimização
void reoptimization_decision_destroy(ReoptimizationDecision* decision);

#ifdef __cplusplus
}
#endif

#endif // SELF_HEALING_H

