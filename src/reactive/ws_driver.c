/**
 * =============================================================================
 * ASTERON WEBSOCKET STATE DRIVER - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "ws_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <poll.h>
#endif

/* =============================================================================
 * HELPERS
 * ============================================================================= */

static uint32_t g_next_ws_id = 1;

static void set_nonblocking(int fd) {
    #ifndef _WIN32
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    #else
    u_long mode = 1;
    ioctlsocket(fd, FIONBIO, &mode);
    #endif
}

/* =============================================================================
 * CLIENTE WEBSOCKET
 * ============================================================================= */

WebSocket* ws_create(ReactiveRuntime* rt, const char* url) {
    WebSocket* ws = (WebSocket*)calloc(1, sizeof(WebSocket));
    if (!ws) return NULL;
    
    ws->id = g_next_ws_id++;
    ws->url = url ? strdup(url) : NULL;
    ws->state = WS_STATE_CLOSED;
    ws->fd = -1;
    ws->rt = rt;
    
    /* Buffer de recebimento */
    ws->recv_capacity = 64 * 1024;  /* 64KB */
    ws->recv_buffer = (uint8_t*)malloc(ws->recv_capacity);
    ws->recv_len = 0;
    
    /* Cria nós reativos */
    if (rt) {
        char name[64];
        
        snprintf(name, sizeof(name), "ws_%u_state", ws->id);
        ws->state_node = reactive_state(rt, name, ASTERON_NUMBER(WS_STATE_CLOSED));
        
        snprintf(name, sizeof(name), "ws_%u_message", ws->id);
        ws->message_node = reactive_state(rt, name, ASTERON_NIL());
        
        snprintf(name, sizeof(name), "ws_%u_error", ws->id);
        ws->error_node = reactive_state(rt, name, ASTERON_NIL());
    }
    
    return ws;
}

AsteronResult ws_connect(WebSocket* ws) {
    if (!ws || !ws->url) return ASTERON_ERROR_INVALID;
    
    /* Parse URL (simplificado: host:port) */
    char host[256] = {0};
    int port = 80;
    
    const char* p = ws->url;
    if (strncmp(p, "ws://", 5) == 0) p += 5;
    else if (strncmp(p, "wss://", 6) == 0) { p += 6; port = 443; }
    
    const char* colon = strchr(p, ':');
    const char* slash = strchr(p, '/');
    
    if (colon && (!slash || colon < slash)) {
        size_t host_len = colon - p;
        memcpy(host, p, host_len);
        port = atoi(colon + 1);
    } else if (slash) {
        size_t host_len = slash - p;
        memcpy(host, p, host_len);
    } else {
        strcpy(host, p);
    }
    
    /* Atualiza estado reativo */
    ws->state = WS_STATE_CONNECTING;
    if (ws->rt && ws->state_node) {
        reactive_set(ws->rt, ws->state_node, ASTERON_NUMBER(WS_STATE_CONNECTING));
    }
    
    /* Resolve hostname */
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    if (getaddrinfo(host, port_str, &hints, &result) != 0) {
        ws->state = WS_STATE_ERROR;
        if (ws->rt && ws->state_node) {
            reactive_set(ws->rt, ws->state_node, ASTERON_NUMBER(WS_STATE_ERROR));
        }
        return ASTERON_ERROR_NET;
    }
    
    /* Cria socket */
    ws->fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ws->fd < 0) {
        freeaddrinfo(result);
        ws->state = WS_STATE_ERROR;
        return ASTERON_ERROR_NET;
    }
    
    /* Conecta */
    if (connect(ws->fd, result->ai_addr, result->ai_addrlen) < 0) {
        #ifndef _WIN32
        close(ws->fd);
        #else
        closesocket(ws->fd);
        #endif
        freeaddrinfo(result);
        ws->fd = -1;
        ws->state = WS_STATE_ERROR;
        return ASTERON_ERROR_NET;
    }
    
    freeaddrinfo(result);
    
    /* Faz upgrade WebSocket (simplificado) */
    char handshake[512];
    snprintf(handshake, sizeof(handshake),
        "GET / HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n", host);
    
    send(ws->fd, handshake, strlen(handshake), 0);
    
    /* Aguarda resposta (simplificado) */
    char response[1024];
    recv(ws->fd, response, sizeof(response) - 1, 0);
    
    /* Configura non-blocking */
    set_nonblocking(ws->fd);
    
    /* Sucesso */
    ws->state = WS_STATE_OPEN;
    if (ws->rt && ws->state_node) {
        reactive_set(ws->rt, ws->state_node, ASTERON_NUMBER(WS_STATE_OPEN));
    }
    
    if (ws->on_state) {
        ws->on_state(ws, WS_STATE_OPEN, ws->user_data);
    }
    
    return ASTERON_OK;
}

AsteronResult ws_send(WebSocket* ws, const uint8_t* data, size_t len,
                       WebSocketMsgType type) {
    if (!ws || ws->state != WS_STATE_OPEN || ws->fd < 0) {
        return ASTERON_ERROR_NET;
    }
    
    /* Constrói frame WebSocket (simplificado) */
    uint8_t frame[16 + len];
    size_t frame_len = 0;
    
    /* Opcode */
    frame[0] = 0x80 | (type == WS_MSG_TEXT ? 0x01 : 0x02);
    frame_len = 1;
    
    /* Length */
    if (len < 126) {
        frame[1] = 0x80 | (uint8_t)len;  /* Mask bit set */
        frame_len = 2;
    } else if (len < 65536) {
        frame[1] = 0x80 | 126;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        frame_len = 4;
    } else {
        /* Messages > 64KB not supported in this simple implementation */
        return ASTERON_ERROR_OVERFLOW;
    }
    
    /* Mask key */
    uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
    memcpy(frame + frame_len, mask, 4);
    frame_len += 4;
    
    /* Masked data */
    for (size_t i = 0; i < len; i++) {
        frame[frame_len + i] = data[i] ^ mask[i % 4];
    }
    frame_len += len;
    
    int sent = send(ws->fd, (char*)frame, (int)frame_len, 0);
    if (sent < 0) {
        return ASTERON_ERROR_NET;
    }
    
    ws->messages_sent++;
    ws->bytes_sent += len;
    
    return ASTERON_OK;
}

AsteronResult ws_send_text(WebSocket* ws, const char* text) {
    return ws_send(ws, (const uint8_t*)text, strlen(text), WS_MSG_TEXT);
}

void ws_close(WebSocket* ws) {
    if (!ws) return;
    
    if (ws->fd >= 0) {
        ws->state = WS_STATE_CLOSING;
        if (ws->rt && ws->state_node) {
            reactive_set(ws->rt, ws->state_node, ASTERON_NUMBER(WS_STATE_CLOSING));
        }
        
        /* Envia close frame */
        uint8_t close_frame[] = {0x88, 0x00};
        send(ws->fd, (char*)close_frame, 2, 0);
        
        #ifndef _WIN32
        close(ws->fd);
        #else
        closesocket(ws->fd);
        #endif
        ws->fd = -1;
    }
    
    ws->state = WS_STATE_CLOSED;
    if (ws->rt && ws->state_node) {
        reactive_set(ws->rt, ws->state_node, ASTERON_NUMBER(WS_STATE_CLOSED));
    }
    
    if (ws->on_state) {
        ws->on_state(ws, WS_STATE_CLOSED, ws->user_data);
    }
}

void ws_destroy(WebSocket* ws) {
    if (!ws) return;
    
    ws_close(ws);
    
    if (ws->rt) {
        if (ws->state_node) reactive_dispose(ws->rt, ws->state_node);
        if (ws->message_node) reactive_dispose(ws->rt, ws->message_node);
        if (ws->error_node) reactive_dispose(ws->rt, ws->error_node);
    }
    
    free((void*)ws->url);
    free(ws->recv_buffer);
    free(ws);
}

void ws_poll(WebSocket* ws) {
    if (!ws || ws->state != WS_STATE_OPEN || ws->fd < 0) return;
    
    #ifndef _WIN32
    struct pollfd pfd = { .fd = ws->fd, .events = POLLIN };
    
    if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
        uint8_t buffer[4096];
        int received = recv(ws->fd, buffer, sizeof(buffer), 0);
        
        if (received > 0) {
            /* Parse frame (simplificado) */
            uint8_t opcode = buffer[0] & 0x0F;
            size_t payload_len = buffer[1] & 0x7F;
            size_t header_len = 2;
            
            if (payload_len == 126) {
                payload_len = (buffer[2] << 8) | buffer[3];
                header_len = 4;
            }
            
            uint8_t* payload = buffer + header_len;
            
            ws->messages_received++;
            ws->bytes_received += payload_len;
            
            /* Atualiza nó reativo com a mensagem */
            if (ws->rt && ws->message_node) {
                /* Cria string com a mensagem */
                AsteronString* str = (AsteronString*)malloc(
                    sizeof(AsteronString) + payload_len + 1);
                str->header.obj_type = ASTERON_OBJ_STRING;
                str->header.flags = 0;
                str->header.ref_count = 1;
                str->header.next = NULL;
                str->length = payload_len;
                str->hash = 0;
                str->capacity = payload_len + 1;
                memcpy(str->chars, payload, payload_len);
                str->chars[payload_len] = '\0';
                
                /* MUTA O ESTADO - O runtime vai reagir */
                reactive_set(ws->rt, ws->message_node, 
                             ASTERON_PTR(str, ASTERON_VAL_STRING));
            }
            
            /* Callback legado */
            if (ws->on_message) {
                WebSocketMsgType type = (opcode == 1) ? WS_MSG_TEXT : WS_MSG_BINARY;
                ws->on_message(ws, type, payload, payload_len, ws->user_data);
            }
            
            /* Close frame */
            if (opcode == 0x08) {
                ws_close(ws);
            }
        } else if (received == 0) {
            /* Conexão fechada */
            ws_close(ws);
        }
    }
    #endif
}

/* =============================================================================
 * SERVIDOR
 * ============================================================================= */

WebSocketServer* ws_server_create(ReactiveRuntime* rt, int port) {
    WebSocketServer* server = (WebSocketServer*)calloc(1, sizeof(WebSocketServer));
    if (!server) return NULL;
    
    server->port = port;
    server->listen_fd = -1;
    server->rt = rt;
    
    server->client_capacity = 64;
    server->clients = (WebSocket**)calloc(server->client_capacity, sizeof(WebSocket*));
    
    if (rt) {
        server->clients_node = reactive_state(rt, "ws_server_clients", ASTERON_NUMBER(0));
    }
    
    return server;
}

AsteronResult ws_server_listen(WebSocketServer* server) {
    if (!server) return ASTERON_ERROR_INVALID;
    
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) return ASTERON_ERROR_NET;
    
    int opt = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(server->port);
    
    if (bind(server->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        #ifndef _WIN32
        close(server->listen_fd);
        #else
        closesocket(server->listen_fd);
        #endif
        server->listen_fd = -1;
        return ASTERON_ERROR_NET;
    }
    
    if (listen(server->listen_fd, 128) < 0) {
        #ifndef _WIN32
        close(server->listen_fd);
        #else
        closesocket(server->listen_fd);
        #endif
        server->listen_fd = -1;
        return ASTERON_ERROR_NET;
    }
    
    set_nonblocking(server->listen_fd);
    
    return ASTERON_OK;
}

WebSocket* ws_server_accept(WebSocketServer* server) {
    if (!server || server->listen_fd < 0) return NULL;
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = accept(server->listen_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) return NULL;
    
    /* Recebe handshake */
    char buffer[1024];
    recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    /* Envia resposta (simplificado) */
    const char* response = 
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
        "\r\n";
    send(client_fd, response, strlen(response), 0);
    
    /* Cria WebSocket para o cliente */
    WebSocket* ws = ws_create(server->rt, NULL);
    ws->fd = client_fd;
    ws->state = WS_STATE_OPEN;
    
    set_nonblocking(client_fd);
    
    /* Adiciona à lista */
    if (server->client_count >= server->client_capacity) {
        server->client_capacity *= 2;
        server->clients = (WebSocket**)realloc(server->clients,
            server->client_capacity * sizeof(WebSocket*));
    }
    server->clients[server->client_count++] = ws;
    
    /* Atualiza estado reativo */
    if (server->rt && server->clients_node) {
        reactive_set(server->rt, server->clients_node, 
                     ASTERON_NUMBER((double)server->client_count));
    }
    if (ws->rt && ws->state_node) {
        reactive_set(ws->rt, ws->state_node, ASTERON_NUMBER(WS_STATE_OPEN));
    }
    
    if (server->on_connect) {
        server->on_connect(ws, server->user_data);
    }
    
    return ws;
}

void ws_server_broadcast(WebSocketServer* server, const uint8_t* data, size_t len) {
    if (!server) return;
    
    for (uint32_t i = 0; i < server->client_count; i++) {
        ws_send(server->clients[i], data, len, WS_MSG_TEXT);
    }
}

void ws_server_poll(WebSocketServer* server) {
    if (!server) return;
    
    /* Aceita novas conexões */
    WebSocket* new_client;
    while ((new_client = ws_server_accept(server)) != NULL) {
        /* Cliente adicionado automaticamente */
    }
    
    /* Poll cada cliente */
    for (uint32_t i = 0; i < server->client_count; i++) {
        ws_poll(server->clients[i]);
        
        /* Remove desconectados */
        if (server->clients[i]->state == WS_STATE_CLOSED) {
            if (server->on_disconnect) {
                server->on_disconnect(server->clients[i], server->user_data);
            }
            ws_destroy(server->clients[i]);
            memmove(&server->clients[i], &server->clients[i + 1],
                    (server->client_count - i - 1) * sizeof(WebSocket*));
            server->client_count--;
            i--;
            
            /* Atualiza estado reativo */
            if (server->rt && server->clients_node) {
                reactive_set(server->rt, server->clients_node,
                             ASTERON_NUMBER((double)server->client_count));
            }
        }
    }
}

void ws_server_stop(WebSocketServer* server) {
    if (!server) return;
    
    /* Fecha todos os clientes */
    for (uint32_t i = 0; i < server->client_count; i++) {
        ws_close(server->clients[i]);
        ws_destroy(server->clients[i]);
    }
    server->client_count = 0;
    
    /* Fecha listener */
    if (server->listen_fd >= 0) {
        #ifndef _WIN32
        close(server->listen_fd);
        #else
        closesocket(server->listen_fd);
        #endif
        server->listen_fd = -1;
    }
}

void ws_server_destroy(WebSocketServer* server) {
    if (!server) return;
    
    ws_server_stop(server);
    
    if (server->rt && server->clients_node) {
        reactive_dispose(server->rt, server->clients_node);
    }
    
    free(server->clients);
    free(server);
}

/* =============================================================================
 * INTEGRAÇÃO REATIVA
 * ============================================================================= */

ReactiveNode* ws_state_node(WebSocket* ws) {
    return ws ? ws->state_node : NULL;
}

ReactiveNode* ws_message_node(WebSocket* ws) {
    return ws ? ws->message_node : NULL;
}

/* Helper para efeitos de WebSocket */
typedef struct {
    WebSocket* ws;
    ComputeFn user_handler;
    void* user_data;
} WsEffectData;

static AsteronValue ws_message_effect_compute(ReactiveRuntime* rt, ReactiveNode* node) {
    WsEffectData* data = (WsEffectData*)node->user_data;
    
    /* Lê mensagem (registra dependência automaticamente) */
    AsteronValue msg = reactive_get(rt, data->ws->message_node);
    
    /* Se há mensagem, chama handler */
    if (!ASTERON_IS_NIL(msg)) {
        if (data->user_handler) {
            return data->user_handler(rt, node);
        }
    }
    
    return ASTERON_NIL();
}

ReactiveNode* ws_on_message(ReactiveRuntime* rt, WebSocket* ws,
                             ComputeFn handler, void* user_data) {
    if (!rt || !ws) return NULL;
    
    WsEffectData* data = (WsEffectData*)malloc(sizeof(WsEffectData));
    data->ws = ws;
    data->user_handler = handler;
    data->user_data = user_data;
    
    char name[64];
    snprintf(name, sizeof(name), "ws_%u_on_message", ws->id);
    
    ReactiveNode* effect = reactive_effect(rt, name, 
                                            ws_message_effect_compute, data);
    
    /* Adiciona dependência explícita */
    reactive_add_dep(effect, ws->message_node);
    
    return effect;
}

typedef struct {
    WebSocket* ws;
    WebSocketState target_state;
    ComputeFn user_handler;
    void* user_data;
} WsStateEffectData;

static AsteronValue ws_state_effect_compute(ReactiveRuntime* rt, ReactiveNode* node) {
    WsStateEffectData* data = (WsStateEffectData*)node->user_data;
    
    /* Lê estado */
    AsteronValue state = reactive_get(rt, data->ws->state_node);
    
    /* Se é o estado alvo, dispara */
    if (ASTERON_IS_NUMBER(state) && 
        (WebSocketState)ASTERON_AS_NUMBER(state) == data->target_state) {
        if (data->user_handler) {
            return data->user_handler(rt, node);
        }
    }
    
    return ASTERON_NIL();
}

ReactiveNode* ws_on_state(ReactiveRuntime* rt, WebSocket* ws,
                           WebSocketState target_state,
                           ComputeFn handler, void* user_data) {
    if (!rt || !ws) return NULL;
    
    WsStateEffectData* data = (WsStateEffectData*)malloc(sizeof(WsStateEffectData));
    data->ws = ws;
    data->target_state = target_state;
    data->user_handler = handler;
    data->user_data = user_data;
    
    char name[64];
    snprintf(name, sizeof(name), "ws_%u_on_state_%d", ws->id, target_state);
    
    ReactiveNode* effect = reactive_effect(rt, name,
                                            ws_state_effect_compute, data);
    
    reactive_add_dep(effect, ws->state_node);
    
    return effect;
}

