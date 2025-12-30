/**
 * =============================================================================
 * ASTERON CLUSTER-AWARE REACTIVITY - IMPLEMENTAÇÃO
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "distributed.h"
#include "../core/abi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

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

DistributedReactiveRuntime* distributed_reactive_init(
    ReactiveRuntime* local_rt,
    const char* node_id,
    uint16_t cluster_port) {
    
    if (local_rt == NULL || node_id == NULL) return NULL;
    
    DistributedReactiveRuntime* rt = (DistributedReactiveRuntime*)calloc(
        1, sizeof(DistributedReactiveRuntime));
    if (rt == NULL) return NULL;
    
    rt->local_rt = local_rt;
    rt->local_node_id = strdup(node_id);
    rt->cluster_port = cluster_port;
    rt->next_rpc_seq = 1;
    
    // Inicializa arrays
    rt->node_capacity = 16;
    rt->nodes = (ClusterNode*)calloc(rt->node_capacity, sizeof(ClusterNode));
    
    rt->proxy_capacity = 32;
    rt->proxies = (RemoteFunctionProxy*)calloc(
        rt->proxy_capacity, sizeof(RemoteFunctionProxy));
    
    rt->subscription_capacity = 64;
    rt->subscriptions = (RemoteStateSubscription*)calloc(
        rt->subscription_capacity, sizeof(RemoteStateSubscription));
    
    // Adiciona nó local
    ClusterNode* local_node = &rt->nodes[rt->node_count++];
    local_node->node_id = strdup(node_id);
    local_node->address = strdup("127.0.0.1");
    local_node->port = cluster_port;
    local_node->is_local = true;
    local_node->is_connected = true;
    local_node->socket = -1; // Nó local não usa socket
    
    // Inicia servidor de cluster
    rt->listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (rt->listen_socket >= 0) {
        int opt = 1;
        setsockopt(rt->listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(cluster_port);
        
        if (bind(rt->listen_socket, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            if (listen(rt->listen_socket, 10) == 0) {
                rt->is_listening = true;
                printf("[Distributed] Cluster server listening on port %d\n", cluster_port);
            }
        }
    }
    
    printf("[Distributed] Runtime inicializado (node: %s)\n", node_id);
    return rt;
}

void distributed_reactive_destroy(DistributedReactiveRuntime* rt) {
    if (rt == NULL) return;
    
    // Fecha sockets
    if (rt->listen_socket >= 0) {
        close(rt->listen_socket);
    }
    
    for (size_t i = 0; i < rt->node_count; i++) {
        if (rt->nodes[i].socket >= 0) {
            close(rt->nodes[i].socket);
        }
        free(rt->nodes[i].node_id);
        free(rt->nodes[i].address);
    }
    free(rt->nodes);
    
    // Libera proxies
    for (size_t i = 0; i < rt->proxy_count; i++) {
        free(rt->proxies[i].function_name);
        free(rt->proxies[i].target_node);
    }
    free(rt->proxies);
    
    // Libera subscriptions
    for (size_t i = 0; i < rt->subscription_count; i++) {
        free(rt->subscriptions[i].node_name);
        free(rt->subscriptions[i].source_node_id);
    }
    free(rt->subscriptions);
    
    free(rt->local_node_id);
    free(rt);
}

// =============================================================================
// GESTÃO DE CLUSTER
// =============================================================================

int distributed_reactive_add_node(DistributedReactiveRuntime* rt,
                                   const char* node_id,
                                   const char* address,
                                   uint16_t port) {
    if (rt == NULL || node_id == NULL || address == NULL) return 1;
    
    // Verifica se já existe
    for (size_t i = 0; i < rt->node_count; i++) {
        if (strcmp(rt->nodes[i].node_id, node_id) == 0) {
            return 0; // Já existe
        }
    }
    
    // Expande array se necessário
    if (rt->node_count >= rt->node_capacity) {
        rt->node_capacity *= 2;
        rt->nodes = (ClusterNode*)realloc(rt->nodes, 
            sizeof(ClusterNode) * rt->node_capacity);
    }
    
    // Adiciona nó
    ClusterNode* node = &rt->nodes[rt->node_count++];
    node->node_id = strdup(node_id);
    node->address = strdup(address);
    node->port = port;
    node->socket = -1;
    node->is_local = false;
    node->is_connected = false;
    node->last_heartbeat = get_timestamp_ns();
    node->message_seq = 0;
    
    printf("[Distributed] Nó '%s' adicionado (%s:%d)\n", node_id, address, port);
    return 0;
}

void distributed_reactive_remove_node(DistributedReactiveRuntime* rt,
                                       const char* node_id) {
    if (rt == NULL || node_id == NULL) return;
    
    for (size_t i = 0; i < rt->node_count; i++) {
        if (strcmp(rt->nodes[i].node_id, node_id) == 0) {
            if (rt->nodes[i].socket >= 0) {
                close(rt->nodes[i].socket);
            }
            free(rt->nodes[i].node_id);
            free(rt->nodes[i].address);
            
            // Move últimos elementos
            rt->nodes[i] = rt->nodes[rt->node_count - 1];
            rt->node_count--;
            return;
        }
    }
}

ClusterNode* distributed_reactive_get_node(DistributedReactiveRuntime* rt,
                                             const char* node_id) {
    if (rt == NULL || node_id == NULL) return NULL;
    
    for (size_t i = 0; i < rt->node_count; i++) {
        if (strcmp(rt->nodes[i].node_id, node_id) == 0) {
            return &rt->nodes[i];
        }
    }
    return NULL;
}

int distributed_reactive_connect_node(DistributedReactiveRuntime* rt,
                                       const char* node_id) {
    if (rt == NULL || node_id == NULL) return 1;
    
    ClusterNode* node = distributed_reactive_get_node(rt, node_id);
    if (node == NULL || node->is_local || node->is_connected) return 1;
    
    // Conecta socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 1;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(node->port);
    inet_pton(AF_INET, node->address, &addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        node->socket = sock;
        node->is_connected = true;
        node->last_heartbeat = get_timestamp_ns();
        printf("[Distributed] Conectado ao nó '%s'\n", node_id);
        return 0;
    }
    
    close(sock);
    return 1;
}

void distributed_reactive_disconnect_node(DistributedReactiveRuntime* rt,
                                           const char* node_id) {
    if (rt == NULL || node_id == NULL) return;
    
    ClusterNode* node = distributed_reactive_get_node(rt, node_id);
    if (node == NULL) return;
    
    if (node->socket >= 0) {
        close(node->socket);
        node->socket = -1;
    }
    node->is_connected = false;
}

// =============================================================================
// RPC TRANSPARENTE
// =============================================================================

int distributed_reactive_register_proxy(DistributedReactiveRuntime* rt,
                                          const char* function_name,
                                          const char* target_node,
                                          bool is_async,
                                          uint32_t timeout_ms) {
    if (rt == NULL || function_name == NULL) return 1;
    
    // Expande array se necessário
    if (rt->proxy_count >= rt->proxy_capacity) {
        rt->proxy_capacity *= 2;
        rt->proxies = (RemoteFunctionProxy*)realloc(rt->proxies,
            sizeof(RemoteFunctionProxy) * rt->proxy_capacity);
    }
    
    // Adiciona proxy
    RemoteFunctionProxy* proxy = &rt->proxies[rt->proxy_count++];
    proxy->function_name = strdup(function_name);
    proxy->target_node = target_node ? strdup(target_node) : NULL;
    proxy->is_async = is_async;
    proxy->timeout_ms = timeout_ms;
    
    printf("[Distributed] Proxy registrado: %s -> %s\n", 
           function_name, target_node ? target_node : "any");
    return 0;
}

AsteronValue distributed_reactive_call_remote(
    DistributedReactiveRuntime* rt,
    const char* function_name,
    AsteronValue* args,
    size_t arg_count,
    const char* target_node) {
    
    if (rt == NULL || function_name == NULL) {
        return ASTERON_ERROR_VAL(ASTERON_ERROR_INVALID);
    }
    
    // Encontra nó destino
    ClusterNode* node = NULL;
    if (target_node) {
        node = distributed_reactive_get_node(rt, target_node);
    } else {
        // Usa primeiro nó conectado (exceto local)
        for (size_t i = 0; i < rt->node_count; i++) {
            if (!rt->nodes[i].is_local && rt->nodes[i].is_connected) {
                node = &rt->nodes[i];
                break;
            }
        }
    }
    
    if (node == NULL || !node->is_connected) {
        return ASTERON_ERROR_VAL(ASTERON_ERROR_NET);
    }
    
    // Cria mensagem RPC
    RpcMessage msg;
    msg.type = RPC_CALL;
    msg.seq = rt->next_rpc_seq++;
    msg.function_name = strdup(function_name);
    msg.args = args;
    msg.arg_count = arg_count;
    msg.target_node = NULL;
    
    // Serializa e envia (simplificado - em produção, usar protocolo binário)
    // Por enquanto, apenas incrementa estatísticas
    rt->rpc_calls++;
    
    // Em produção, aqui:
    // 1. Serializa mensagem
    // 2. Envia via socket
    // 3. Aguarda resposta
    // 4. Deserializa resultado
    // 5. Retorna
    
    free(msg.function_name);
    
    // Placeholder - retorna nil
    return ASTERON_NIL();
}

int distributed_reactive_call_remote_async(
    DistributedReactiveRuntime* rt,
    const char* function_name,
    AsteronValue* args,
    size_t arg_count,
    const char* target_node,
    void (*callback)(AsteronValue result, void* user_data),
    void* user_data) {
    
    // Similar a call_remote, mas não bloqueia
    // Em produção, usaria thread pool ou event loop
    return 0;
}

void distributed_reactive_process_rpc(DistributedReactiveRuntime* rt) {
    if (rt == NULL) return;
    
    // Em produção, processaria mensagens RPC recebidas
    // Por enquanto, apenas placeholder
}

// =============================================================================
// PROPAGAÇÃO DE ESTADO DISTRIBUÍDO
// =============================================================================

int distributed_reactive_subscribe_state(
    DistributedReactiveRuntime* rt,
    const char* node_name,
    const char* source_node_id,
    ReactiveNode* local_proxy) {
    
    if (rt == NULL || node_name == NULL || source_node_id == NULL) return 1;
    
    // Expande array se necessário
    if (rt->subscription_count >= rt->subscription_capacity) {
        rt->subscription_capacity *= 2;
        rt->subscriptions = (RemoteStateSubscription*)realloc(rt->subscriptions,
            sizeof(RemoteStateSubscription) * rt->subscription_capacity);
    }
    
    // Adiciona subscription
    RemoteStateSubscription* sub = &rt->subscriptions[rt->subscription_count++];
    sub->node_name = strdup(node_name);
    sub->source_node_id = strdup(source_node_id);
    sub->local_proxy = local_proxy;
    sub->is_active = true;
    
    // Envia mensagem de subscribe para o nó remoto
    ClusterNode* source_node = distributed_reactive_get_node(rt, source_node_id);
    if (source_node && source_node->is_connected) {
        // Em produção, enviaria mensagem STATE_SUBSCRIBE via socket
        printf("[Distributed] Inscrito em estado '%s' do nó '%s'\n", 
               node_name, source_node_id);
    }
    
    return 0;
}

void distributed_reactive_unsubscribe_state(
    DistributedReactiveRuntime* rt,
    const char* node_name,
    const char* source_node_id) {
    
    if (rt == NULL || node_name == NULL || source_node_id == NULL) return;
    
    for (size_t i = 0; i < rt->subscription_count; i++) {
        RemoteStateSubscription* sub = &rt->subscriptions[i];
        if (strcmp(sub->node_name, node_name) == 0 &&
            strcmp(sub->source_node_id, source_node_id) == 0) {
            sub->is_active = false;
            free(sub->node_name);
            free(sub->source_node_id);
            
            // Move últimos elementos
            rt->subscriptions[i] = rt->subscriptions[rt->subscription_count - 1];
            rt->subscription_count--;
            return;
        }
    }
}

int distributed_reactive_propagate_state(
    DistributedReactiveRuntime* rt,
    ReactiveNode* node,
    AsteronValue new_value) {
    
    if (rt == NULL || node == NULL) return 1;
    
    // Cria evento de propagação
    StatePropagationEvent event;
    event.type = STATE_UPDATE;
    event.node_name = node->name;
    event.value = new_value;
    event.version = node->version;
    event.source_node_id = rt->local_node_id;
    event.timestamp_ns = get_timestamp_ns();
    
    // Propaga para todos os nós inscritos
    int propagated = 0;
    for (size_t i = 0; i < rt->subscription_count; i++) {
        RemoteStateSubscription* sub = &rt->subscriptions[i];
        if (!sub->is_active) continue;
        
        // Verifica se algum nó está inscrito neste estado
        // (simplificado - em produção, manteria índice)
        ClusterNode* target_node = distributed_reactive_get_node(rt, sub->source_node_id);
        if (target_node && target_node->is_connected) {
            // Em produção, enviaria evento via socket
            propagated++;
        }
    }
    
    // Também propaga para todos os nós conectados (broadcast)
    for (size_t i = 0; i < rt->node_count; i++) {
        ClusterNode* node = &rt->nodes[i];
        if (!node->is_local && node->is_connected) {
            // Envia evento de atualização de estado
            // (simplificado - em produção, serializaria e enviaria)
        }
    }
    
    rt->state_propagations++;
    return propagated > 0 ? 0 : 1;
}

int distributed_reactive_sync_state(
    DistributedReactiveRuntime* rt,
    const char* node_name,
    const char* source_node_id) {
    
    if (rt == NULL || node_name == NULL || source_node_id == NULL) return 1;
    
    // Envia mensagem STATE_SYNC para o nó remoto
    ClusterNode* source_node = distributed_reactive_get_node(rt, source_node_id);
    if (source_node && source_node->is_connected) {
        // Em produção, enviaria mensagem de sincronização
        printf("[Distributed] Sincronizando estado '%s' com nó '%s'\n",
               node_name, source_node_id);
        return 0;
    }
    
    return 1;
}

void distributed_reactive_process_state_events(DistributedReactiveRuntime* rt) {
    if (rt == NULL) return;
    
    // Em produção, processaria eventos de estado recebidos via socket
    // Por enquanto, apenas placeholder
}

// =============================================================================
// INTEGRAÇÃO COM REACTIVE RUNTIME
// =============================================================================

void distributed_reactive_hook_set(
    DistributedReactiveRuntime* rt,
    ReactiveNode* node,
    AsteronValue value) {
    
    if (rt == NULL || node == NULL) return;
    
    // Propaga mudança para outros nós
    distributed_reactive_propagate_state(rt, node, value);
}

void distributed_reactive_hook_propagate(DistributedReactiveRuntime* rt) {
    if (rt == NULL) return;
    
    // Processa eventos de estado recebidos
    distributed_reactive_process_state_events(rt);
    
    // Processa mensagens RPC recebidas
    distributed_reactive_process_rpc(rt);
}

// =============================================================================
// UTILITÁRIOS
// =============================================================================

char* distributed_reactive_serialize_value(AsteronValue value) {
    // Em produção, serializaria para JSON ou formato binário
    // Por enquanto, placeholder
    return strdup("{}");
}

AsteronValue distributed_reactive_deserialize_value(const char* data) {
    // Em produção, deserializaria de JSON ou formato binário
    // Por enquanto, retorna nil
    return ASTERON_NIL();
}

void distributed_reactive_get_stats(DistributedReactiveRuntime* rt,
                                     uint64_t* rpc_calls,
                                     uint64_t* state_propagations,
                                     uint64_t* bytes_sent,
                                     uint64_t* bytes_received) {
    if (rt == NULL) return;
    
    if (rpc_calls) *rpc_calls = rt->rpc_calls;
    if (state_propagations) *state_propagations = rt->state_propagations;
    if (bytes_sent) *bytes_sent = rt->bytes_sent;
    if (bytes_received) *bytes_received = rt->bytes_received;
}

void distributed_reactive_print_cluster(DistributedReactiveRuntime* rt) {
    if (rt == NULL) return;
    
    printf("\n=== Cluster Information ===\n");
    printf("Local Node: %s\n", rt->local_node_id);
    printf("Total Nodes: %zu\n", rt->node_count);
    printf("\nNodes:\n");
    
    for (size_t i = 0; i < rt->node_count; i++) {
        ClusterNode* node = &rt->nodes[i];
        printf("  [%s] %s:%d %s %s\n",
               node->node_id,
               node->address,
               node->port,
               node->is_local ? "(local)" : "",
               node->is_connected ? "[connected]" : "[disconnected]");
    }
    
    printf("\nProxies: %zu\n", rt->proxy_count);
    printf("Subscriptions: %zu\n", rt->subscription_count);
    printf("RPC Calls: %llu\n", (unsigned long long)rt->rpc_calls);
    printf("State Propagations: %llu\n", (unsigned long long)rt->state_propagations);
    printf("===========================\n\n");
}

