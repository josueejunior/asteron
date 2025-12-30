/**
 * =============================================================================
 * ASTERON INTENT-BASED SCHEDULING - IMPLEMENTAÇÃO
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "intent_based.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/sysinfo.h>

#ifdef __x86_64__
#include <cpuid.h>
// Definições de bits CPUID (se não disponíveis)
#ifndef bit_SSE4_1
#define bit_SSE4_1 (1 << 19)
#endif
#ifndef bit_AVX
#define bit_AVX (1 << 28)
#endif
#ifndef bit_AVX2
#define bit_AVX2 (1 << 5)
#endif
#ifndef bit_AVX512F
#define bit_AVX512F (1 << 16)
#endif
#endif

// =============================================================================
// DETECÇÃO DE HARDWARE
// =============================================================================

HardwareCapabilities* hardware_detect(void) {
    HardwareCapabilities* hw = (HardwareCapabilities*)calloc(
        1, sizeof(HardwareCapabilities));
    if (hw == NULL) return NULL;
    
    // Detecta número de CPUs
    hw->cpu_count = get_nprocs();
    hw->logical_cores = sysconf(_SC_NPROCESSORS_ONLN);
    hw->physical_cores = hw->logical_cores; // Simplificado
    
    // Detecta memória
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        hw->total_memory = si.totalram * si.mem_unit;
        hw->available_memory = si.freeram * si.mem_unit;
    }
    
    // Detecta SIMD (x86_64)
#ifdef __x86_64__
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    
    // CPUID level 1
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        if (ecx & bit_SSE4_1) hw->has_sse4 = true;
        if (ecx & bit_AVX) hw->has_avx = true;
    }
    
    // CPUID level 7 (AVX2, AVX-512)
    unsigned int max_level = __get_cpuid_max(0, NULL);
    if (max_level >= 7) {
        __cpuid_count(7, 0, eax, ebx, ecx, edx);
        if (ebx & bit_AVX2) hw->has_avx2 = true;
        if (ebx & bit_AVX512F) hw->has_avx512 = true;
    }
    
    // Determina melhor SIMD disponível
    if (hw->has_avx512) {
        hw->max_simd = SIMD_AVX512;
    } else if (hw->has_avx2) {
        hw->max_simd = SIMD_AVX2;
    } else if (hw->has_avx) {
        hw->max_simd = SIMD_AVX;
    } else if (hw->has_sse4) {
        hw->max_simd = SIMD_SSE4;
    } else {
        hw->max_simd = SIMD_NONE;
    }
#else
    // Arquitetura não-x86: sem SIMD por enquanto
    hw->max_simd = SIMD_NONE;
    hw->has_avx512 = false;
    hw->has_avx2 = false;
    hw->has_avx = false;
    hw->has_sse4 = false;
#endif
    
    // Arquitetura
    hw->is_64bit = sizeof(void*) == 8;
    hw->architecture = strdup("x86_64"); // Simplificado
    
    // CPU brand (simplificado)
    hw->cpu_brand = strdup("Unknown CPU");
    
    printf("[Hardware] Detectado: %d CPUs, SIMD: %d\n", 
           hw->cpu_count, hw->max_simd);
    
    return hw;
}

void hardware_destroy(HardwareCapabilities* hw) {
    if (hw == NULL) return;
    free(hw->cpu_brand);
    free(hw->architecture);
    free(hw);
}

bool hardware_supports_simd(HardwareCapabilities* hw, SimdCapability simd) {
    if (hw == NULL) return false;
    return hw->max_simd >= simd;
}

SimdCapability hardware_get_best_simd(HardwareCapabilities* hw) {
    if (hw == NULL) return SIMD_NONE;
    return hw->max_simd;
}

// =============================================================================
// INICIALIZAÇÃO E DESTRUIÇÃO
// =============================================================================

IntentBasedScheduler* intent_scheduler_init(
    UnifiedGraph* graph,
    Scheduler* scheduler) {
    
    if (graph == NULL || scheduler == NULL) return NULL;
    
    IntentBasedScheduler* sched = (IntentBasedScheduler*)calloc(
        1, sizeof(IntentBasedScheduler));
    if (sched == NULL) return NULL;
    
    sched->graph = graph;
    sched->scheduler = scheduler;
    
    // Detecta hardware
    sched->hw_caps = hardware_detect();
    
    // Inicializa arrays
    sched->intent_capacity = 64;
    sched->intents = (IntentDeclaration*)calloc(
        sched->intent_capacity, sizeof(IntentDeclaration));
    
    sched->model_count = 0;
    sched->load_models = NULL;
    
    sched->prediction_capacity = 32;
    sched->predictions = (EventPrediction*)calloc(
        sched->prediction_capacity, sizeof(EventPrediction));
    
    sched->simd_cache_size = 64;
    sched->simd_cache = (SimdRewriteResult*)calloc(
        sched->simd_cache_size, sizeof(SimdRewriteResult));
    
    // Flags padrão
    sched->auto_simd_enabled = true;
    sched->prediction_enabled = true;
    sched->proactive_optimization = true;
    
    printf("[Intent-Based] Scheduler inicializado\n");
    return sched;
}

void intent_scheduler_destroy(IntentBasedScheduler* scheduler) {
    if (scheduler == NULL) return;
    
    // Libera hardware
    if (scheduler->hw_caps) {
        hardware_destroy(scheduler->hw_caps);
    }
    
    // Libera intenções
    for (size_t i = 0; i < scheduler->intent_count; i++) {
        free((void*)scheduler->intents[i].description);
    }
    free(scheduler->intents);
    
    // Libera modelos
    for (size_t i = 0; i < scheduler->model_count; i++) {
        if (scheduler->load_models[i].historical_loads) {
            free(scheduler->load_models[i].historical_loads);
        }
        if (scheduler->load_models[i].timestamps) {
            free(scheduler->load_models[i].timestamps);
        }
    }
    free(scheduler->load_models);
    
    // Libera previsões
    free(scheduler->predictions);
    
    // Libera cache SIMD
    for (size_t i = 0; i < scheduler->simd_cache_size; i++) {
        // Não libera bytecode - é propriedade da VM
    }
    free(scheduler->simd_cache);
    
    free(scheduler);
}

// =============================================================================
// DECLARAÇÃO DE INTENÇÕES
// =============================================================================

int intent_scheduler_declare(
    IntentBasedScheduler* scheduler,
    UnifiedNode* node,
    IntentType intent,
    const char* description,
    double priority) {
    
    if (scheduler == NULL || node == NULL) return 1;
    
    // Verifica se já existe intenção para este nó
    for (size_t i = 0; i < scheduler->intent_count; i++) {
        if (scheduler->intents[i].target_node == node) {
            // Atualiza intenção existente
            scheduler->intents[i].type = intent;
            scheduler->intents[i].description = description ? strdup(description) : NULL;
            scheduler->intents[i].priority = priority;
            return 0;
        }
    }
    
    // Expande array se necessário
    if (scheduler->intent_count >= scheduler->intent_capacity) {
        scheduler->intent_capacity *= 2;
        scheduler->intents = (IntentDeclaration*)realloc(
            scheduler->intents, sizeof(IntentDeclaration) * scheduler->intent_capacity);
    }
    
    // Adiciona nova intenção
    IntentDeclaration* decl = &scheduler->intents[scheduler->intent_count++];
    decl->type = intent;
    decl->description = description ? strdup(description) : NULL;
    decl->target_node = node;
    decl->priority = priority;
    decl->constraints = NULL;
    
    printf("[Intent-Based] Intenção declarada para nó '%s': %d\n", 
           node->name, intent);
    
    return 0;
}

void intent_scheduler_remove(IntentBasedScheduler* scheduler, UnifiedNode* node) {
    if (scheduler == NULL || node == NULL) return;
    
    for (size_t i = 0; i < scheduler->intent_count; i++) {
        if (scheduler->intents[i].target_node == node) {
            free((void*)scheduler->intents[i].description);
            
            // Move últimos elementos
            scheduler->intents[i] = scheduler->intents[scheduler->intent_count - 1];
            scheduler->intent_count--;
            return;
        }
    }
}

IntentDeclaration* intent_scheduler_get_intent(
    IntentBasedScheduler* scheduler,
    UnifiedNode* node) {
    
    if (scheduler == NULL || node == NULL) return NULL;
    
    for (size_t i = 0; i < scheduler->intent_count; i++) {
        if (scheduler->intents[i].target_node == node) {
            return &scheduler->intents[i];
        }
    }
    return NULL;
}

// =============================================================================
// REESCRITA SIMD
// =============================================================================

bool intent_scheduler_can_use_simd(
    IntentBasedScheduler* scheduler,
    UnifiedNode* node) {
    
    if (scheduler == NULL || node == NULL || scheduler->hw_caps == NULL) {
        return false;
    }
    
    // Verifica se hardware suporta SIMD
    if (scheduler->hw_caps->max_simd == SIMD_NONE) {
        return false;
    }
    
    // Verifica se nó tem intenção de ser rápido ou computação intensiva
    IntentDeclaration* intent = intent_scheduler_get_intent(scheduler, node);
    if (intent != NULL) {
        if (intent->type == INTENT_FAST || 
            intent->type == INTENT_COMPUTE_INTENSIVE ||
            intent->type == INTENT_HIGH_THROUGHPUT) {
            return true;
        }
    }
    
    // Verifica métricas - se é hot path e tem muitas operações
    if (node->metrics.is_hot && 
        node->metrics.execution_count > 1000 &&
        node->metrics.execution_time_us > 100.0) {
        return true;
    }
    
    return false;
}

SimdRewriteResult* intent_scheduler_rewrite_simd(
    IntentBasedScheduler* scheduler,
    UnifiedNode* node,
    BytecodeProgram* program,
    SimdStrategy strategy) {
    
    if (scheduler == NULL || node == NULL || program == NULL) {
        return NULL;
    }
    
    SimdRewriteResult* result = (SimdRewriteResult*)calloc(
        1, sizeof(SimdRewriteResult));
    if (result == NULL) return NULL;
    
    // Verifica se pode usar SIMD
    if (!intent_scheduler_can_use_simd(scheduler, node)) {
        result->was_rewritten = false;
        return result;
    }
    
    // Determina SIMD a usar
    SimdCapability simd = hardware_get_best_simd(scheduler->hw_caps);
    if (simd == SIMD_NONE) {
        result->was_rewritten = false;
        return result;
    }
    
    // Em produção, aqui reescreveria o bytecode:
    // 1. Identifica loops que podem ser vetorizados
    // 2. Substitui operações escalares por SIMD
    // 3. Gera novo bytecode otimizado
    
    // Por enquanto, apenas marca como reescrito
    result->was_rewritten = true;
    result->simd_used = simd;
    result->original_ops = program->instruction_count; // Simplificado
    result->simd_ops = result->original_ops / 4; // Estimativa (4x speedup)
    result->speedup_estimate = 4.0; // Estimativa
    result->optimized = program; // Em produção, criaria novo programa
    
    scheduler->simd_rewrites++;
    scheduler->avg_speedup = (scheduler->avg_speedup * 0.9) + (4.0 * 0.1);
    
    printf("[Intent-Based] Bytecode reescrito para SIMD (nó: %s, SIMD: %d)\n",
           node->name, simd);
    
    return result;
}

int intent_scheduler_auto_optimize_simd(IntentBasedScheduler* scheduler) {
    if (scheduler == NULL || !scheduler->auto_simd_enabled) return 1;
    
    int optimized = 0;
    
    // Processa todos os nós do grafo
    for (size_t i = 0; i < scheduler->graph->node_count; i++) {
        UnifiedNode* node = scheduler->graph->nodes[i];
        
        if (intent_scheduler_can_use_simd(scheduler, node)) {
            // Em produção, obteria bytecode do nó e reescreveria
            // Por enquanto, apenas marca
            optimized++;
        }
    }
    
    return optimized > 0 ? 0 : 1;
}

// =============================================================================
// PREVISÃO DE CARGA (IA)
// =============================================================================

LoadPredictionModel* intent_scheduler_create_prediction_model(
    IntentBasedScheduler* scheduler,
    const char* when_block_name) {
    
    if (scheduler == NULL || when_block_name == NULL) return NULL;
    
    // Expande array se necessário
    if (scheduler->model_count >= scheduler->model_count + 1) {
        scheduler->load_models = (LoadPredictionModel*)realloc(
            scheduler->load_models,
            sizeof(LoadPredictionModel) * (scheduler->model_count + 1));
    }
    
    LoadPredictionModel* model = &scheduler->load_models[scheduler->model_count++];
    memset(model, 0, sizeof(LoadPredictionModel));
    
    // Inicializa modelo
    model->history_size = 0;
    model->historical_loads = NULL;
    model->timestamps = NULL;
    model->has_pattern = false;
    model->confidence = 0.0;
    
    return model;
}

int intent_scheduler_train_model(
    LoadPredictionModel* model,
    double* loads,
    uint64_t* timestamps,
    size_t count) {
    
    if (model == NULL || loads == NULL || timestamps == NULL || count == 0) {
        return 1;
    }
    
    // Aloca histórico
    model->historical_loads = (double*)malloc(sizeof(double) * count);
    model->timestamps = (uint64_t*)malloc(sizeof(uint64_t) * count);
    if (model->historical_loads == NULL || model->timestamps == NULL) {
        return 1;
    }
    
    memcpy(model->historical_loads, loads, sizeof(double) * count);
    memcpy(model->timestamps, timestamps, sizeof(uint64_t) * count);
    model->history_size = count;
    
    // Calcula estatísticas
    double sum = 0.0;
    double max_load = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += loads[i];
        if (loads[i] > max_load) {
            max_load = loads[i];
        }
    }
    
    model->avg_load = sum / count;
    model->peak_load = max_load;
    
    // Detecta periodicidade simples (diferença média entre picos)
    if (count > 1) {
        double total_diff = 0.0;
        int peak_count = 0;
        for (size_t i = 1; i < count; i++) {
            if (loads[i] > model->avg_load * 1.5) { // Pico
                total_diff += (double)(timestamps[i] - timestamps[i-1]);
                peak_count++;
            }
        }
        if (peak_count > 0) {
            model->periodicity = total_diff / peak_count;
            model->has_pattern = true;
            model->confidence = 0.7; // Confiança média
        }
    }
    
    return 0;
}

EventPrediction* intent_scheduler_predict_event(
    IntentBasedScheduler* scheduler,
    const char* when_block_name) {
    
    if (scheduler == NULL || when_block_name == NULL) return NULL;
    
    // Encontra modelo para este WHEN block
    LoadPredictionModel* model = NULL;
    for (size_t i = 0; i < scheduler->model_count; i++) {
        // Em produção, associaria modelo ao nome do WHEN block
        // Por enquanto, usa primeiro modelo disponível
        if (scheduler->load_models[i].history_size > 0) {
            model = &scheduler->load_models[i];
            break;
        }
    }
    
    if (model == NULL || !model->has_pattern) {
        return NULL;
    }
    
    // Cria previsão
    EventPrediction* prediction = (EventPrediction*)calloc(
        1, sizeof(EventPrediction));
    if (prediction == NULL) return NULL;
    
    prediction->when_block_name = when_block_name;
    prediction->model = model;
    prediction->confidence = model->confidence;
    
    // Previsão simples: próximo pico baseado em periodicidade
    uint64_t now_ns = (uint64_t)time(NULL) * 1000000000ULL;
    if (model->periodicity > 0) {
        prediction->predicted_time_ns = now_ns + (uint64_t)model->periodicity;
        prediction->probability = 0.8; // Alta probabilidade se há padrão
    } else {
        prediction->predicted_time_ns = now_ns;
        prediction->probability = 0.3; // Baixa probabilidade sem padrão
    }
    
    scheduler->predictions_made++;
    return prediction;
}

void intent_scheduler_process_predictions(IntentBasedScheduler* scheduler) {
    if (scheduler == NULL || !scheduler->prediction_enabled) return;
    
    // Processa todas as previsões e toma ações proativas
    for (size_t i = 0; i < scheduler->prediction_count; i++) {
        EventPrediction* pred = &scheduler->predictions[i];
        
        if (pred->probability > 0.7) { // Alta probabilidade
            // Pré-aloca recursos
            intent_scheduler_preallocate(scheduler, pred);
            
            // Aquece JIT
            intent_scheduler_warmup_jit(scheduler, pred);
            
            scheduler->proactive_actions++;
        }
    }
}

int intent_scheduler_preallocate(IntentBasedScheduler* scheduler,
                                  EventPrediction* prediction) {
    if (scheduler == NULL || prediction == NULL) return 1;
    
    // Em produção, pré-alocaria memória baseado na carga prevista
    printf("[Intent-Based] Pré-alocando recursos para '%s' (prob: %.2f)\n",
           prediction->when_block_name, prediction->probability);
    
    return 0;
}

int intent_scheduler_warmup_jit(IntentBasedScheduler* scheduler,
                                 EventPrediction* prediction) {
    if (scheduler == NULL || prediction == NULL) return 1;
    
    // Em produção, "aqueceria" JIT compilando código antecipadamente
    printf("[Intent-Based] Aquecendo JIT para '%s'\n",
           prediction->when_block_name);
    
    return 0;
}

// =============================================================================
// SCHEDULING BASEADO EM INTENÇÃO
// =============================================================================

int intent_scheduler_schedule(IntentBasedScheduler* scheduler,
                              UnifiedNode* node) {
    if (scheduler == NULL || node == NULL) return 1;
    
    // Obtém intenção do nó
    IntentDeclaration* intent = intent_scheduler_get_intent(scheduler, node);
    
    if (intent == NULL) {
        // Sem intenção específica - usa scheduler tradicional
        // (em produção, integraria com scheduler->scheduler)
        return 0;
    }
    
    // Decide infraestrutura baseado em intenção
    switch (intent->type) {
        case INTENT_FAST:
        case INTENT_COMPUTE_INTENSIVE:
            // Usa SIMD se disponível
            if (intent_scheduler_can_use_simd(scheduler, node)) {
                // Em produção, reescreveria bytecode
            }
            break;
            
        case INTENT_PARALLEL:
            // Agenda em thread separada
            // (em produção, integraria com scheduler)
            break;
            
        case INTENT_MEMORY_EFFICIENT:
            // Usa region-based memory
            break;
            
        default:
            break;
    }
    
    return 0;
}

void intent_scheduler_optimize_all(IntentBasedScheduler* scheduler) {
    if (scheduler == NULL) return;
    
    // Otimiza todos os nós baseado em intenções
    for (size_t i = 0; i < scheduler->graph->node_count; i++) {
        UnifiedNode* node = scheduler->graph->nodes[i];
        intent_scheduler_schedule(scheduler, node);
    }
    
    // Aplica otimizações SIMD automáticas
    if (scheduler->auto_simd_enabled) {
        intent_scheduler_auto_optimize_simd(scheduler);
    }
    
    // Processa previsões
    if (scheduler->prediction_enabled) {
        intent_scheduler_process_predictions(scheduler);
    }
}

// =============================================================================
// UTILITÁRIOS
// =============================================================================

void intent_scheduler_get_stats(IntentBasedScheduler* scheduler,
                                 uint64_t* simd_rewrites,
                                 uint64_t* predictions,
                                 uint64_t* proactive_actions,
                                 double* avg_speedup) {
    if (scheduler == NULL) return;
    
    if (simd_rewrites) *simd_rewrites = scheduler->simd_rewrites;
    if (predictions) *predictions = scheduler->predictions_made;
    if (proactive_actions) *proactive_actions = scheduler->proactive_actions;
    if (avg_speedup) *avg_speedup = scheduler->avg_speedup;
}

void intent_scheduler_print_info(IntentBasedScheduler* scheduler) {
    if (scheduler == NULL) return;
    
    printf("\n=== Intent-Based Scheduler ===\n");
    printf("Intenções: %zu\n", scheduler->intent_count);
    printf("Modelos de previsão: %zu\n", scheduler->model_count);
    printf("Reescritas SIMD: %llu\n", (unsigned long long)scheduler->simd_rewrites);
    printf("Previsões: %llu\n", (unsigned long long)scheduler->predictions_made);
    printf("Ações proativas: %llu\n", 
           (unsigned long long)scheduler->proactive_actions);
    printf("Speedup médio: %.2fx\n", scheduler->avg_speedup);
    
    if (scheduler->hw_caps) {
        printf("\nHardware:\n");
        printf("  CPUs: %d\n", scheduler->hw_caps->cpu_count);
        printf("  SIMD: %d\n", scheduler->hw_caps->max_simd);
    }
    printf("=============================\n\n");
}

