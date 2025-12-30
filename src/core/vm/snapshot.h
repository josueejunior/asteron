#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include "vm.h"
#include "../../graph/unified_graph.h"
#include <stddef.h>
#include <stdint.h>

/* =============================================================================
 * SNAPSHOT COMPLETO DE ESTADO
 * =============================================================================
 * Expande o sistema de snapshot para incluir estado completo da VM,
 * grafos, e permite rollback instantâneo.
 * ============================================================================= */

// Snapshot completo do sistema
typedef struct CompleteSnapshot {
    uint64_t snapshot_id;          // ID único do snapshot
    time_t timestamp;              // Quando foi criado
    
    // Snapshot da VM
    VMSnapshot vm_snapshot;
    
    // Snapshot do grafo unificado
    GraphVersion* graph_version;    // Versão do grafo neste snapshot
    
    // Estado do ambiente
    struct {
        size_t stack_size;          // Tamanho do stack
        Value* stack_values;        // Valores do stack (cópia)
        size_t env_size;            // Tamanho do environment
        // (Simplificado - em produção, serializaria environment completo)
    } environment;
    
    // Estado de execução
    struct {
        size_t pc;                  // Program counter
        int is_running;             // Flag de execução
        size_t completed_tasks;     // Tasks completadas
    } execution;
    
    // Metadados
    const char* label;              // Label opcional do snapshot
    int is_checkpoint;              // É um checkpoint (pode restaurar)?
    int is_valid;                   // Snapshot é válido?
    
    // Lista encadeada
    struct CompleteSnapshot* next;
    struct CompleteSnapshot* prev;
} CompleteSnapshot;

// Gerenciador de snapshots
typedef struct SnapshotManager {
    CompleteSnapshot* snapshots;    // Lista de snapshots
    CompleteSnapshot* current;      // Snapshot atual
    uint64_t next_id;               // Próximo ID
    
    UnifiedGraph* graph;            // Grafo unificado (referência)
    VM* vm;                         // VM (referência)
    
    // Configuração
    int auto_checkpoint;            // Cria checkpoints automaticamente?
    size_t max_snapshots;           // Máximo de snapshots mantidos
    size_t checkpoint_interval;     // Intervalo entre checkpoints (em instruções)
    size_t instruction_count;       // Contador de instruções desde último checkpoint
    
    // Estatísticas
    struct {
        size_t total_snapshots;
        size_t total_restores;
        size_t total_rollbacks;
        double avg_snapshot_time_us;
        double avg_restore_time_us;
    } stats;
} SnapshotManager;

/* =============================================================================
 * API - CRIAÇÃO E DESTRUIÇÃO
 * ============================================================================= */

/**
 * Cria gerenciador de snapshots
 */
SnapshotManager* snapshot_manager_create(VM* vm, UnifiedGraph* graph);

/**
 * Destrói gerenciador de snapshots
 */
void snapshot_manager_destroy(SnapshotManager* mgr);

/* =============================================================================
 * API - SNAPSHOTS
 * ============================================================================= */

/**
 * Cria snapshot completo do sistema
 */
CompleteSnapshot* snapshot_create(SnapshotManager* mgr, const char* label);

/**
 * Restaura snapshot específico
 */
int snapshot_restore(SnapshotManager* mgr, uint64_t snapshot_id);

/**
 * Restaura snapshot anterior (rollback)
 */
int snapshot_rollback(SnapshotManager* mgr);

/**
 * Destrói snapshot específico
 */
void snapshot_destroy(SnapshotManager* mgr, uint64_t snapshot_id);

/**
 * Lista todos os snapshots disponíveis
 */
void snapshot_list(SnapshotManager* mgr);

/* =============================================================================
 * API - CHECKPOINTS AUTOMÁTICOS
 * ============================================================================= */

/**
 * Habilita checkpoints automáticos
 */
void snapshot_enable_auto_checkpoint(SnapshotManager* mgr, size_t interval);

/**
 * Desabilita checkpoints automáticos
 */
void snapshot_disable_auto_checkpoint(SnapshotManager* mgr);

/**
 * Cria checkpoint manual
 */
CompleteSnapshot* snapshot_checkpoint(SnapshotManager* mgr);

/**
 * Atualiza contador de instruções (chamado durante execução)
 */
void snapshot_update_instruction_count(SnapshotManager* mgr);

/* =============================================================================
 * API - SERIALIZAÇÃO
 * ============================================================================= */

/**
 * Serializa snapshot para arquivo
 */
int snapshot_serialize(SnapshotManager* mgr, uint64_t snapshot_id, 
                      const char* filename);

/**
 * Deserializa snapshot de arquivo
 */
CompleteSnapshot* snapshot_deserialize(SnapshotManager* mgr, const char* filename);

/* =============================================================================
 * API - ESTATÍSTICAS
 * ============================================================================= */

/**
 * Obtém estatísticas do gerenciador
 */
void snapshot_get_stats(SnapshotManager* mgr, size_t* total, size_t* restores,
                        double* avg_snapshot_time, double* avg_restore_time);

#endif // SNAPSHOT_H

