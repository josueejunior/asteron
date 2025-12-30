/*
 * Asteron Runtime - (C) 2024 Asteron Contributors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * =============================================================================
 * ASTERON WASM SERVER v1.0
 * =============================================================================
 * 
 * Servidor HTTP que fornece:
 * - Arquivos WebAssembly (asteron.wasm, asteron.js)
 * - Frontend React/Next.js (HTML, CSS, JS)
 * - API para compilação em tempo real
 * 
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

// =============================================================================
// CONFIGURAÇÃO
// =============================================================================

#define SERVER_PORT 8080
#define MAX_CONNECTIONS 100
#define BUFFER_SIZE 8192
#define PUBLIC_DIR "public"
#define FRONTEND_DIR "frontend"

// =============================================================================
// ESTRUTURAS
// =============================================================================

typedef struct {
    int socket;
    struct sockaddr_in address;
} Server;

// =============================================================================
// UTILITÁRIOS
// =============================================================================

static const char* get_mime_type(const char* path) {
    const char* ext = strrchr(path, '.');
    if (ext == NULL) return "application/octet-stream";
    
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".wasm") == 0) return "application/wasm";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    
    return "application/octet-stream";
}

static char* read_file_content(const char* path, size_t* size) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) return NULL;
    
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* content = (char*)malloc(*size + 1);
    if (content == NULL) {
        fclose(file);
        return NULL;
    }
    
    size_t read_size = fread(content, 1, *size, file);
    (void)read_size; // Suprime warning
    content[*size] = '\0';
    
    fclose(file);
    return content;
}

static void send_response(int client_socket, int status_code, 
                          const char* content_type, const char* body, size_t body_size) {
    char response[BUFFER_SIZE];
    int len = snprintf(response, sizeof(response),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, content_type, body_size);
    
    send(client_socket, response, len, 0);
    send(client_socket, body, body_size, 0);
}

static void send_file(int client_socket, const char* file_path) {
    size_t file_size;
    char* content = read_file_content(file_path, &file_size);
    
    if (content == NULL) {
        const char* not_found = "404 Not Found";
        send_response(client_socket, 404, "text/plain", not_found, strlen(not_found));
        return;
    }
    
    const char* mime_type = get_mime_type(file_path);
    send_response(client_socket, 200, mime_type, content, file_size);
    
    free(content);
}

// =============================================================================
// ROTEAMENTO
// =============================================================================

static void handle_request(int client_socket, const char* method, const char* path) {
    printf("[Server] %s %s\n", method, path);
    
    // CORS preflight
    if (strcmp(method, "OPTIONS") == 0) {
        send_response(client_socket, 200, "text/plain", "", 0);
        return;
    }
    
    // API endpoints
    if (strncmp(path, "/api/", 5) == 0) {
        // API de compilação (em produção, integraria com Wasm)
        if (strcmp(path, "/api/compile") == 0 && strcmp(method, "POST") == 0) {
            // Por enquanto, retorna JSON vazio
            const char* response = "{\"status\":\"ok\",\"message\":\"Compilation endpoint - integrate with Wasm\"}";
            send_response(client_socket, 200, "application/json", response, strlen(response));
            return;
        }
        
        if (strcmp(path, "/api/health") == 0) {
            const char* response = "{\"status\":\"ok\",\"version\":\"1.0.0\"}";
            send_response(client_socket, 200, "application/json", response, strlen(response));
            return;
        }
    }
    
    // Arquivos estáticos
    char file_path[1024]; // Aumentado para evitar truncation
    
    // Root -> index.html
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        snprintf(file_path, sizeof(file_path), "%s/index.html", PUBLIC_DIR);
        send_file(client_socket, file_path);
        return;
    }
    
    // Favicon (retorna 204 No Content para evitar erro)
    if (strcmp(path, "/favicon.ico") == 0) {
        send_response(client_socket, 204, "image/x-icon", "", 0);
        return;
    }
    
    // Arquivos Wasm
    if (strncmp(path, "/asteron.", 9) == 0) {
        snprintf(file_path, sizeof(file_path), "%s%s", PUBLIC_DIR, path);
        send_file(client_socket, file_path);
        return;
    }
    
    // Outros arquivos estáticos
    snprintf(file_path, sizeof(file_path), "%s%s", PUBLIC_DIR, path);
    
    // Verifica se arquivo existe
    struct stat st;
    if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
        send_file(client_socket, file_path);
    } else {
        // Tenta em frontend/
        snprintf(file_path, sizeof(file_path), "%s%s", FRONTEND_DIR, path);
        if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
            send_file(client_socket, file_path);
        } else {
            const char* not_found = "404 Not Found";
            send_response(client_socket, 404, "text/plain", not_found, strlen(not_found));
        }
    }
}

// =============================================================================
// SERVIDOR
// =============================================================================

static int create_server(int port) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        return -1;
    }
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind");
        close(server_socket);
        return -1;
    }
    
    if (listen(server_socket, MAX_CONNECTIONS) < 0) {
        perror("listen");
        close(server_socket);
        return -1;
    }
    
    return server_socket;
}

static void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read <= 0) {
        close(client_socket);
        return;
    }
    
    buffer[bytes_read] = '\0';
    
    // Parse HTTP request
    char method[16], path[512], protocol[16];
    if (sscanf(buffer, "%15s %511s %15s", method, path, protocol) == 3) {
        handle_request(client_socket, method, path);
    }
    
    close(client_socket);
}

int main(int argc, char** argv) {
    int port = SERVER_PORT;
    
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║     ASTERON WASM SERVER                                        ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Iniciando servidor na porta %d...\n", port);
    printf("Diretório público: %s\n", PUBLIC_DIR);
    printf("Diretório frontend: %s\n", FRONTEND_DIR);
    printf("\n");
    printf("Acesse: http://localhost:%d\n", port);
    printf("\n");
    
    int server_socket = create_server(port);
    if (server_socket < 0) {
        fprintf(stderr, "Erro ao criar servidor\n");
        return 1;
    }
    
    printf("✓ Servidor iniciado com sucesso!\n");
    printf("Pressione Ctrl+C para parar\n\n");
    
    // Loop principal
    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        
        int client_socket = accept(server_socket, 
                                   (struct sockaddr*)&client_address, 
                                   &client_len);
        
        if (client_socket < 0) {
            perror("accept");
            // Continua mesmo em caso de erro (servidor não fecha)
            sleep(1); // Evita loop infinito em caso de erro persistente
            continue;
        }
        
        // Processa requisição (simplificado - em produção, usar threads)
        handle_client(client_socket);
        
        // Continua o loop para próxima conexão
    }
    
    close(server_socket);
    return 0;
}

