/**
 * =============================================================================
 * ASTERON TLS/HTTPS MODULE - Implementação
 * =============================================================================
 * 
 * Implementação simplificada de TLS usando OpenSSL (se disponível).
 * Fallback para HTTP sem TLS quando OpenSSL não está presente.
 * 
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "tls_module.h"
#include "net_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* =============================================================================
 * DETECÇÃO DE OPENSSL
 * ============================================================================= */

/* Tenta incluir OpenSSL se disponível */
#ifdef ASTERON_USE_OPENSSL
    #include <openssl/ssl.h>
    #include <openssl/err.h>
    #include <openssl/bio.h>
    #define TLS_AVAILABLE 1
#else
    /* Stub definitions quando OpenSSL não está disponível */
    #define TLS_AVAILABLE 0
    typedef void SSL;
    typedef void SSL_CTX;
#endif

/* =============================================================================
 * INCLUDES DE REDE
 * ============================================================================= */

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define sock_close closesocket
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    typedef int socket_t;
    #define sock_close close
    #define INVALID_SOCKET (-1)
#endif

/* =============================================================================
 * ESTRUTURAS INTERNAS
 * ============================================================================= */

#define MAX_TLS_CONNECTIONS 256

typedef struct {
    int in_use;
    socket_t fd;
    SSL* ssl;
    SSL_CTX* ctx;
    char* host;
    int port;
    int is_secure;
} TLSConnection;

static TLSConnection g_tls_connections[MAX_TLS_CONNECTIONS];
static int g_tls_initialized = 0;

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

static int tls_alloc(socket_t fd, SSL* ssl, SSL_CTX* ctx, const char* host, int port) {
    for (int i = 0; i < MAX_TLS_CONNECTIONS; i++) {
        if (!g_tls_connections[i].in_use) {
            g_tls_connections[i].in_use = 1;
            g_tls_connections[i].fd = fd;
            g_tls_connections[i].ssl = ssl;
            g_tls_connections[i].ctx = ctx;
            g_tls_connections[i].host = host ? strdup(host) : NULL;
            g_tls_connections[i].port = port;
            g_tls_connections[i].is_secure = (ssl != NULL);
            return i + 1; /* Handle começa em 1 */
        }
    }
    return 0;
}

static TLSConnection* tls_get(int handle) {
    if (handle < 1 || handle > MAX_TLS_CONNECTIONS) return NULL;
    TLSConnection* conn = &g_tls_connections[handle - 1];
    if (!conn->in_use) return NULL;
    return conn;
}

static void tls_free(int handle) {
    if (handle < 1 || handle > MAX_TLS_CONNECTIONS) return;
    TLSConnection* conn = &g_tls_connections[handle - 1];
    
    if (conn->in_use) {
        #if TLS_AVAILABLE
        if (conn->ssl) {
            SSL_shutdown(conn->ssl);
            SSL_free(conn->ssl);
        }
        if (conn->ctx) {
            SSL_CTX_free(conn->ctx);
        }
        #endif
        
        if (conn->fd != INVALID_SOCKET) {
            sock_close(conn->fd);
        }
        if (conn->host) {
            free(conn->host);
        }
        
        memset(conn, 0, sizeof(TLSConnection));
    }
}

/* =============================================================================
 * INICIALIZAÇÃO
 * ============================================================================= */

int tls_init(void) {
    if (g_tls_initialized) return 1;
    
    #if TLS_AVAILABLE
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    #endif
    
    memset(g_tls_connections, 0, sizeof(g_tls_connections));
    g_tls_initialized = 1;
    
    return 1;
}

void tls_cleanup(void) {
    if (!g_tls_initialized) return;
    
    /* Fecha todas as conexões */
    for (int i = 1; i <= MAX_TLS_CONNECTIONS; i++) {
        tls_free(i);
    }
    
    #if TLS_AVAILABLE
    EVP_cleanup();
    ERR_free_strings();
    #endif
    
    g_tls_initialized = 0;
}

/* =============================================================================
 * TLS CONNECT
 * ============================================================================= */

AsteronValue tls_connect(int argc, AsteronValue* args) {
    if (argc < 2) return ASTERON_NUMBER(-1);
    
    tls_init();
    
    const char* host = get_string_arg(args, 0);
    if (!host) return ASTERON_NUMBER(-1);
    
    int port = (int)ASTERON_AS_NUMBER(args[1]);
    if (port <= 0 || port > 65535) return ASTERON_NUMBER(-1);
    
    /* Resolve hostname */
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    if (getaddrinfo(host, port_str, &hints, &result) != 0) {
        return ASTERON_NUMBER(-1);
    }
    
    /* Cria socket */
    socket_t sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET) {
        freeaddrinfo(result);
        return ASTERON_NUMBER(-1);
    }
    
    /* Conecta */
    if (connect(sock, result->ai_addr, (socklen_t)result->ai_addrlen) < 0) {
        sock_close(sock);
        freeaddrinfo(result);
        return ASTERON_NUMBER(-1);
    }
    
    freeaddrinfo(result);
    
    SSL* ssl = NULL;
    SSL_CTX* ctx = NULL;
    
    #if TLS_AVAILABLE
    /* Cria contexto SSL */
    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        sock_close(sock);
        return ASTERON_NUMBER(-1);
    }
    
    /* Cria objeto SSL */
    ssl = SSL_new(ctx);
    if (!ssl) {
        SSL_CTX_free(ctx);
        sock_close(sock);
        return ASTERON_NUMBER(-1);
    }
    
    /* Associa socket */
    SSL_set_fd(ssl, sock);
    SSL_set_tlsext_host_name(ssl, host);
    
    /* Handshake TLS */
    if (SSL_connect(ssl) <= 0) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        sock_close(sock);
        return ASTERON_NUMBER(-1);
    }
    #endif
    
    int handle = tls_alloc(sock, ssl, ctx, host, port);
    if (handle == 0) {
        #if TLS_AVAILABLE
        if (ssl) SSL_free(ssl);
        if (ctx) SSL_CTX_free(ctx);
        #endif
        sock_close(sock);
        return ASTERON_NUMBER(-1);
    }
    
    return ASTERON_NUMBER((double)handle);
}

/* =============================================================================
 * TLS SEND
 * ============================================================================= */

AsteronValue tls_send(int argc, AsteronValue* args) {
    if (argc < 2) return ASTERON_NUMBER(-1);
    
    int handle = (int)ASTERON_AS_NUMBER(args[0]);
    TLSConnection* conn = tls_get(handle);
    if (!conn) return ASTERON_NUMBER(-1);
    
    const char* data = NULL;
    size_t len = 0;
    
    if (ASTERON_IS_STRING(args[1])) {
        AsteronString* str = ASTERON_AS_STRING(args[1]);
        if (str) {
            data = str->chars;
            len = str->length;
        }
    }
    
    if (!data) return ASTERON_NUMBER(-1);
    
    int sent;
    #if TLS_AVAILABLE
    if (conn->ssl) {
        sent = SSL_write(conn->ssl, data, (int)len);
    } else {
        sent = send(conn->fd, data, (int)len, 0);
    }
    #else
    sent = send(conn->fd, data, (int)len, 0);
    #endif
    
    return ASTERON_NUMBER((double)sent);
}

/* =============================================================================
 * TLS RECV
 * ============================================================================= */

AsteronValue tls_recv(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_NIL();
    
    int handle = (int)ASTERON_AS_NUMBER(args[0]);
    TLSConnection* conn = tls_get(handle);
    if (!conn) return ASTERON_NIL();
    
    int max_bytes = 4096;
    if (argc >= 2 && ASTERON_IS_NUMBER(args[1])) {
        max_bytes = (int)ASTERON_AS_NUMBER(args[1]);
    }
    
    char* buffer = (char*)malloc(max_bytes + 1);
    if (!buffer) return ASTERON_NIL();
    
    int received;
    #if TLS_AVAILABLE
    if (conn->ssl) {
        received = SSL_read(conn->ssl, buffer, max_bytes);
    } else {
        received = recv(conn->fd, buffer, max_bytes, 0);
    }
    #else
    received = recv(conn->fd, buffer, max_bytes, 0);
    #endif
    
    if (received <= 0) {
        free(buffer);
        return ASTERON_NIL();
    }
    
    buffer[received] = '\0';
    AsteronValue result = make_string_len(buffer, received);
    free(buffer);
    
    return result;
}

/* =============================================================================
 * TLS CLOSE
 * ============================================================================= */

AsteronValue tls_close(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_BOOL(false);
    
    int handle = (int)ASTERON_AS_NUMBER(args[0]);
    tls_free(handle);
    
    return ASTERON_BOOL(true);
}

/* =============================================================================
 * HTTPS GET
 * ============================================================================= */

AsteronValue https_get(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_NIL();
    
    const char* url = get_string_arg(args, 0);
    if (!url) return ASTERON_NIL();
    
    /* Parse URL (https://host:port/path) */
    char host[256] = "";
    int port = 443;
    char path[1024] = "/";
    int use_tls = 1;
    
    if (strncmp(url, "https://", 8) == 0) {
        url += 8;
        port = 443;
        use_tls = 1;
    } else if (strncmp(url, "http://", 7) == 0) {
        url += 7;
        port = 80;
        use_tls = 0;
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
    
    int handle;
    
    #if TLS_AVAILABLE
    if (use_tls) {
        /* Usa TLS */
        AsteronValue args2[2] = { make_string(host), ASTERON_NUMBER(port) };
        AsteronValue sock_val = tls_connect(2, args2);
        handle = (int)ASTERON_AS_NUMBER(sock_val);
    } else {
        /* Usa HTTP simples via net module */
        AsteronValue args2[2] = { make_string(host), ASTERON_NUMBER(port) };
        AsteronValue sock_val = net_tcp_connect(2, args2);
        handle = (int)ASTERON_AS_NUMBER(sock_val);
    }
    #else
    /* Sem TLS, sempre usa HTTP */
    if (use_tls) {
        /* Retorna erro - TLS não disponível */
        return make_string("ERROR: TLS not available. Compile with -DASTERON_USE_OPENSSL");
    }
    AsteronValue args2[2] = { make_string(host), ASTERON_NUMBER(port) };
    AsteronValue sock_val = net_tcp_connect(2, args2);
    handle = (int)ASTERON_AS_NUMBER(sock_val);
    #endif
    
    if (handle <= 0) return ASTERON_NIL();
    
    /* Envia request HTTP */
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
    
    #if TLS_AVAILABLE
    if (use_tls) {
        tls_send(2, send_args);
    } else {
        net_tcp_send(2, send_args);
    }
    #else
    net_tcp_send(2, send_args);
    #endif
    
    /* Recebe resposta */
    size_t total = 0;
    size_t capacity = 16384;
    char* buffer = (char*)malloc(capacity);
    
    while (1) {
        if (total + 4096 > capacity) {
            capacity *= 2;
            buffer = (char*)realloc(buffer, capacity);
        }
        
        AsteronValue recv_args[2] = { ASTERON_NUMBER(handle), ASTERON_NUMBER(4096) };
        AsteronValue chunk;
        
        #if TLS_AVAILABLE
        if (use_tls) {
            chunk = tls_recv(2, recv_args);
        } else {
            chunk = net_tcp_recv(2, recv_args);
        }
        #else
        chunk = net_tcp_recv(2, recv_args);
        #endif
        
        if (ASTERON_IS_NIL(chunk)) break;
        
        if (ASTERON_IS_STRING(chunk)) {
            AsteronString* s = ASTERON_AS_STRING(chunk);
            if (s && s->length > 0) {
                memcpy(buffer + total, s->chars, s->length);
                total += s->length;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    
    /* Fecha conexão */
    AsteronValue close_args[1] = { ASTERON_NUMBER(handle) };
    #if TLS_AVAILABLE
    if (use_tls) {
        tls_close(1, close_args);
    } else {
        net_tcp_close(1, close_args);
    }
    #else
    net_tcp_close(1, close_args);
    #endif
    
    if (total == 0) {
        free(buffer);
        return ASTERON_NIL();
    }
    
    buffer[total] = '\0';
    AsteronValue result = make_string_len(buffer, total);
    free(buffer);
    
    return result;
}

/* =============================================================================
 * HTTPS POST
 * ============================================================================= */

AsteronValue https_post(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    /* TODO: Implementar POST */
    return make_string("TODO: HTTPS POST not implemented yet");
}

/* =============================================================================
 * STATUS TLS
 * ============================================================================= */

AsteronValue tls_available(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    #if TLS_AVAILABLE
    return ASTERON_BOOL(true);
    #else
    return ASTERON_BOOL(false);
    #endif
}

AsteronValue tls_version(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    #if TLS_AVAILABLE
    return make_string(SSLeay_version(SSLEAY_VERSION));
    #else
    return make_string("TLS not available - compile with -DASTERON_USE_OPENSSL");
    #endif
}

