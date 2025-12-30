/**
 * =============================================================================
 * ASTERON STATE-DRIVEN RUNTIME v1.0
 * =============================================================================
 * 
 *  ███████╗████████╗ █████╗ ████████╗███████╗
 *  ██╔════╝╚══██╔══╝██╔══██╗╚══██╔══╝██╔════╝
 *  ███████╗   ██║   ███████║   ██║   █████╗  
 *  ╚════██║   ██║   ██╔══██║   ██║   ██╔══╝  
 *  ███████║   ██║   ██║  ██║   ██║   ███████╗
 *  ╚══════╝   ╚═╝   ╚═╝  ╚═╝   ╚═╝   ╚══════╝
 *              DRIVEN RUNTIME
 * 
 * =============================================================================
 * EXECUTION-ORIENTED STATE GRAPH (EOSG)
 * =============================================================================
 * 
 * CONCEITO CENTRAL:
 *   ❌ Executar quando chega request
 *   ✅ Executar quando o estado fica inconsistente
 * 
 * MODELO:
 *   1. Variáveis são NÓS REATIVOS com versão e dependentes
 *   2. Mudanças propagam automaticamente pelo grafo
 *   3. WHEN blocks disparam quando condições são satisfeitas
 *   4. Eventos externos (WebSocket, etc) apenas MUTAM estado
 *   5. A execução é CONSEQUÊNCIA, não causa
 * 
 * SINTAXE:
 * 
 *   // Variável reativa
 *   let reactive status = "idle"
 *   
 *   // Contexto vivo
 *   context Session {
 *       user: Socket
 *       status: State
 *   }
 *   
 *   // Gatilho baseado em estado
 *   when status == "connected" {
 *       start_stream()
 *   }
 *   
 *   // Efeito derivado
 *   derive total = price * quantity
 * 
 * =============================================================================
 */

#ifndef ASTERON_REACTIVE_H
#define ASTERON_REACTIVE_H

#include "../core/abi.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * CONFIGURAÇÃO
 * ============================================================================= */

#define REACTIVE_MAX_DEPS       64      /* Máximo de dependências por nó */
#define REACTIVE_MAX_WATCHERS   32      /* Máximo de watchers por nó */
#define REACTIVE_BATCH_SIZE     256     /* Tamanho do batch de propagação */
#define REACTIVE_VERSION_BITS   32      /* Bits para versão */

/* =============================================================================
 * TIPOS DE NÓS REATIVOS
 * ============================================================================= */

typedef enum {
    NODE_STATE,         /* Variável de estado (let reactive) */
    NODE_DERIVED,       /* Valor derivado (derive x = ...) */
    NODE_EFFECT,        /* Efeito colateral (when ...) */
    NODE_CONTEXT,       /* Contexto vivo */
    NODE_COMPUTED,      /* Computação memo-izada */
    NODE_RESOURCE       /* Recurso externo (socket, etc) */
} ReactiveNodeType;

typedef enum {
    STATE_CLEAN,        /* Valor está atualizado */
    STATE_DIRTY,        /* Precisa re-computar */
    STATE_PENDING,      /* Aguardando dependências */
    STATE_COMPUTING,    /* Sendo computado agora */
    STATE_ERROR         /* Erro durante computação */
} ReactiveState;

/* =============================================================================
 * NÓ REATIVO
 * ============================================================================= */

typedef struct ReactiveNode ReactiveNode;
typedef struct ReactiveRuntime ReactiveRuntime;

/**
 * Função de computação para nós derivados/effects
 */
typedef AsteronValue (*ComputeFn)(ReactiveRuntime* rt, ReactiveNode* node);

/**
 * Função de comparação para detectar mudanças
 */
typedef bool (*EqualsFn)(AsteronValue a, AsteronValue b);

/**
 * Função de cleanup quando nó é destruído
 */
typedef void (*CleanupFn)(ReactiveNode* node);

/**
 * Nó no grafo reativo
 */
struct ReactiveNode {
    /* Identificação */
    uint32_t id;                    /* ID único */
    const char* name;               /* Nome para debug */
    ReactiveNodeType type;          /* Tipo do nó */
    
    /* Estado */
    ReactiveState state;            /* Estado atual */
    _Atomic uint64_t version;       /* Versão (para tracking) */
    AsteronValue value;             /* Valor atual */
    AsteronValue prev_value;        /* Valor anterior (para diff) */
    
    /* Dependências (quem EU dependo) */
    ReactiveNode** deps;            /* Array de dependências */
    uint32_t dep_count;             /* Número de deps */
    uint32_t dep_capacity;          /* Capacidade */
    
    /* Dependentes (quem depende de MIM) */
    ReactiveNode** dependents;      /* Array de dependentes */
    uint32_t dependent_count;
    uint32_t dependent_capacity;
    
    /* Computação */
    ComputeFn compute;              /* Função de computação (se derived) */
    EqualsFn equals;                /* Comparador customizado */
    CleanupFn cleanup;              /* Cleanup callback */
    void* user_data;                /* Dados do usuário */
    
    /* Para WHEN blocks */
    struct {
        AsteronValue condition;     /* Condição a avaliar */
        AsteronValue body;          /* Código a executar */
        bool triggered;             /* Já foi ativado? */
        bool once;                  /* Executar apenas uma vez? */
    } trigger;
    
    /* Para CONTEXT */
    struct {
        const char* context_name;
        ReactiveNode** fields;      /* Campos do contexto */
        uint32_t field_count;
    } context;
    
    /* Metadados */
    uint32_t created_at;            /* Timestamp de criação */
    uint32_t last_updated;          /* Último update */
    uint32_t compute_count;         /* Quantas vezes foi computado */
    bool disposed;                  /* Foi descartado? */
    
    /* Lista encadeada para batch processing */
    ReactiveNode* next_dirty;
};

/* =============================================================================
 * BATCH DE PROPAGAÇÃO
 * ============================================================================= */

typedef struct {
    ReactiveNode** nodes;           /* Nós a processar */
    uint32_t count;                 /* Quantidade atual */
    uint32_t capacity;              /* Capacidade */
    uint32_t generation;            /* Geração para evitar ciclos */
} PropagationBatch;

/* =============================================================================
 * RUNTIME REATIVO
 * ============================================================================= */

struct ReactiveRuntime {
    /* Todos os nós */
    ReactiveNode** nodes;
    uint32_t node_count;
    uint32_t node_capacity;
    uint32_t next_node_id;
    
    /* Nó sendo computado atualmente (para tracking automático) */
    ReactiveNode* computing;
    
    /* Batch de propagação */
    PropagationBatch dirty_queue;
    bool is_propagating;
    uint32_t propagation_depth;
    
    /* Transações */
    bool in_transaction;
    ReactiveNode** transaction_changes;
    uint32_t transaction_count;
    
    /* Estatísticas */
    struct {
        uint64_t total_updates;
        uint64_t total_propagations;
        uint64_t skipped_unchanged;
        uint64_t effects_triggered;
        double avg_propagation_time_us;
    } stats;
    
    /* Contextos ativos */
    struct {
        const char* name;
        ReactiveNode* root;
    }* contexts;
    uint32_t context_count;
    
    /* Callbacks globais */
    void (*on_change)(ReactiveRuntime* rt, ReactiveNode* node);
    void (*on_error)(ReactiveRuntime* rt, ReactiveNode* node, const char* error);
    void (*on_effect)(ReactiveRuntime* rt, ReactiveNode* node);
};

/* =============================================================================
 * API - CRIAÇÃO DO RUNTIME
 * ============================================================================= */

/**
 * Cria runtime reativo
 */
ReactiveRuntime* reactive_runtime_create(void);

/**
 * Destrói runtime
 */
void reactive_runtime_destroy(ReactiveRuntime* rt);

/* =============================================================================
 * API - CRIAÇÃO DE NÓS
 * ============================================================================= */

/**
 * Cria variável de estado reativa
 * 
 * let reactive x = 10
 */
ReactiveNode* reactive_state(ReactiveRuntime* rt, const char* name, 
                              AsteronValue initial);

/**
 * Cria valor derivado
 * 
 * derive total = price * quantity
 */
ReactiveNode* reactive_derived(ReactiveRuntime* rt, const char* name,
                                ComputeFn compute, void* user_data);

/**
 * Cria efeito (when block)
 * 
 * when status == "connected" { ... }
 */
ReactiveNode* reactive_effect(ReactiveRuntime* rt, const char* name,
                               ComputeFn effect, void* user_data);

/**
 * Cria contexto vivo
 * 
 * context Session { ... }
 */
ReactiveNode* reactive_context(ReactiveRuntime* rt, const char* name);

/**
 * Adiciona campo a contexto
 */
ReactiveNode* reactive_context_field(ReactiveRuntime* rt, ReactiveNode* ctx,
                                      const char* field_name, AsteronValue initial);

/* =============================================================================
 * API - LEITURA E ESCRITA
 * ============================================================================= */

/**
 * Lê valor de um nó (registra dependência se dentro de compute)
 */
AsteronValue reactive_get(ReactiveRuntime* rt, ReactiveNode* node);

/**
 * Escreve valor em nó de estado (dispara propagação)
 */
void reactive_set(ReactiveRuntime* rt, ReactiveNode* node, AsteronValue value);

/**
 * Atualiza valor com função
 */
void reactive_update(ReactiveRuntime* rt, ReactiveNode* node,
                      AsteronValue (*updater)(AsteronValue current));

/**
 * Verifica se valor mudou desde última leitura
 */
bool reactive_changed(ReactiveNode* node);

/* =============================================================================
 * API - DEPENDÊNCIAS
 * ============================================================================= */

/**
 * Adiciona dependência manualmente
 */
void reactive_add_dep(ReactiveNode* node, ReactiveNode* dep);

/**
 * Remove dependência
 */
void reactive_remove_dep(ReactiveNode* node, ReactiveNode* dep);

/**
 * Limpa todas as dependências
 */
void reactive_clear_deps(ReactiveNode* node);

/* =============================================================================
 * API - PROPAGAÇÃO
 * ============================================================================= */

/**
 * Marca nó como sujo (precisa re-computar)
 */
void reactive_invalidate(ReactiveRuntime* rt, ReactiveNode* node);

/**
 * Propaga mudanças (chamado automaticamente ou manualmente)
 */
void reactive_propagate(ReactiveRuntime* rt);

/**
 * Força re-computação de um nó
 */
void reactive_recompute(ReactiveRuntime* rt, ReactiveNode* node);

/* =============================================================================
 * API - TRANSAÇÕES (BATCH UPDATES)
 * ============================================================================= */

/**
 * Inicia transação (agrupa múltiplas mudanças)
 */
void reactive_transaction_begin(ReactiveRuntime* rt);

/**
 * Commita transação (propaga todas as mudanças)
 */
void reactive_transaction_commit(ReactiveRuntime* rt);

/**
 * Aborta transação (descarta mudanças)
 */
void reactive_transaction_rollback(ReactiveRuntime* rt);

/* =============================================================================
 * API - WHEN BLOCKS
 * ============================================================================= */

/**
 * Cria when block
 * 
 * when condition { body }
 */
ReactiveNode* reactive_when(ReactiveRuntime* rt, const char* name,
                             ComputeFn condition, ComputeFn body,
                             bool once);

/**
 * Verifica e dispara when blocks pendentes
 */
void reactive_check_triggers(ReactiveRuntime* rt);

/* =============================================================================
 * API - CLEANUP
 * ============================================================================= */

/**
 * Dispõe um nó (remove do grafo)
 */
void reactive_dispose(ReactiveRuntime* rt, ReactiveNode* node);

/**
 * Limpa nós não utilizados
 */
void reactive_gc(ReactiveRuntime* rt);

/* =============================================================================
 * API - DEBUG E INTROSPECTION
 * ============================================================================= */

/**
 * Imprime estado do grafo
 */
void reactive_print_graph(ReactiveRuntime* rt);

/**
 * Obtém estatísticas
 */
void reactive_get_stats(ReactiveRuntime* rt, uint64_t* updates, 
                         uint64_t* propagations, uint64_t* effects);

/**
 * Exporta grafo para DOT (Graphviz)
 */
char* reactive_export_dot(ReactiveRuntime* rt);

/* =============================================================================
 * MACROS DE CONVENIÊNCIA
 * ============================================================================= */

/**
 * Cria state inline
 */
#define REACTIVE_STATE(rt, name, val) \
    reactive_state(rt, #name, val)

/**
 * Cria derived inline
 */
#define REACTIVE_DERIVED(rt, name, compute_fn) \
    reactive_derived(rt, #name, compute_fn, NULL)

/**
 * Cria effect inline
 */
#define REACTIVE_EFFECT(rt, name, effect_fn) \
    reactive_effect(rt, #name, effect_fn, NULL)

/**
 * Batch update
 */
#define REACTIVE_BATCH(rt, code) \
    do { \
        reactive_transaction_begin(rt); \
        code; \
        reactive_transaction_commit(rt); \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_REACTIVE_H */

