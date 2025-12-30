/**
 * =============================================================================
 * ASTERON CLUSTER-AWARE REACTIVITY v1.0
 * =============================================================================
 * 
 * Sistema reativo distribuído que permite:
 * 
 * 1. TRANSPARENT RPC
 *    - Chamar funções em outros nós Asteron como se fossem locais
 *    - Proxy automático para funções remotas
 *    - Serialização/deserialização transparente
 * 
 * 2. DISTRIBUTED STATE PROPAGATION
 *    - Se uma variável reativa mudar no Nó A, WHEN blocks no Nó B
 *      (em outro servidor) são disparados automaticamente
 *    - Protocolo de rede nativo para propagação de estado
 *    - Sincronização automática de estado reativo entre nós
 * 
 * =============================================================================
 */

#ifndef DISTRIBUTED_REACTIVE_H
#define DISTRIBUTED_REACTIVE_H

#include "reactive.h"
#include "../modules/native/net_module.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// NÓ DO CLUSTER
// =============================================================================

// Informações de um nó no cluster
typedef struct {
    char* node_id;              // ID único do nó (ex: "node-1", "server-2")
    char* address;              // Endereço IP ou hostname
    uint16_t port;               // Porta do serviço de cluster
    int socket;                  // Socket TCP conectado (ou -1 se desconectado)
    bool is_local;               // true se é o nó local
    bool is_connected;           // true se está conectado
    uint64_t last_heartbeat;     // Último heartbeat recebido (ns)
    uint32_t message_seq;        // Sequência de mensagens
} ClusterNode;

// =============================================================================
// RPC TRANSPARENTE
// =============================================================================

// Tipo de mensagem RPC
typedef enum {
    RPC_CALL,                    // Chamada de função remota
    RPC_RETURN,                  // Retorno de função
    RPC_ERROR                    // Erro na execução
} RpcMessageType;

// Mensagem RPC
typedef struct {
    RpcMessageType type;          // Tipo da mensagem
    uint32_t seq;                 // Sequência única
    char* function_name;          // Nome da função
    AsteronValue* args;           // Argumentos serializados
    size_t arg_count;             // Número de argumentos
    AsteronValue result;          // Resultado (para RPC_RETURN)
    char* error_message;          // Mensagem de erro (para RPC_ERROR)
    char* target_node;            // Nó destino (NULL = broadcast)
} RpcMessage;

// Proxy para função remota
typedef struct {
    char* function_name;          // Nome da função
    char* target_node;            // Nó onde a função está (NULL = qualquer nó)
    bool is_async;                // true se é chamada assíncrona
    uint32_t timeout_ms;          // Timeout em milissegundos
} RemoteFunctionProxy;

// =============================================================================
// PROPAGAÇÃO DE ESTADO DISTRIBUÍDO
// =============================================================================

// Tipo de evento de estado
typedef enum {
    STATE_UPDATE,                 // Atualização de estado
    STATE_SUBSCRIBE,              // Inscrição em mudanças de estado
    STATE_UNSUBSCRIBE,            // Cancelamento de inscrição
    STATE_SYNC                    // Sincronização completa de estado
} StateEventType;

// Evento de propagação de estado
typedef struct {
    StateEventType type;          // Tipo do evento
    char* node_name;              // Nome do nó reativo
    AsteronValue value;           // Novo valor (para STATE_UPDATE)
    uint64_t version;             // Versão do estado
    char* source_node_id;         // ID do nó que originou a mudança
    uint64_t timestamp_ns;        // Timestamp do evento
} StatePropagationEvent;

// Inscrição em estado remoto
typedef struct {
    char* node_name;              // Nome do nó reativo remoto
    char* source_node_id;         // ID do nó que possui o estado
    ReactiveNode* local_proxy;    // Nó reativo local que espelha o remoto
    bool is_active;               // true se a inscrição está ativa
} RemoteStateSubscription;

// =============================================================================
// CLUSTER-AWARE REACTIVE RUNTIME
// =============================================================================

// Runtime distribuído
typedef struct {
    ReactiveRuntime* local_rt;    // Runtime reativo local
    
    // Cluster
    ClusterNode* nodes;           // Lista de nós no cluster
    size_t node_count;            // Número de nós
    size_t node_capacity;          // Capacidade do array
    char* local_node_id;          // ID do nó local
    
    // RPC
    RemoteFunctionProxy* proxies; // Proxies de funções remotas
    size_t proxy_count;           // Número de proxies
    size_t proxy_capacity;         // Capacidade do array
    uint32_t next_rpc_seq;        // Próxima sequência RPC
    
    // Propagação de estado
    RemoteStateSubscription* subscriptions; // Inscrições em estados remotos
    size_t subscription_count;    // Número de inscrições
    size_t subscription_capacity; // Capacidade do array
    
    // Rede
    int listen_socket;            // Socket de escuta para cluster
    uint16_t cluster_port;        // Porta do serviço de cluster
    bool is_listening;            // true se está escutando conexões
    
    // Callbacks
    void (*on_state_propagated)(struct DistributedReactiveRuntime* rt, 
                                 StatePropagationEvent* event);
    void (*on_rpc_received)(struct DistributedReactiveRuntime* rt, 
                            RpcMessage* message);
    
    // Estatísticas
    uint64_t rpc_calls;           // Número de chamadas RPC
    uint64_t state_propagations;  // Número de propagações de estado
    uint64_t bytes_sent;          // Bytes enviados
    uint64_t bytes_received;      // Bytes recebidos
} DistributedReactiveRuntime;

// =============================================================================
// API PÚBLICA
// =============================================================================

// Inicializa runtime distribuído
DistributedReactiveRuntime* distributed_reactive_init(
    ReactiveRuntime* local_rt,
    const char* node_id,
    uint16_t cluster_port
);

// Destrói runtime distribuído
void distributed_reactive_destroy(DistributedReactiveRuntime* rt);

// =============================================================================
// GESTÃO DE CLUSTER
// =============================================================================

// Adiciona nó ao cluster
int distributed_reactive_add_node(DistributedReactiveRuntime* rt,
                                   const char* node_id,
                                   const char* address,
                                   uint16_t port);

// Remove nó do cluster
void distributed_reactive_remove_node(DistributedReactiveRuntime* rt,
                                       const char* node_id);

// Conecta a um nó do cluster
int distributed_reactive_connect_node(DistributedReactiveRuntime* rt,
                                       const char* node_id);

// Desconecta de um nó
void distributed_reactive_disconnect_node(DistributedReactiveRuntime* rt,
                                           const char* node_id);

// Obtém nó por ID
ClusterNode* distributed_reactive_get_node(DistributedReactiveRuntime* rt,
                                             const char* node_id);

// =============================================================================
// RPC TRANSPARENTE
// =============================================================================

// Registra proxy para função remota
int distributed_reactive_register_proxy(DistributedReactiveRuntime* rt,
                                          const char* function_name,
                                          const char* target_node,
                                          bool is_async,
                                          uint32_t timeout_ms);

// Chama função remota (síncrona)
AsteronValue distributed_reactive_call_remote(
    DistributedReactiveRuntime* rt,
    const char* function_name,
    AsteronValue* args,
    size_t arg_count,
    const char* target_node
);

// Chama função remota (assíncrona)
int distributed_reactive_call_remote_async(
    DistributedReactiveRuntime* rt,
    const char* function_name,
    AsteronValue* args,
    size_t arg_count,
    const char* target_node,
    void (*callback)(AsteronValue result, void* user_data),
    void* user_data
);

// Processa mensagens RPC recebidas
void distributed_reactive_process_rpc(DistributedReactiveRuntime* rt);

// =============================================================================
// PROPAGAÇÃO DE ESTADO DISTRIBUÍDO
// =============================================================================

// Inscreve-se em mudanças de estado remoto
int distributed_reactive_subscribe_state(
    DistributedReactiveRuntime* rt,
    const char* node_name,
    const char* source_node_id,
    ReactiveNode* local_proxy
);

// Cancela inscrição em estado remoto
void distributed_reactive_unsubscribe_state(
    DistributedReactiveRuntime* rt,
    const char* node_name,
    const char* source_node_id
);

// Propaga mudança de estado para outros nós
int distributed_reactive_propagate_state(
    DistributedReactiveRuntime* rt,
    ReactiveNode* node,
    AsteronValue new_value
);

// Sincroniza estado com nó remoto
int distributed_reactive_sync_state(
    DistributedReactiveRuntime* rt,
    const char* node_name,
    const char* source_node_id
);

// Processa eventos de propagação recebidos
void distributed_reactive_process_state_events(DistributedReactiveRuntime* rt);

// =============================================================================
// INTEGRAÇÃO COM REACTIVE RUNTIME
// =============================================================================

// Hook para reactive_set - propaga automaticamente
void distributed_reactive_hook_set(
    DistributedReactiveRuntime* rt,
    ReactiveNode* node,
    AsteronValue value
);

// Hook para reactive_propagate - processa eventos remotos
void distributed_reactive_hook_propagate(DistributedReactiveRuntime* rt);

// =============================================================================
// UTILITÁRIOS
// =============================================================================

// Serializa valor para transmissão
char* distributed_reactive_serialize_value(AsteronValue value);

// Deserializa valor recebido
AsteronValue distributed_reactive_deserialize_value(const char* data);

// Obtém estatísticas do cluster
void distributed_reactive_get_stats(DistributedReactiveRuntime* rt,
                                     uint64_t* rpc_calls,
                                     uint64_t* state_propagations,
                                     uint64_t* bytes_sent,
                                     uint64_t* bytes_received);

// Imprime informações do cluster
void distributed_reactive_print_cluster(DistributedReactiveRuntime* rt);

#ifdef __cplusplus
}
#endif

#endif // DISTRIBUTED_REACTIVE_H


