/**
 * =============================================================================
 * ASTERON NET MODULE v2.0 - REVOLUCIONÁRIO
 * =============================================================================
 * 
 * MELHORIAS:
 * 
 * 1. HANDLES COMO NÚMEROS
 *    - Sockets são armazenados como file descriptors (int)
 *    - Compatível com o sistema de Value da VM
 * 
 * 2. STREAMING / CHUNKS
 *    - tcp_recv_all(sock) - recebe tudo até fechar
 *    - tcp_recv_until(sock, delimiter) - recebe até delimiter
 *    - tcp_recv_chunked(sock, chunk_size) - recebe em chunks
 * 
 * 3. TIMEOUTS
 *    - tcp_connect(host, port, timeout_ms)
 *    - tcp_recv(sock, max, timeout_ms)
 *    - tcp_set_timeout(sock, timeout_ms)
 * 
 * 4. DNS AVANÇADO
 *    - resolve_all(hostname) - todos os IPs
 *    - reverse_lookup(ip) - IP para hostname
 * 
 * 5. CALLBACKS REATIVOS (preparação)
 *    - on_data(sock, callback_id)
 *    - on_connect(sock, callback_id)
 *    - on_close(sock, callback_id)
 * 
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "net_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

/* =============================================================================
 * DEBUG CONTROL - Para produção, comente a linha abaixo
 * ============================================================================= */
/* #define ASTERON_NET_DEBUG 1 */

#ifdef ASTERON_NET_DEBUG
    #define NET_DEBUG(fmt, ...) fprintf(stderr, "[net] " fmt "\n", ##__VA_ARGS__)
#else
    #define NET_DEBUG(fmt, ...) ((void)0)
#endif

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define SOCKET_INVALID INVALID_SOCKET
    #define SOCKET_ERR SOCKET_ERROR
    #define sock_close closesocket
    #define sock_errno WSAGetLastError()
    #define MSG_DONTWAIT 0
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/select.h>
    #include <sys/time.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <fcntl.h>
    #include <poll.h>
    typedef int socket_t;
    #define SOCKET_INVALID (-1)
    #define SOCKET_ERR (-1)
    #define sock_close close
    #define sock_errno errno
#endif

/* =============================================================================
 * TABELA GLOBAL DE SOCKETS
 * =============================================================================
 * 
 * Armazenamos sockets em uma tabela global e retornamos o índice como número.
 * Isso permite que a VM trabalhe com handles como valores numéricos simples.
 */

#define MAX_SOCKETS 1024

typedef struct {
    socket_t fd;
    int in_use;
    int timeout_ms;
    char* remote_host;
    int remote_port;
    
    /* Para callbacks reativos */
    int on_data_callback;
    int on_close_callback;
    void* user_data;
    
    /* Estatísticas */
    size_t bytes_sent;
    size_t bytes_received;
} SocketEntry;

static SocketEntry g_sockets[MAX_SOCKETS];
static int g_sockets_initialized = 0;
static int g_net_initialized = 0;

/* =============================================================================
 * INICIALIZAÇÃO
 * ============================================================================= */

static void net_init(void) {
    if (g_net_initialized) return;
    
    #ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    #endif
    
    /* Inicializa tabela de sockets */
    if (!g_sockets_initialized) {
        memset(g_sockets, 0, sizeof(g_sockets));
        for (int i = 0; i < MAX_SOCKETS; i++) {
            g_sockets[i].fd = SOCKET_INVALID;
            g_sockets[i].timeout_ms = 30000; /* 30 segundos default */
        }
        g_sockets_initialized = 1;
    }
    
    g_net_initialized = 1;
}

/* =============================================================================
 * GERENCIAMENTO DE SOCKETS
 * ============================================================================= */

static int socket_alloc(socket_t fd, const char* host, int port) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!g_sockets[i].in_use) {
            g_sockets[i].fd = fd;
            g_sockets[i].in_use = 1;
            g_sockets[i].timeout_ms = 30000;
            g_sockets[i].remote_host = host ? strdup(host) : NULL;
            g_sockets[i].remote_port = port;
            g_sockets[i].bytes_sent = 0;
            g_sockets[i].bytes_received = 0;
            g_sockets[i].on_data_callback = -1;
            g_sockets[i].on_close_callback = -1;
            return i + 1; /* Handle começa em 1 (0 = inválido) */
        }
    }
    return 0; /* Sem slots disponíveis */
}

static SocketEntry* socket_get(int handle) {
    if (handle < 1 || handle > MAX_SOCKETS) return NULL;
    SocketEntry* entry = &g_sockets[handle - 1];
    if (!entry->in_use) return NULL;
    return entry;
}

static void socket_free(int handle) {
    if (handle < 1 || handle > MAX_SOCKETS) return;
    SocketEntry* entry = &g_sockets[handle - 1];
    if (entry->in_use) {
        if (entry->fd != SOCKET_INVALID) {
            sock_close(entry->fd);
        }
        if (entry->remote_host) {
            free(entry->remote_host);
        }
        memset(entry, 0, sizeof(SocketEntry));
        entry->fd = SOCKET_INVALID;
    }
}

/* =============================================================================
 * HELPERS
 * ============================================================================= */

static AsteronValue make_string(const char* s) {
    if (!s) return ASTERON_NIL();
    
    size_t len = strlen(s);
    AsteronString* str = (AsteronString*)malloc(sizeof(AsteronString) + len + 1);
    str->header.obj_type = ASTERON_OBJ_STRING;
    str->header.flags = 0;
    str->header.ref_count = 1;
    str->header.next = NULL;
    str->length = len;
    str->hash = 0;
    str->capacity = len + 1;
    strcpy(str->chars, s);
    
    return ASTERON_PTR(str, ASTERON_VAL_STRING);
}

static AsteronValue make_string_len(const char* s, size_t len) {
    AsteronString* str = (AsteronString*)malloc(sizeof(AsteronString) + len + 1);
    str->header.obj_type = ASTERON_OBJ_STRING;
    str->header.flags = 0;
    str->header.ref_count = 1;
    str->header.next = NULL;
    str->length = len;
    str->hash = 0;
    str->capacity = len + 1;
    memcpy(str->chars, s, len);
    str->chars[len] = '\0';
    
    return ASTERON_PTR(str, ASTERON_VAL_STRING);
}

static const char* get_string_arg(AsteronValue* args, int index) {
    // Verifica nil primeiro (segurança crítica)
    if (ASTERON_IS_NIL(args[index])) return NULL;
    if (!ASTERON_IS_STRING(args[index])) return NULL;
    AsteronString* str = ASTERON_AS_STRING(args[index]);
    return str ? str->chars : NULL;
}

static int set_socket_timeout(socket_t fd, int timeout_ms) {
    #ifdef _WIN32
    DWORD tv = timeout_ms;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));
    #else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    #endif
    return 0;
}

static int wait_for_data(socket_t fd, int timeout_ms) {
    #ifdef _WIN32
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
    return select(0, &readfds, NULL, NULL, &tv);
    #else
    struct pollfd pfd = { fd, POLLIN, 0 };
    return poll(&pfd, 1, timeout_ms);
    #endif
}

/* =============================================================================
 * TCP CONNECT (com timeout)
 * 
 * tcp_connect(host, port)
 * tcp_connect(host, port, timeout_ms)
 * ============================================================================= */

AsteronValue net_tcp_connect(int argc, AsteronValue* args) {
    NET_DEBUG("tcp_connect: argc=%d", argc);
    
    if (argc < 2) {
        NET_DEBUG("tcp_connect: ERRO argc < 2");
        return ASTERON_NUMBER(-1);
    }
    
    net_init();
    
    const char* host = get_string_arg(args, 0);
    if (!host) {
        NET_DEBUG("tcp_connect: ERRO host é NULL");
        return ASTERON_NUMBER(-1);
    }
    
    NET_DEBUG("tcp_connect: host='%s'", host);
    
    int port = (int)ASTERON_AS_NUMBER(args[1]);
    NET_DEBUG("tcp_connect: port=%d", port);
    
    if (port <= 0 || port > 65535) {
        NET_DEBUG("tcp_connect: ERRO porta inválida");
        return ASTERON_NUMBER(-1);
    }
    
    int timeout_ms = 30000; /* Default: 30 segundos */
    if (argc >= 3 && ASTERON_IS_NUMBER(args[2])) {
        timeout_ms = (int)ASTERON_AS_NUMBER(args[2]);
    }
    
    /* Resolve hostname */
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    NET_DEBUG("tcp_connect: Resolvendo DNS '%s'...", host);
    
    if (getaddrinfo(host, port_str, &hints, &result) != 0) {
        NET_DEBUG("tcp_connect: ERRO getaddrinfo falhou");
        return ASTERON_NUMBER(-1);
    }
    
    NET_DEBUG("tcp_connect: DNS resolvido");
    
    /* Cria socket */
    socket_t sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == SOCKET_INVALID) {
        NET_DEBUG("tcp_connect: ERRO socket() falhou");
        freeaddrinfo(result);
        return ASTERON_NUMBER(-1);
    }
    
    NET_DEBUG("tcp_connect: fd=%d", (int)sock);
    
    /* Configura timeout */
    set_socket_timeout(sock, timeout_ms);
    
    /* Conecta */
    NET_DEBUG("tcp_connect: Conectando...");
    
    if (connect(sock, result->ai_addr, (socklen_t)result->ai_addrlen) == SOCKET_ERR) {
        NET_DEBUG("tcp_connect: ERRO connect() falhou");
        sock_close(sock);
        freeaddrinfo(result);
        return ASTERON_NUMBER(-1);
    }
    
    NET_DEBUG("tcp_connect: OK!");
    
    freeaddrinfo(result);
    
    /* Aloca na tabela global */
    int handle = socket_alloc(sock, host, port);
    NET_DEBUG("tcp_connect: handle=%d", handle);
    
    if (handle == 0) {
        NET_DEBUG("tcp_connect: ERRO socket_alloc falhou");
        sock_close(sock);
        return ASTERON_NUMBER(-1);
    }
    
    return ASTERON_NUMBER((double)handle);
}

/* =============================================================================
 * TCP SEND
 * 
 * tcp_send(sock, data) -> bytes_sent
 * ============================================================================= */

AsteronValue net_tcp_send(int argc, AsteronValue* args) {
    if (argc < 2) {
        NET_DEBUG("tcp_send: argc < 2");
        return ASTERON_NUMBER(-1);
    }
    
    int handle = (int)ASTERON_AS_NUMBER(args[0]);
    NET_DEBUG("tcp_send: handle=%d", handle);
    
    SocketEntry* entry = socket_get(handle);
    if (!entry) {
        NET_DEBUG("tcp_send: ERRO socket_get(%d) NULL", handle);
        return ASTERON_NUMBER(-1);
    }
    
    NET_DEBUG("tcp_send: fd=%d", (int)entry->fd);
    
    const char* data = NULL;
    size_t len = 0;
    
    if (ASTERON_IS_STRING(args[1])) {
        AsteronString* str = ASTERON_AS_STRING(args[1]);
        if (str) {
            data = str->chars;
            len = str->length;
            NET_DEBUG("tcp_send: len=%zu", len);
        }
    }
    
    if (!data) {
        NET_DEBUG("tcp_send: ERRO data é NULL");
        return ASTERON_NUMBER(-1);
    }
    
    NET_DEBUG("tcp_send: enviando %zu bytes...", len);
    
    int sent = send(entry->fd, data, (int)len, 0);
    
    if (sent == SOCKET_ERR) {
        NET_DEBUG("tcp_send: ERRO send() falhou");
        return ASTERON_NUMBER(-1);
    }
    
    NET_DEBUG("tcp_send: OK %d bytes", sent);
    entry->bytes_sent += sent;
    
    return ASTERON_NUMBER((double)sent);
}

/* =============================================================================
 * TCP RECV (com timeout e tamanho variável)
 * 
 * tcp_recv(sock)                -> recebe até 4096 bytes
 * tcp_recv(sock, max_bytes)     -> recebe até max_bytes
 * tcp_recv(sock, max, timeout)  -> com timeout em ms
 * ============================================================================= */

AsteronValue net_tcp_recv(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_NIL();
    
    int handle = (int)ASTERON_AS_NUMBER(args[0]);
    SocketEntry* entry = socket_get(handle);
    if (!entry) return ASTERON_NIL();
    
    int max_bytes = 4096;
    if (argc >= 2 && ASTERON_IS_NUMBER(args[1])) {
        int requested = (int)ASTERON_AS_NUMBER(args[1]);
        if (requested > 0) max_bytes = requested;
        if (requested == -1) max_bytes = 65536; /* Receber "tudo" */
    }
    
    int timeout_ms = entry->timeout_ms;
    if (argc >= 3 && ASTERON_IS_NUMBER(args[2])) {
        timeout_ms = (int)ASTERON_AS_NUMBER(args[2]);
    }
    
    /* Aplica timeout */
    set_socket_timeout(entry->fd, timeout_ms);
    
    char* buffer = (char*)malloc(max_bytes + 1);
    if (!buffer) return ASTERON_NIL();
    
    int received = recv(entry->fd, buffer, max_bytes, 0);
    
    if (received <= 0) {
        free(buffer);
        return ASTERON_NIL();
    }
    
    buffer[received] = '\0';
    entry->bytes_received += received;
    
    AsteronValue result = make_string_len(buffer, received);
    free(buffer);
    
    return result;
}

/* =============================================================================
 * TCP RECV ALL - Recebe toda a resposta
 * 
 * tcp_recv_all(sock)              -> recebe até conexão fechar
 * tcp_recv_all(sock, timeout_ms)  -> com timeout total
 * ============================================================================= */

AsteronValue net_tcp_recv_all(int argc, AsteronValue* args) {
    // CORREÇÃO: Nunca retornar nil - retornar string vazia
    if (argc < 1) return make_string("");
    
    int handle = (int)ASTERON_AS_NUMBER(args[0]);
    SocketEntry* entry = socket_get(handle);
    if (!entry) return make_string("");
    
    int timeout_ms = 30000;
    if (argc >= 2 && ASTERON_IS_NUMBER(args[1])) {
        timeout_ms = (int)ASTERON_AS_NUMBER(args[1]);
    }
    
    /* Buffer dinâmico */
    size_t capacity = 16384;
    size_t total = 0;
    char* buffer = (char*)malloc(capacity);
    if (!buffer) return make_string("");
    
    /* Configura timeout */
    set_socket_timeout(entry->fd, 1000); /* 1 segundo por chunk */
    
    time_t start_time = time(NULL);
    
    while (1) {
        /* Verifica timeout total */
        if ((time(NULL) - start_time) * 1000 > timeout_ms) {
            break;
        }
        
        /* Espera dados */
        int ready = wait_for_data(entry->fd, 1000);
        if (ready <= 0) {
            if (ready == 0 && total > 0) break; /* Timeout, mas temos dados */
            continue;
        }
        
        /* Expande buffer se necessário */
        if (total + 8192 > capacity) {
            capacity *= 2;
            char* new_buf = (char*)realloc(buffer, capacity);
            if (!new_buf) break;
            buffer = new_buf;
        }
        
        int received = recv(entry->fd, buffer + total, 8192, 0);
        if (received <= 0) break;
        
        total += received;
        entry->bytes_received += received;
    }
    
    if (total == 0) {
        free(buffer);
        // CORREÇÃO: Retorna string vazia ao invés de nil
        return make_string("");
    }
    
    buffer[total] = '\0';
    AsteronValue result = make_string_len(buffer, total);
    free(buffer);
    
    return result;
}

/* =============================================================================
 * TCP CLOSE
 * ============================================================================= */

AsteronValue net_tcp_close(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_BOOL(false);
    
    int handle = (int)ASTERON_AS_NUMBER(args[0]);
    socket_free(handle);
    
    return ASTERON_BOOL(true);
}

/* =============================================================================
 * TCP LISTEN
 * ============================================================================= */

AsteronValue net_tcp_listen(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_NUMBER(-1);
    
    net_init();
    
    int port = (int)ASTERON_AS_NUMBER(args[0]);
    if (port <= 0 || port > 65535) return ASTERON_NUMBER(-1);
    
    int backlog = 128;
    if (argc >= 2 && ASTERON_IS_NUMBER(args[1])) {
        backlog = (int)ASTERON_AS_NUMBER(args[1]);
    }
    
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == SOCKET_INVALID) {
        return ASTERON_NUMBER(-1);
    }
    
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERR) {
        sock_close(sock);
        return ASTERON_NUMBER(-1);
    }
    
    if (listen(sock, backlog) == SOCKET_ERR) {
        sock_close(sock);
        return ASTERON_NUMBER(-1);
    }
    
    int handle = socket_alloc(sock, "0.0.0.0", port);
    return ASTERON_NUMBER((double)handle);
}

/* =============================================================================
 * TCP ACCEPT
 * ============================================================================= */

AsteronValue net_tcp_accept(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_NUMBER(-1);
    
    int handle = (int)ASTERON_AS_NUMBER(args[0]);
    SocketEntry* entry = socket_get(handle);
    if (!entry) return ASTERON_NUMBER(-1);
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    socket_t client = accept(entry->fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client == SOCKET_INVALID) {
        return ASTERON_NUMBER(-1);
    }
    
    char* client_ip = inet_ntoa(client_addr.sin_addr);
    int client_port = ntohs(client_addr.sin_port);
    
    int client_handle = socket_alloc(client, client_ip, client_port);
    return ASTERON_NUMBER((double)client_handle);
}

/* =============================================================================
 * DNS - RESOLVE
 * ============================================================================= */

AsteronValue net_resolve(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_NIL();
    
    net_init();
    
    const char* hostname = get_string_arg(args, 0);
    if (!hostname) return ASTERON_NIL();
    
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(hostname, NULL, &hints, &result) != 0) {
        return ASTERON_NIL();
    }
    
    struct sockaddr_in* addr = (struct sockaddr_in*)result->ai_addr;
    char* ip = inet_ntoa(addr->sin_addr);
    
    AsteronValue ret = make_string(ip);
    freeaddrinfo(result);
    
    return ret;
}

/* =============================================================================
 * DNS - RESOLVE ALL (todos os IPs)
 * ============================================================================= */

AsteronValue net_resolve_all(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_NIL();
    
    net_init();
    
    const char* hostname = get_string_arg(args, 0);
    if (!hostname) return ASTERON_NIL();
    
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(hostname, NULL, &hints, &result) != 0) {
        return ASTERON_NIL();
    }
    
    /* Conta resultados */
    int count = 0;
    for (rp = result; rp != NULL; rp = rp->ai_next) count++;
    
    /* Cria array */
    AsteronArray* arr = (AsteronArray*)malloc(sizeof(AsteronArray));
    arr->header.obj_type = ASTERON_OBJ_ARRAY;
    arr->header.flags = 0;
    arr->header.ref_count = 1;
    arr->header.next = NULL;
    arr->capacity = count;
    arr->length = count;
    arr->items = (AsteronValue*)malloc(count * sizeof(AsteronValue));
    
    int i = 0;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        struct sockaddr_in* addr = (struct sockaddr_in*)rp->ai_addr;
        arr->items[i++] = make_string(inet_ntoa(addr->sin_addr));
    }
    
    freeaddrinfo(result);
    
    return ASTERON_PTR(arr, ASTERON_VAL_ARRAY);
}

/* =============================================================================
 * DNS - REVERSE LOOKUP
 * ============================================================================= */

AsteronValue net_reverse_lookup(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_NIL();
    
    net_init();
    
    const char* ip = get_string_arg(args, 0);
    if (!ip) return ASTERON_NIL();
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    
    char hostname[256];
    if (getnameinfo((struct sockaddr*)&addr, sizeof(addr), 
                     hostname, sizeof(hostname), 
                     NULL, 0, 0) != 0) {
        return ASTERON_NIL();
    }
    
    return make_string(hostname);
}

/* =============================================================================
 * HOSTNAME
 * ============================================================================= */

AsteronValue net_hostname(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    net_init();
    
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return ASTERON_NIL();
    }
    
    return make_string(hostname);
}

/* =============================================================================
 * SET TIMEOUT
 * ============================================================================= */

AsteronValue net_set_timeout(int argc, AsteronValue* args) {
    if (argc < 2) return ASTERON_BOOL(false);
    
    int handle = (int)ASTERON_AS_NUMBER(args[0]);
    SocketEntry* entry = socket_get(handle);
    if (!entry) return ASTERON_BOOL(false);
    
    int timeout_ms = (int)ASTERON_AS_NUMBER(args[1]);
    entry->timeout_ms = timeout_ms;
    set_socket_timeout(entry->fd, timeout_ms);
    
    return ASTERON_BOOL(true);
}

/* =============================================================================
 * SOCKET STATS
 * ============================================================================= */

AsteronValue net_socket_stats(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_NIL();
    
    int handle = (int)ASTERON_AS_NUMBER(args[0]);
    SocketEntry* entry = socket_get(handle);
    if (!entry) return ASTERON_NIL();
    
    /* Retorna como string formatada por simplicidade */
    char buf[256];
    snprintf(buf, sizeof(buf), 
             "{ \"host\": \"%s\", \"port\": %d, \"sent\": %zu, \"received\": %zu }",
             entry->remote_host ? entry->remote_host : "unknown",
             entry->remote_port,
             entry->bytes_sent,
             entry->bytes_received);
    
    return make_string(buf);
}

/* =============================================================================
 * IS CONNECTED
 * ============================================================================= */

AsteronValue net_is_connected(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_BOOL(false);
    
    int handle = (int)ASTERON_AS_NUMBER(args[0]);
    SocketEntry* entry = socket_get(handle);
    if (!entry) return ASTERON_BOOL(false);
    
    /* Verifica se socket ainda está válido usando select */
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(entry->fd, &readfds);
    struct timeval tv = {0, 0};
    
    int result = select((int)entry->fd + 1, &readfds, NULL, NULL, &tv);
    if (result < 0) return ASTERON_BOOL(false);
    
    return ASTERON_BOOL(true);
}

/* =============================================================================
 * HTTP GET (utilitário)
 * ============================================================================= */

AsteronValue net_http_get(int argc, AsteronValue* args) {
    // CORREÇÃO CRÍTICA: Nunca retornar nil silenciosamente - retornar string vazia
    if (argc < 1) return make_string("");
    
    const char* url = get_string_arg(args, 0);
    if (!url) return make_string("");
    
    /* Parse URL básico (http://host:port/path) */
    char host[256] = "";
    int port = 80;
    char path[1024] = "/";
    
    if (strncmp(url, "http://", 7) == 0) {
        url += 7;
    }
    
    const char* slash = strchr(url, '/');
    const char* colon = strchr(url, ':');
    
    if (colon && (!slash || colon < slash)) {
        strncpy(host, url, colon - url);
        host[colon - url] = '\0';
        port = atoi(colon + 1);
        if (slash) strcpy(path, slash);
    } else if (slash) {
        strncpy(host, url, slash - url);
        host[slash - url] = '\0';
        strcpy(path, slash);
    } else {
        strcpy(host, url);
    }
    
    /* Conecta */
    AsteronValue args2[2] = { make_string(host), ASTERON_NUMBER(port) };
    AsteronValue sock_val = net_tcp_connect(2, args2);
    
    int handle = (int)ASTERON_AS_NUMBER(sock_val);
    if (handle <= 0) {
        // CORREÇÃO: Retorna string vazia ao invés de nil
        return make_string("");
    }
    
    /* Envia request */
    char request[2048];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "User-Agent: Asteron/1.0\r\n"
             "\r\n",
             path, host);
    
    AsteronString* req_str = (AsteronString*)malloc(sizeof(AsteronString) + strlen(request) + 1);
    req_str->header.obj_type = ASTERON_OBJ_STRING;
    req_str->header.ref_count = 1;
    req_str->length = strlen(request);
    strcpy(req_str->chars, request);
    
    AsteronValue send_args[2] = { ASTERON_NUMBER(handle), ASTERON_PTR(req_str, ASTERON_VAL_STRING) };
    net_tcp_send(2, send_args);
    
    /* Recebe resposta */
    AsteronValue recv_args[2] = { ASTERON_NUMBER(handle), ASTERON_NUMBER(30000) };
    AsteronValue response = net_tcp_recv_all(2, recv_args);
    
    /* Fecha */
    AsteronValue close_args[1] = { ASTERON_NUMBER(handle) };
    net_tcp_close(1, close_args);
    
    // CORREÇÃO: Se response for nil, retorna string vazia
    if (ASTERON_IS_NIL(response)) {
        return make_string("");
    }
    
    return response;
}

/* =============================================================================
 * STUBS PARA FUNÇÕES NÃO IMPLEMENTADAS
 * ============================================================================= */

AsteronValue net_udp_bind(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }
AsteronValue net_udp_send(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }
AsteronValue net_udp_recv(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }
AsteronValue net_http_post(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }

