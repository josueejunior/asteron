#define _POSIX_C_SOURCE 200809L
#include "hotreload.h"
#include "../../utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* =============================================================================
 * CRIAÇÃO E DESTRUIÇÃO
 * ============================================================================= */

HotReloadManager* hotreload_manager_create(VM* vm, UnifiedGraph* graph) {
    HotReloadManager* mgr = (HotReloadManager*)calloc(1, sizeof(HotReloadManager));
    if (mgr == NULL) return NULL;
    
    mgr->vm = vm;
    mgr->graph = graph;
    
    mgr->change_capacity = 32;
    mgr->changes = (ReloadChange*)calloc(
        mgr->change_capacity, sizeof(ReloadChange));
    
    mgr->handler_capacity = 8;
    mgr->handlers = (ReloadHandler*)calloc(
        mgr->handler_capacity, sizeof(ReloadHandler));
    
    mgr->enabled = 1;
    mgr->auto_apply = 0;
    mgr->watch_files = 1;
    mgr->watch_graph = 1;
    
    return mgr;
}

void hotreload_manager_destroy(HotReloadManager* mgr) {
    if (mgr == NULL) return;
    
    // Libera mudanças
    for (size_t i = 0; i < mgr->change_count; i++) {
        if (mgr->changes[i].target) {
            free((void*)mgr->changes[i].target);
        }
    }
    free(mgr->changes);
    
    // Libera handlers
    for (size_t i = 0; i < mgr->handler_count; i++) {
        if (mgr->handlers[i].name) {
            free((void*)mgr->handlers[i].name);
        }
    }
    free(mgr->handlers);
    
    free(mgr);
}

/* =============================================================================
 * MONITORAMENTO
 * ============================================================================= */

void hotreload_enable(HotReloadManager* mgr) {
    if (mgr == NULL) return;
    mgr->enabled = 1;
}

void hotreload_disable(HotReloadManager* mgr) {
    if (mgr == NULL) return;
    mgr->enabled = 0;
}

void hotreload_check_changes(HotReloadManager* mgr) {
    if (mgr == NULL || !mgr->enabled) return;
    
    // Verifica mudanças no grafo
    if (mgr->watch_graph && mgr->graph != NULL) {
        // Compara versão atual com anterior
        // (Simplificado - em produção, compararia estruturas completas)
    }
    
    // Verifica mudanças em arquivos
    if (mgr->watch_files) {
        // (Simplificado - em produção, usaria inotify ou similar)
    }
}

int hotreload_apply_change(HotReloadManager* mgr, ReloadChange* change) {
    if (mgr == NULL || change == NULL || !mgr->enabled) return 0;
    
    // Encontra handler apropriado
    ReloadHandler* handler = NULL;
    for (size_t i = 0; i < mgr->handler_count; i++) {
        if (mgr->handlers[i].can_reload != NULL && 
            mgr->handlers[i].can_reload(change)) {
            handler = &mgr->handlers[i];
            break;
        }
    }
    
    if (handler == NULL || handler->apply_change == NULL) {
        if (mgr->on_reload_failure) {
            mgr->on_reload_failure(change->target, "No handler available");
        }
        mgr->stats.failed_reloads++;
        return 0;
    }
    
    // Aplica mudança
    int result = handler->apply_change(change, mgr->vm, mgr->graph);
    
    if (result) {
        if (handler->on_success) {
            handler->on_success(change);
        }
        if (mgr->on_reload_success) {
            mgr->on_reload_success(change->target);
        }
        mgr->stats.successful_reloads++;
    } else {
        const char* error = "Unknown error";
        if (handler->on_failure) {
            handler->on_failure(change, error);
        }
        if (mgr->on_reload_failure) {
            mgr->on_reload_failure(change->target, error);
        }
        mgr->stats.failed_reloads++;
    }
    
    return result;
}

int hotreload_apply_all(HotReloadManager* mgr) {
    if (mgr == NULL || !mgr->enabled) return 0;
    
    int success_count = 0;
    for (size_t i = 0; i < mgr->change_count; i++) {
        if (hotreload_apply_change(mgr, &mgr->changes[i])) {
            success_count++;
        }
    }
    
    return success_count;
}

/* =============================================================================
 * HANDLERS
 * ============================================================================= */

void hotreload_register_handler(HotReloadManager* mgr, ReloadHandler* handler) {
    if (mgr == NULL || handler == NULL) return;
    
    if (mgr->handler_count >= mgr->handler_capacity) {
        mgr->handler_capacity *= 2;
        ReloadHandler* new_handlers = (ReloadHandler*)realloc(
            mgr->handlers, sizeof(ReloadHandler) * mgr->handler_capacity);
        if (new_handlers == NULL) return;
        mgr->handlers = new_handlers;
    }
    
    mgr->handlers[mgr->handler_count++] = *handler;
}

void hotreload_unregister_handler(HotReloadManager* mgr, const char* name) {
    if (mgr == NULL || name == NULL) return;
    
    for (size_t i = 0; i < mgr->handler_count; i++) {
        if (mgr->handlers[i].name && strcmp(mgr->handlers[i].name, name) == 0) {
            // Remove handler
            for (size_t j = i; j < mgr->handler_count - 1; j++) {
                mgr->handlers[j] = mgr->handlers[j + 1];
            }
            mgr->handler_count--;
            break;
        }
    }
}

/* =============================================================================
 * MUDANÇAS
 * ============================================================================= */

void hotreload_record_change(HotReloadManager* mgr, ReloadChangeType type, 
                             const char* target, void* old_data, void* new_data) {
    if (mgr == NULL || !mgr->enabled) return;
    
    if (mgr->change_count >= mgr->change_capacity) {
        mgr->change_capacity *= 2;
        ReloadChange* new_changes = (ReloadChange*)realloc(
            mgr->changes, sizeof(ReloadChange) * mgr->change_capacity);
        if (new_changes == NULL) return;
        mgr->changes = new_changes;
    }
    
    ReloadChange* change = &mgr->changes[mgr->change_count++];
    change->type = type;
    change->target = target ? strdup(target) : NULL;
    change->timestamp = time(NULL);
    change->old_data = old_data;
    change->new_data = new_data;
    change->requires_restart = (type == RELOAD_CHANGE_TYPE || 
                                type == RELOAD_CHANGE_CONFIG);
    change->is_critical = (type == RELOAD_CHANGE_TYPE);
    
    mgr->stats.total_changes++;
    
    if (mgr->on_change_detected) {
        mgr->on_change_detected(change);
    }
    
    // Aplica automaticamente se configurado
    if (mgr->auto_apply) {
        hotreload_apply_change(mgr, change);
    }
}

void hotreload_list_changes(HotReloadManager* mgr) {
    if (mgr == NULL) return;
    
    printf("\n=== Mudanças Pendentes ===\n");
    for (size_t i = 0; i < mgr->change_count; i++) {
        ReloadChange* change = &mgr->changes[i];
        const char* type_str = "unknown";
        switch (change->type) {
            case RELOAD_CHANGE_CODE: type_str = "code"; break;
            case RELOAD_CHANGE_GRAPH: type_str = "graph"; break;
            case RELOAD_CHANGE_DEPENDENCY: type_str = "dependency"; break;
            case RELOAD_CHANGE_TYPE: type_str = "type"; break;
            case RELOAD_CHANGE_CONFIG: type_str = "config"; break;
        }
        
        printf("%zu. %s: %s [%s]%s\n", i + 1, type_str, 
               change->target ? change->target : "(unknown)",
               change->requires_restart ? "RESTART" : "live",
               change->is_critical ? " CRITICAL" : "");
    }
}

void hotreload_clear_changes(HotReloadManager* mgr) {
    if (mgr == NULL) return;
    
    for (size_t i = 0; i < mgr->change_count; i++) {
        if (mgr->changes[i].target) {
            free((void*)mgr->changes[i].target);
        }
    }
    mgr->change_count = 0;
}

/* =============================================================================
 * INTEGRAÇÃO COM GRAFOS
 * ============================================================================= */

int hotreload_update_node(HotReloadManager* mgr, UnifiedNode* node, 
                          void* new_data) {
    if (mgr == NULL || node == NULL) return 0;
    
    void* old_data = node->user_data;
    node->user_data = new_data;
    
    hotreload_record_change(mgr, RELOAD_CHANGE_GRAPH, node->name, 
                           old_data, new_data);
    
    return 1;
}

void hotreload_rebuild_graph(HotReloadManager* mgr) {
    if (mgr == NULL || mgr->graph == NULL) return;
    
    unified_graph_rebuild(mgr->graph);
}

