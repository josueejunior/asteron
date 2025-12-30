#ifndef HOTRELOAD_H
#define HOTRELOAD_H

#include "../vm/vm.h"
#include "../../graph/unified_graph.h"
#include <stddef.h>

/* =============================================================================
 * HOT-RELOAD GRÁFICO
 * =============================================================================
 * Sistema de hot-reload que atualiza código e grafos em tempo real,
 * permitindo ver efeitos imediatamente na execução.
 * ============================================================================= */

// Tipo de mudança detectada
typedef enum {
    RELOAD_CHANGE_CODE,         // Código mudou
    RELOAD_CHANGE_GRAPH,        // Grafo mudou
    RELOAD_CHANGE_DEPENDENCY,   // Dependência mudou
    RELOAD_CHANGE_TYPE,          // Tipo mudou
    RELOAD_CHANGE_CONFIG         // Configuração mudou
} ReloadChangeType;

// Mudança detectada
typedef struct ReloadChange {
    ReloadChangeType type;      // Tipo de mudança
    const char* target;         // Alvo da mudança (nome do nó/função)
    time_t timestamp;           // Quando foi detectada
    void* old_data;             // Dados antigos (se disponível)
    void* new_data;             // Dados novos
    int requires_restart;        // Requer reinício da execução?
    int is_critical;            // É uma mudança crítica?
} ReloadChange;

// Handler de hot-reload
typedef struct ReloadHandler {
    const char* name;           // Nome do handler
    int (*can_reload)(ReloadChange* change);  // Pode fazer reload desta mudança?
    int (*apply_change)(ReloadChange* change, VM* vm, UnifiedGraph* graph);  // Aplica mudança
    void (*on_success)(ReloadChange* change); // Callback de sucesso
    void (*on_failure)(ReloadChange* change, const char* error); // Callback de falha
} ReloadHandler;

// Gerenciador de hot-reload
typedef struct HotReloadManager {
    UnifiedGraph* graph;        // Grafo sendo monitorado
    VM* vm;                     // VM sendo monitorada
    
    // Mudanças detectadas
    ReloadChange* changes;      // Fila de mudanças
    size_t change_count;
    size_t change_capacity;
    
    // Handlers
    ReloadHandler* handlers;    // Handlers registrados
    size_t handler_count;
    size_t handler_capacity;
    
    // Configuração
    int enabled;                 // Hot-reload habilitado?
    int auto_apply;              // Aplica mudanças automaticamente?
    int watch_files;             // Monitora arquivos?
    int watch_graph;             // Monitora grafo?
    
    // Callbacks
    void (*on_change_detected)(ReloadChange* change);
    void (*on_reload_success)(const char* target);
    void (*on_reload_failure)(const char* target, const char* error);
    
    // Estatísticas
    struct {
        size_t total_changes;
        size_t successful_reloads;
        size_t failed_reloads;
        size_t restarts_required;
    } stats;
} HotReloadManager;

/* =============================================================================
 * API - CRIAÇÃO E DESTRUIÇÃO
 * ============================================================================= */

/**
 * Cria gerenciador de hot-reload
 */
HotReloadManager* hotreload_manager_create(VM* vm, UnifiedGraph* graph);

/**
 * Destrói gerenciador de hot-reload
 */
void hotreload_manager_destroy(HotReloadManager* mgr);

/* =============================================================================
 * API - MONITORAMENTO
 * ============================================================================= */

/**
 * Habilita hot-reload
 */
void hotreload_enable(HotReloadManager* mgr);

/**
 * Desabilita hot-reload
 */
void hotreload_disable(HotReloadManager* mgr);

/**
 * Verifica mudanças (chamado periodicamente)
 */
void hotreload_check_changes(HotReloadManager* mgr);

/**
 * Aplica mudança específica
 */
int hotreload_apply_change(HotReloadManager* mgr, ReloadChange* change);

/**
 * Aplica todas as mudanças pendentes
 */
int hotreload_apply_all(HotReloadManager* mgr);

/* =============================================================================
 * API - HANDLERS
 * ============================================================================= */

/**
 * Registra handler de hot-reload
 */
void hotreload_register_handler(HotReloadManager* mgr, ReloadHandler* handler);

/**
 * Remove handler
 */
void hotreload_unregister_handler(HotReloadManager* mgr, const char* name);

/* =============================================================================
 * API - MUDANÇAS
 * ============================================================================= */

/**
 * Registra mudança manualmente
 */
void hotreload_record_change(HotReloadManager* mgr, ReloadChangeType type, 
                             const char* target, void* old_data, void* new_data);

/**
 * Lista mudanças pendentes
 */
void hotreload_list_changes(HotReloadManager* mgr);

/**
 * Limpa mudanças processadas
 */
void hotreload_clear_changes(HotReloadManager* mgr);

/* =============================================================================
 * API - INTEGRAÇÃO COM GRAFOS
 * ============================================================================= */

/**
 * Atualiza nó do grafo em tempo real
 */
int hotreload_update_node(HotReloadManager* mgr, UnifiedNode* node, 
                          void* new_data);

/**
 * Reconstrói grafo após mudanças
 */
void hotreload_rebuild_graph(HotReloadManager* mgr);

#endif // HOTRELOAD_H

