/**
 * =============================================================================
 * ASTERON INTENT-BASED SCHEDULING v1.0
 * =============================================================================
 * 
 * Sistema de programação por "intenção" onde o programador diz O QUE quer,
 * e o Grafo Unificado decide COMO executar.
 * 
 * COMPONENTES:
 * 
 * 1. INTENT-BASED SCHEDULING
 *    - Programador declara intenção (ex: "processar array rapidamente")
 *    - Grafo Unificado decide infraestrutura (threads, SIMD, etc)
 * 
 * 2. HEURÍSTICA DE HARDWARE DINÂMICA
 *    - Detecta capacidades do hardware (AVX-512, múltiplos núcleos)
 *    - Reescreve bytecode em tempo real para usar SIMD
 *    - Otimizações específicas por arquitetura
 * 
 * 3. PREVISÃO DE CARGA VIA IA
 *    - Usa framework de IA para prever quando WHEN blocks serão disparados
 *    - Pré-aloca memória e "aquece" JIT antes do evento
 *    - Otimizações proativas baseadas em padrões históricos
 * 
 * =============================================================================
 */

#ifndef INTENT_BASED_H
#define INTENT_BASED_H

#include "../../graph/unified_graph.h"
#include "../../scheduler/scheduler.h"
#include "../../core/vm/vm.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// INTENT DECLARATIONS
// =============================================================================

// Tipo de intenção do programador
typedef enum {
    INTENT_FAST,                // Executar rapidamente
    INTENT_PARALLEL,            // Executar em paralelo
    INTENT_MEMORY_EFFICIENT,    // Usar memória eficientemente
    INTENT_IO_OPTIMIZED,        // Otimizado para I/O
    INTENT_COMPUTE_INTENSIVE,   // Computação intensiva
    INTENT_LOW_LATENCY,         // Baixa latência
    INTENT_HIGH_THROUGHPUT,     // Alto throughput
    INTENT_BALANCED             // Balanceado (padrão)
} IntentType;

// Declaração de intenção
typedef struct {
    IntentType type;            // Tipo de intenção
    const char* description;    // Descrição em texto livre
    UnifiedNode* target_node;   // Nó do grafo alvo
    double priority;            // Prioridade (0.0 a 1.0)
    void* constraints;          // Restrições adicionais (opcional)
} IntentDeclaration;

// =============================================================================
// HARDWARE CAPABILITIES
// =============================================================================

// Capacidades de SIMD
typedef enum {
    SIMD_NONE,                  // Sem SIMD
    SIMD_SSE,                   // SSE (128 bits)
    SIMD_SSE2,                  // SSE2
    SIMD_SSE3,                  // SSE3
    SIMD_SSE4,                  // SSE4
    SIMD_AVX,                   // AVX (256 bits)
    SIMD_AVX2,                  // AVX2
    SIMD_AVX512                 // AVX-512 (512 bits)
} SimdCapability;

// Informações do hardware
typedef struct {
    // CPU
    int cpu_count;              // Número de CPUs/núcleos
    int physical_cores;         // Núcleos físicos
    int logical_cores;          // Núcleos lógicos (com hyperthreading)
    char* cpu_brand;            // Marca do processador
    uint64_t cpu_frequency_hz;  // Frequência do CPU
    
    // SIMD
    SimdCapability max_simd;    // Máxima capacidade SIMD
    bool has_avx512;            // Suporta AVX-512?
    bool has_avx2;              // Suporta AVX2?
    bool has_avx;               // Suporta AVX?
    bool has_sse4;              // Suporta SSE4?
    
    // Cache
    size_t l1_cache_size;       // Tamanho do L1 cache
    size_t l2_cache_size;       // Tamanho do L2 cache
    size_t l3_cache_size;       // Tamanho do L3 cache
    
    // Memória
    size_t total_memory;        // Memória total disponível
    size_t available_memory;    // Memória disponível
    
    // Arquitetura
    bool is_64bit;              // É 64-bit?
    char* architecture;          // Arquitetura (x86_64, ARM, etc)
} HardwareCapabilities;

// =============================================================================
// SIMD BYTECODE REWRITING
// =============================================================================

// Estratégia de otimização SIMD
typedef enum {
    SIMD_STRATEGY_NONE,         // Não usar SIMD
    SIMD_STRATEGY_AUTO,         // Detectar automaticamente
    SIMD_STRATEGY_FORCE,        // Forçar SIMD mesmo se não ideal
    SIMD_STRATEGY_HYBRID        // Híbrido (SIMD + escalar)
} SimdStrategy;

// Resultado da reescrita SIMD
typedef struct {
    bool was_rewritten;         // Foi reescrito?
    SimdCapability simd_used;   // Qual SIMD foi usado
    size_t original_ops;        // Número de operações originais
    size_t simd_ops;            // Número de operações SIMD
    double speedup_estimate;    // Estimativa de speedup
    BytecodeProgram* optimized; // Bytecode otimizado
} SimdRewriteResult;

// =============================================================================
// LOAD PREDICTION (IA)
// =============================================================================

// Modelo de previsão de carga
typedef struct {
    // Dados históricos
    double* historical_loads;    // Cargas históricas
    size_t history_size;        // Tamanho do histórico
    uint64_t* timestamps;       // Timestamps das cargas
    
    // Padrões detectados
    double avg_load;            // Carga média
    double peak_load;           // Carga de pico
    double periodicity;         // Periodicidade (em segundos)
    bool has_pattern;           // Tem padrão detectado?
    
    // Previsão
    double predicted_load;      // Carga prevista
    double confidence;          // Confiança na previsão (0.0 a 1.0)
    uint64_t predicted_time;    // Quando a carga será alta (ns)
} LoadPredictionModel;

// Previsão de evento (WHEN block)
typedef struct {
    const char* when_block_name; // Nome do WHEN block
    double probability;          // Probabilidade de disparo (0.0 a 1.0)
    uint64_t predicted_time_ns; // Quando será disparado (ns)
    double confidence;           // Confiança (0.0 a 1.0)
    LoadPredictionModel* model;  // Modelo usado
} EventPrediction;

// =============================================================================
// INTENT-BASED SCHEDULER
// =============================================================================

// Scheduler baseado em intenção
typedef struct {
    UnifiedGraph* graph;                // Grafo unificado
    Scheduler* scheduler;               // Scheduler tradicional
    HardwareCapabilities* hw_caps;      // Capacidades do hardware
    
    // Intenções
    IntentDeclaration* intents;         // Declarações de intenção
    size_t intent_count;                // Número de intenções
    size_t intent_capacity;             // Capacidade do array
    
    // Previsão de carga
    LoadPredictionModel* load_models;   // Modelos de previsão
    size_t model_count;                 // Número de modelos
    EventPrediction* predictions;       // Previsões ativas
    size_t prediction_count;            // Número de previsões
    size_t prediction_capacity;         // Capacidade do array de previsões
    
    // Cache de otimizações
    SimdRewriteResult* simd_cache;      // Cache de reescritas SIMD
    size_t simd_cache_size;            // Tamanho do cache
    
    // Flags
    bool auto_simd_enabled;             // Auto-SIMD habilitado?
    bool prediction_enabled;            // Previsão habilitada?
    bool proactive_optimization;        // Otimização proativa?
    
    // Estatísticas
    uint64_t simd_rewrites;             // Número de reescritas SIMD
    uint64_t predictions_made;          // Número de previsões
    uint64_t proactive_actions;         // Ações proativas tomadas
    double avg_speedup;                 // Speedup médio obtido
} IntentBasedScheduler;

// =============================================================================
// API PÚBLICA
// =============================================================================

// Inicializa scheduler baseado em intenção
IntentBasedScheduler* intent_scheduler_init(
    UnifiedGraph* graph,
    Scheduler* scheduler
);

// Destrói scheduler
void intent_scheduler_destroy(IntentBasedScheduler* scheduler);

// =============================================================================
// DETECÇÃO DE HARDWARE
// =============================================================================

// Detecta capacidades do hardware
HardwareCapabilities* hardware_detect(void);

// Libera informações de hardware
void hardware_destroy(HardwareCapabilities* hw);

// Verifica se hardware suporta SIMD específico
bool hardware_supports_simd(HardwareCapabilities* hw, SimdCapability simd);

// Obtém melhor estratégia SIMD para hardware
SimdCapability hardware_get_best_simd(HardwareCapabilities* hw);

// =============================================================================
// DECLARAÇÃO DE INTENÇÕES
// =============================================================================

// Declara intenção para um nó do grafo
int intent_scheduler_declare(
    IntentBasedScheduler* scheduler,
    UnifiedNode* node,
    IntentType intent,
    const char* description,
    double priority
);

// Remove declaração de intenção
void intent_scheduler_remove(IntentBasedScheduler* scheduler, UnifiedNode* node);

// Obtém intenção de um nó
IntentDeclaration* intent_scheduler_get_intent(
    IntentBasedScheduler* scheduler,
    UnifiedNode* node
);

// =============================================================================
// REESCRITA SIMD
// =============================================================================

// Reescreve bytecode para usar SIMD
SimdRewriteResult* intent_scheduler_rewrite_simd(
    IntentBasedScheduler* scheduler,
    UnifiedNode* node,
    BytecodeProgram* program,
    SimdStrategy strategy
);

// Verifica se nó pode se beneficiar de SIMD
bool intent_scheduler_can_use_simd(
    IntentBasedScheduler* scheduler,
    UnifiedNode* node
);

// Aplica otimizações SIMD automaticamente
int intent_scheduler_auto_optimize_simd(IntentBasedScheduler* scheduler);

// =============================================================================
// PREVISÃO DE CARGA (IA)
// =============================================================================

// Cria modelo de previsão para um WHEN block
LoadPredictionModel* intent_scheduler_create_prediction_model(
    IntentBasedScheduler* scheduler,
    const char* when_block_name
);

// Treina modelo com dados históricos
int intent_scheduler_train_model(
    LoadPredictionModel* model,
    double* loads,
    uint64_t* timestamps,
    size_t count
);

// Prevê quando WHEN block será disparado
EventPrediction* intent_scheduler_predict_event(
    IntentBasedScheduler* scheduler,
    const char* when_block_name
);

// Processa todas as previsões e toma ações proativas
void intent_scheduler_process_predictions(IntentBasedScheduler* scheduler);

// Pré-aloca recursos baseado em previsão
int intent_scheduler_preallocate(IntentBasedScheduler* scheduler,
                                  EventPrediction* prediction);

// "Aquece" JIT baseado em previsão
int intent_scheduler_warmup_jit(IntentBasedScheduler* scheduler,
                                 EventPrediction* prediction);

// =============================================================================
// SCHEDULING BASEADO EM INTENÇÃO
// =============================================================================

// Agenda execução baseado em intenção
int intent_scheduler_schedule(IntentBasedScheduler* scheduler,
                              UnifiedNode* node);

// Processa todas as intenções e otimiza
void intent_scheduler_optimize_all(IntentBasedScheduler* scheduler);

// =============================================================================
// UTILITÁRIOS
// =============================================================================

// Obtém estatísticas do scheduler
void intent_scheduler_get_stats(IntentBasedScheduler* scheduler,
                                 uint64_t* simd_rewrites,
                                 uint64_t* predictions,
                                 uint64_t* proactive_actions,
                                 double* avg_speedup);

// Imprime informações do scheduler
void intent_scheduler_print_info(IntentBasedScheduler* scheduler);

#ifdef __cplusplus
}
#endif

#endif // INTENT_BASED_H

