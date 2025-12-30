/**
 * =============================================================================
 * ASTERON WEBSOCKET STATE DRIVER v1.0
 * =============================================================================
 * 
 * WebSocket como DRIVER DE ESTADO, não executor de lógica.
 * 
 * CONCEITO:
 *   ❌ on_message(ws, data) { process(data); send_response(); }
 *   ✅ on_message(ws, data) { state.data = data; }  // Runtime reage
 * 
 * O WebSocket apenas MUTA estado.
 * O runtime reativo EXECUTA baseado em mudanças.
 * 
 * EXEMPLO:
 * 
 *   context CallSession {
 *       userA: Socket
 *       userB: Socket  
 *       status: State
 *       audioReady: bool
 *   }
 *   
 *   // WebSocket só muta estado
 *   on ws.message {
 *       session.audioReady = true
 *   }
 *   
 *   // Runtime reage automaticamente
 *   when session.audioReady && session.status == "connected" {
 *       route_audio(session.userA, session.userB)
 *   }
 * 
 * =============================================================================
 */

#ifndef ASTERON_WS_DRIVER_H
#define ASTERON_WS_DRIVER_H

#include "reactive.h"
#include "../modules/native/net_module.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * TIPOS
 * ============================================================================= */

typedef enum {
    WS_STATE_CONNECTING,
    WS_STATE_OPEN,
    WS_STATE_CLOSING,
    WS_STATE_CLOSED,
    WS_STATE_ERROR
} WebSocketState;

typedef enum {
    WS_MSG_TEXT,
    WS_MSG_BINARY,
    WS_MSG_PING,
    WS_MSG_PONG,
    WS_MSG_CLOSE
} WebSocketMsgType;

typedef struct WebSocket WebSocket;

/**
 * Handler de mensagem - apenas MUTA estado, não executa lógica
 */
typedef void (*WsMessageHandler)(WebSocket* ws, WebSocketMsgType type,
                                   const uint8_t* data, size_t len,
                                   void* user_data);

/**
 * Handler de estado
 */
typedef void (*WsStateHandler)(WebSocket* ws, WebSocketState state,
                                 void* user_data);

/**
 * WebSocket com integração reativa
 */
struct WebSocket {
    /* Identificação */
    uint32_t id;
    const char* url;
    
    /* Estado */
    WebSocketState state;
    int fd;
    
    /* Integração reativa */
    ReactiveRuntime* rt;
    ReactiveNode* state_node;       /* Nó reativo para estado */
    ReactiveNode* message_node;     /* Nó reativo para última mensagem */
    ReactiveNode* error_node;       /* Nó reativo para erro */
    
    /* Handlers (opcionais, para bridges com código existente) */
    WsMessageHandler on_message;
    WsStateHandler on_state;
    void* user_data;
    
    /* Buffer de recebimento */
    uint8_t* recv_buffer;
    size_t recv_capacity;
    size_t recv_len;
    
    /* Metadados */
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
};

/* =============================================================================
 * SERVIDOR WEBSOCKET
 * ============================================================================= */

typedef struct {
    int listen_fd;
    int port;
    
    /* WebSockets conectados */
    WebSocket** clients;
    uint32_t client_count;
    uint32_t client_capacity;
    
    /* Integração reativa */
    ReactiveRuntime* rt;
    ReactiveNode* clients_node;     /* Array reativo de clientes */
    
    /* Callbacks */
    void (*on_connect)(WebSocket* ws, void* user_data);
    void (*on_disconnect)(WebSocket* ws, void* user_data);
    void* user_data;
} WebSocketServer;

/* =============================================================================
 * API - CLIENTE
 * ============================================================================= */

/**
 * Cria WebSocket conectado ao runtime reativo
 */
WebSocket* ws_create(ReactiveRuntime* rt, const char* url);

/**
 * Conecta ao servidor
 */
AsteronResult ws_connect(WebSocket* ws);

/**
 * Envia mensagem (apenas muta buffer, não bloqueia)
 */
AsteronResult ws_send(WebSocket* ws, const uint8_t* data, size_t len,
                       WebSocketMsgType type);

/**
 * Envia texto
 */
AsteronResult ws_send_text(WebSocket* ws, const char* text);

/**
 * Fecha conexão
 */
void ws_close(WebSocket* ws);

/**
 * Libera recursos
 */
void ws_destroy(WebSocket* ws);

/**
 * Processa eventos pendentes (non-blocking)
 * Chame em loop ou deixe o runtime chamar
 */
void ws_poll(WebSocket* ws);

/* =============================================================================
 * API - SERVIDOR
 * ============================================================================= */

/**
 * Cria servidor WebSocket
 */
WebSocketServer* ws_server_create(ReactiveRuntime* rt, int port);

/**
 * Inicia escuta
 */
AsteronResult ws_server_listen(WebSocketServer* server);

/**
 * Aceita conexões pendentes (non-blocking)
 */
WebSocket* ws_server_accept(WebSocketServer* server);

/**
 * Broadcast para todos os clientes
 */
void ws_server_broadcast(WebSocketServer* server, const uint8_t* data, size_t len);

/**
 * Processa eventos (non-blocking)
 */
void ws_server_poll(WebSocketServer* server);

/**
 * Para servidor
 */
void ws_server_stop(WebSocketServer* server);

/**
 * Libera recursos
 */
void ws_server_destroy(WebSocketServer* server);

/* =============================================================================
 * INTEGRAÇÃO REATIVA
 * ============================================================================= */

/**
 * Obtém nó reativo do estado do WebSocket
 */
ReactiveNode* ws_state_node(WebSocket* ws);

/**
 * Obtém nó reativo da última mensagem
 */
ReactiveNode* ws_message_node(WebSocket* ws);

/**
 * Cria efeito que reage a mensagens do WebSocket
 * 
 * when ws.message {
 *     process(ws.message)
 * }
 */
ReactiveNode* ws_on_message(ReactiveRuntime* rt, WebSocket* ws,
                             ComputeFn handler, void* user_data);

/**
 * Cria efeito que reage a mudanças de estado
 * 
 * when ws.state == WS_STATE_OPEN {
 *     initialize()
 * }
 */
ReactiveNode* ws_on_state(ReactiveRuntime* rt, WebSocket* ws,
                           WebSocketState target_state,
                           ComputeFn handler, void* user_data);

/* =============================================================================
 * MACROS DE CONVENIÊNCIA
 * ============================================================================= */

/**
 * Cria WebSocket e conecta
 */
#define WS_CONNECT(rt, url) ({ \
    WebSocket* _ws = ws_create(rt, url); \
    ws_connect(_ws); \
    _ws; \
})

/**
 * Handler de mensagem reativo
 */
#define WS_ON_MESSAGE(rt, ws, handler) \
    ws_on_message(rt, ws, handler, NULL)

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_WS_DRIVER_H */

