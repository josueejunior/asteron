#define _POSIX_C_SOURCE 200809L
#include "snapshot.h"
#include "vm.h"
#include "../interpreter/interpreter.h"
#include "../../utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

/* =============================================================================
 * CRIAÇÃO E DESTRUIÇÃO
 * ============================================================================= */

SnapshotManager* snapshot_manager_create(VM* vm, UnifiedGraph* graph) {
    SnapshotManager* mgr = (SnapshotManager*)calloc(1, sizeof(SnapshotManager));
    if (mgr == NULL) return NULL;
    
    mgr->vm = vm;
    mgr->graph = graph;
    mgr->next_id = 1;
    mgr->max_snapshots = 100;
    mgr->auto_checkpoint = 0;
    mgr->checkpoint_interval = 1000;
    
    return mgr;
}

void snapshot_manager_destroy(SnapshotManager* mgr) {
    if (mgr == NULL) return;
    
    CompleteSnapshot* snap = mgr->snapshots;
    while (snap != NULL) {
        CompleteSnapshot* next = snap->next;
        
        // Libera valores do stack
        if (snap->environment.stack_values) {
            for (size_t i = 0; i < snap->environment.stack_size; i++) {
                value_destroy(snap->environment.stack_values[i]);
            }
            free(snap->environment.stack_values);
        }
        
        // Libera snapshot da VM
        vm_snapshot_destroy(&snap->vm_snapshot);
        
        if (snap->label) {
            free((void*)snap->label);
        }
        
        free(snap);
        snap = next;
    }
    
    free(mgr);
}

/* =============================================================================
 * SNAPSHOTS
 * ============================================================================= */

CompleteSnapshot* snapshot_create(SnapshotManager* mgr, const char* label) {
    if (mgr == NULL || mgr->vm == NULL) return NULL;
    
    double start_time = get_time_us();
    
    CompleteSnapshot* snap = (CompleteSnapshot*)calloc(1, sizeof(CompleteSnapshot));
    if (snap == NULL) return NULL;
    
    snap->snapshot_id = mgr->next_id++;
    snap->timestamp = time(NULL);
    snap->label = label ? strdup(label) : NULL;
    snap->is_checkpoint = 1;
    snap->is_valid = 1;
    
    // Snapshot da VM
    snap->vm_snapshot = vm_take_snapshot(mgr->vm);
    
    // Snapshot do stack
    if (mgr->vm->stack != NULL) {
        snap->environment.stack_size = mgr->vm->stack->size;
        snap->environment.stack_values = (Value*)malloc(
            sizeof(Value) * snap->environment.stack_size);
        if (snap->environment.stack_values != NULL) {
            for (size_t i = 0; i < snap->environment.stack_size; i++) {
                snap->environment.stack_values[i] = mgr->vm->stack->items[i];
                value_retain(snap->environment.stack_values[i]);
            }
        }
    }
    
    // Estado de execução
    snap->execution.pc = mgr->vm->pc;
    snap->execution.is_running = mgr->vm->is_running;
    
    // Snapshot do grafo (cria versão)
    if (mgr->graph != NULL) {
        snap->graph_version = unified_graph_create_version(mgr->graph);
    }
    
    // Adiciona à lista
    if (mgr->snapshots == NULL) {
        mgr->snapshots = snap;
        mgr->current = snap;
    } else {
        snap->prev = mgr->current;
        if (mgr->current != NULL) {
            mgr->current->next = snap;
        }
        mgr->current = snap;
    }
    
    // Limita número de snapshots
    while (mgr->snapshots != NULL) {
        CompleteSnapshot* first = mgr->snapshots;
        size_t count = 0;
        CompleteSnapshot* curr = first;
        while (curr != NULL) {
            count++;
            curr = curr->next;
        }
        
        if (count <= mgr->max_snapshots) break;
        
        // Remove o mais antigo
        mgr->snapshots = first->next;
        if (first->next) {
            first->next->prev = NULL;
        }
        if (mgr->current == first) {
            mgr->current = NULL;
        }
        
        // Libera recursos
        if (first->environment.stack_values) {
            for (size_t i = 0; i < first->environment.stack_size; i++) {
                value_destroy(first->environment.stack_values[i]);
            }
            free(first->environment.stack_values);
        }
        vm_snapshot_destroy(&first->vm_snapshot);
        if (first->label) free((void*)first->label);
        free(first);
    }
    
    double elapsed = get_time_us() - start_time;
    mgr->stats.total_snapshots++;
    mgr->stats.avg_snapshot_time_us = 
        (mgr->stats.avg_snapshot_time_us * (mgr->stats.total_snapshots - 1) + elapsed) / 
        mgr->stats.total_snapshots;
    
    return snap;
}

int snapshot_restore(SnapshotManager* mgr, uint64_t snapshot_id) {
    if (mgr == NULL || mgr->vm == NULL) return 0;
    
    double start_time = get_time_us();
    
    // Encontra snapshot
    CompleteSnapshot* snap = mgr->snapshots;
    while (snap != NULL) {
        if (snap->snapshot_id == snapshot_id && snap->is_valid) {
            break;
        }
        snap = snap->next;
    }
    
    if (snap == NULL) return 0;
    
    // Restaura VM
    vm_restore_snapshot(mgr->vm, snap->vm_snapshot);
    
    // Restaura stack
    if (snap->environment.stack_values != NULL && mgr->vm->stack != NULL) {
        // Limpa stack atual
        while (mgr->vm->stack->size > 0) {
            Value v = stack_pop(mgr->vm->stack);
            value_destroy(v);
        }
        
        // Restaura valores
        for (size_t i = 0; i < snap->environment.stack_size; i++) {
            stack_push(mgr->vm->stack, snap->environment.stack_values[i]);
            value_retain(snap->environment.stack_values[i]);
        }
    }
    
    // Restaura estado de execução
    mgr->vm->pc = snap->execution.pc;
    mgr->vm->is_running = snap->execution.is_running;
    
    // Restaura grafo
    if (snap->graph_version != NULL && mgr->graph != NULL) {
        unified_graph_restore_version(mgr->graph, snap->graph_version->version_id);
    }
    
    mgr->current = snap;
    
    double elapsed = get_time_us() - start_time;
    mgr->stats.total_restores++;
    mgr->stats.avg_restore_time_us = 
        (mgr->stats.avg_restore_time_us * (mgr->stats.total_restores - 1) + elapsed) / 
        mgr->stats.total_restores;
    
    return 1;
}

int snapshot_rollback(SnapshotManager* mgr) {
    if (mgr == NULL || mgr->current == NULL) return 0;
    
    CompleteSnapshot* prev = mgr->current->prev;
    if (prev == NULL) return 0;
    
    mgr->stats.total_rollbacks++;
    return snapshot_restore(mgr, prev->snapshot_id);
}

void snapshot_destroy(SnapshotManager* mgr, uint64_t snapshot_id) {
    if (mgr == NULL) return;
    
    CompleteSnapshot* snap = mgr->snapshots;
    while (snap != NULL) {
        if (snap->snapshot_id == snapshot_id) {
            // Remove da lista
            if (snap->prev) {
                snap->prev->next = snap->next;
            } else {
                mgr->snapshots = snap->next;
            }
            if (snap->next) {
                snap->next->prev = snap->prev;
            }
            if (mgr->current == snap) {
                mgr->current = snap->prev;
            }
            
            // Libera recursos
            if (snap->environment.stack_values) {
                for (size_t i = 0; i < snap->environment.stack_size; i++) {
                    value_destroy(snap->environment.stack_values[i]);
                }
                free(snap->environment.stack_values);
            }
            vm_snapshot_destroy(&snap->vm_snapshot);
            if (snap->label) free((void*)snap->label);
            free(snap);
            return;
        }
        snap = snap->next;
    }
}

void snapshot_list(SnapshotManager* mgr) {
    if (mgr == NULL) return;
    
    printf("\n=== Snapshots Disponíveis ===\n");
    CompleteSnapshot* snap = mgr->snapshots;
    while (snap != NULL) {
        char time_str[64];
        struct tm* tm_info = localtime(&snap->timestamp);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        
        printf("ID: %llu | %s | %s %s\n",
               (unsigned long long)snap->snapshot_id,
               time_str,
               snap->label ? snap->label : "(sem label)",
               (snap == mgr->current) ? "[ATUAL]" : "");
        snap = snap->next;
    }
}

/* =============================================================================
 * CHECKPOINTS AUTOMÁTICOS
 * ============================================================================= */

void snapshot_enable_auto_checkpoint(SnapshotManager* mgr, size_t interval) {
    if (mgr == NULL) return;
    mgr->auto_checkpoint = 1;
    mgr->checkpoint_interval = interval;
    mgr->instruction_count = 0;
}

void snapshot_disable_auto_checkpoint(SnapshotManager* mgr) {
    if (mgr == NULL) return;
    mgr->auto_checkpoint = 0;
}

CompleteSnapshot* snapshot_checkpoint(SnapshotManager* mgr) {
    return snapshot_create(mgr, "checkpoint");
}

void snapshot_update_instruction_count(SnapshotManager* mgr) {
    if (mgr == NULL || !mgr->auto_checkpoint) return;
    
    mgr->instruction_count++;
    if (mgr->instruction_count >= mgr->checkpoint_interval) {
        snapshot_checkpoint(mgr);
        mgr->instruction_count = 0;
    }
}

/* =============================================================================
 * SERIALIZAÇÃO
 * ============================================================================= */

int snapshot_serialize(SnapshotManager* mgr, uint64_t snapshot_id, 
                      const char* filename) {
    // Implementação simplificada
    printf("Serializando snapshot %llu para %s...\n", 
           (unsigned long long)snapshot_id, filename);
    return 1;
}

CompleteSnapshot* snapshot_deserialize(SnapshotManager* mgr, const char* filename) {
    // Implementação simplificada
    printf("Deserializando snapshot de %s...\n", filename);
    return NULL;
}

/* =============================================================================
 * ESTATÍSTICAS
 * ============================================================================= */

void snapshot_get_stats(SnapshotManager* mgr, size_t* total, size_t* restores,
                        double* avg_snapshot_time, double* avg_restore_time) {
    if (mgr == NULL) return;
    
    if (total) *total = mgr->stats.total_snapshots;
    if (restores) *restores = mgr->stats.total_restores;
    if (avg_snapshot_time) *avg_snapshot_time = mgr->stats.avg_snapshot_time_us;
    if (avg_restore_time) *avg_restore_time = mgr->stats.avg_restore_time_us;
}

