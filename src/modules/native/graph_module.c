/**
 * =============================================================================
 * ASTERON GRAPH MODULE - Módulo Nativo de Grafos
 * =============================================================================
 * 
 * Implementa funções nativas para operações com grafos.
 * 
 * =============================================================================
 */

#include "graph_module.h"
#include "../module.h"
#include "../../core/vm/vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * ESTRUTURA DE DADOS DO GRAFO
 * =============================================================================
 * 
 * Armazena o grafo como uma string estruturada:
 * Formato: "client_1:page1,page2,page3|client_2:page1,page4|..."
 */

#define MAX_GRAPH_SIZE 1000000  // 1MB máximo
static char g_graph_data[MAX_GRAPH_SIZE + 1] = {0};
static size_t g_graph_data_len = 0;
static int g_graph_client_count = 0;
static int g_graph_total_requests = 0;

/* =============================================================================
 * FUNÇÕES AUXILIARES
 * ============================================================================= */

static const char* get_string_arg(AsteronValue* args, int index) {
    if (!args || index < 0) return NULL;
    
    if (ASTERON_IS_STRING(args[index])) {
        AsteronString* str = ASTERON_AS_STRING(args[index]);
        if (!str) return NULL;
        
        if (str->length == 0) {
            // String vazia
            return "";
        }
        
        if (str->chars) {
            // String válida - chars já deve estar null-terminated
            return str->chars;
        }
    }
    return NULL;
}

static AsteronValue make_string(const char* str) {
    if (!str) return ASTERON_NIL();
    
    size_t len = strlen(str);
    AsteronString* astr = (AsteronString*)malloc(sizeof(AsteronString) + len + 1);
    if (!astr) return ASTERON_NIL();
    
    // Inicializa header (seguindo o padrão da VM)
    astr->header.obj_type = ASTERON_OBJ_STRING;
    astr->header.flags = 0;  // Não marca como OWNED - será gerenciado pelo caller
    astr->header.reserved = 0;
    astr->header.ref_count = 1;
    astr->header.next = NULL;
    
    astr->length = len;
    astr->hash = 0;
    astr->capacity = len + 1;
    memcpy(astr->chars, str, len);
    astr->chars[len] = '\0';
    
    return ASTERON_PTR(astr, ASTERON_VAL_STRING);
}

/* =============================================================================
 * FUNÇÕES DO MÓDULO
 * ============================================================================= */

/* graph_module_add_edge(client_id, page) -> void */
AsteronValue graph_module_add_edge(int argc, AsteronValue* args) {
    if (argc < 2) return ASTERON_NIL();
    
    const char* client_id = get_string_arg(args, 0);
    const char* page = get_string_arg(args, 1);
    
    if (!client_id || !page) return ASTERON_NIL();
    
    // Constrói chave do cliente
    char client_key[256];
    snprintf(client_key, sizeof(client_key), "%s:", client_id);
    
    // Procura cliente no grafo
    char* client_pos = strstr(g_graph_data, client_key);
    
    if (client_pos) {
        // Cliente existe - adiciona página
        char* pipe_pos = strchr(client_pos, '|');
        size_t entry_len = pipe_pos ? (size_t)(pipe_pos - client_pos) : strlen(client_pos);
        
        // Encontra posição do ':'
        char* colon_pos = strchr(client_pos, ':');
        if (colon_pos && colon_pos < client_pos + entry_len) {
            // Extrai páginas existentes
            char pages[4096] = {0};
            size_t pages_len = entry_len - (colon_pos - client_pos) - 1;
            if (pages_len > 0 && pages_len < sizeof(pages)) {
                memcpy(pages, colon_pos + 1, pages_len);
                pages[pages_len] = '\0';
            }
            
            // Verifica se página já existe
            char search_str[512];
            snprintf(search_str, sizeof(search_str), "%s", page);
            if (strstr(pages, search_str) == NULL) {
                // Adiciona nova página
                if (strlen(pages) > 0) {
                    strcat(pages, ",");
                }
                strcat(pages, page);
                
                // Reconstrói entrada
                size_t before_len = client_pos - g_graph_data;
                size_t after_len = g_graph_data_len - (client_pos - g_graph_data) - entry_len;
                
                if (before_len + strlen(client_key) + strlen(pages) + after_len < MAX_GRAPH_SIZE) {
                    char new_entry[5120];
                    snprintf(new_entry, sizeof(new_entry), "%s%s", client_key, pages);
                    
                    // Reconstrói grafo
                    memmove(g_graph_data + before_len + strlen(new_entry),
                           client_pos + entry_len,
                           after_len);
                    memcpy(g_graph_data + before_len, new_entry, strlen(new_entry));
                    g_graph_data_len = before_len + strlen(new_entry) + after_len;
                    g_graph_data[g_graph_data_len] = '\0';
                    g_graph_total_requests++;
                }
            }
        }
    } else {
        // Cliente novo
        char new_entry[512];
        if (g_graph_data_len > 0) {
            snprintf(new_entry, sizeof(new_entry), "|%s%s", client_key, page);
        } else {
            snprintf(new_entry, sizeof(new_entry), "%s%s", client_key, page);
        }
        
        if (g_graph_data_len + strlen(new_entry) < MAX_GRAPH_SIZE) {
            strcat(g_graph_data, new_entry);
            g_graph_data_len += strlen(new_entry);
            g_graph_client_count++;
            g_graph_total_requests++;
        }
    }
    
    return ASTERON_NIL();
}

/* graph_get_client_pages(client_id) -> string */
AsteronValue graph_get_client_pages(int argc, AsteronValue* args) {
    if (argc < 1) return make_string("");
    
    const char* client_id = get_string_arg(args, 0);
    if (!client_id) return make_string("");
    
    char client_key[256];
    snprintf(client_key, sizeof(client_key), "%s:", client_id);
    
    char* client_pos = strstr(g_graph_data, client_key);
    if (!client_pos) return make_string("");
    
    char* pipe_pos = strchr(client_pos, '|');
    size_t entry_len = pipe_pos ? (size_t)(pipe_pos - client_pos) : strlen(client_pos);
    
    char* colon_pos = strchr(client_pos, ':');
    if (colon_pos && colon_pos < client_pos + entry_len) {
        size_t pages_len = entry_len - (colon_pos - client_pos) - 1;
        if (pages_len > 0) {
            char* pages = (char*)malloc(pages_len + 1);
            if (pages) {
                memcpy(pages, colon_pos + 1, pages_len);
                pages[pages_len] = '\0';
                AsteronValue result = make_string(pages);
                free(pages);
                return result;
            }
        }
    }
    
    return make_string("");
}

/* graph_count_page_access(page) -> number */
AsteronValue graph_count_page_access(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_NUMBER(0);
    
    const char* page = get_string_arg(args, 0);
    if (!page) return ASTERON_NUMBER(0);
    
    int count = 0;
    const char* pos = g_graph_data;
    
    while ((pos = strstr(pos, page)) != NULL) {
        count++;
        pos += strlen(page);
    }
    
    return ASTERON_NUMBER((double)count);
}

/* graph_find_clients_by_page(page) -> string */
AsteronValue graph_find_clients_by_page(int argc, AsteronValue* args) {
    if (argc < 1) return make_string("");
    
    const char* page = get_string_arg(args, 0);
    if (!page) return make_string("");
    
    char clients[4096] = {0};
    const char* pos = g_graph_data;
    
    while (*pos) {
        // Encontra próximo '|' ou fim
        const char* pipe_pos = strchr(pos, '|');
        size_t entry_len = pipe_pos ? (size_t)(pipe_pos - pos) : strlen(pos);
        
        // Encontra ':'
        const char* colon_pos = strchr(pos, ':');
        if (colon_pos && colon_pos < pos + entry_len) {
            // Extrai client_id
            size_t client_id_len = colon_pos - pos;
            char client_id[256] = {0};
            if (client_id_len > 0 && client_id_len < sizeof(client_id)) {
                memcpy(client_id, pos, client_id_len);
                client_id[client_id_len] = '\0';
            }
            
            // Verifica se página está nas páginas deste cliente
            size_t pages_len = entry_len - (colon_pos - pos) - 1;
            if (pages_len > 0) {
                char* pages = (char*)malloc(pages_len + 1);
                if (pages) {
                    memcpy(pages, colon_pos + 1, pages_len);
                    pages[pages_len] = '\0';
                    
                    if (strstr(pages, page) != NULL) {
                        if (strlen(clients) > 0) {
                            strcat(clients, ",");
                        }
                        strcat(clients, client_id);
                    }
                    free(pages);
                }
            }
        }
        
        if (pipe_pos) {
            pos = pipe_pos + 1;
        } else {
            break;
        }
    }
    
    return make_string(clients);
}

/* graph_get_client_count() -> number */
AsteronValue graph_get_client_count(int argc, AsteronValue* args) {
    return ASTERON_NUMBER((double)g_graph_client_count);
}

/* graph_get_total_requests() -> number */
AsteronValue graph_get_total_requests(int argc, AsteronValue* args) {
    return ASTERON_NUMBER((double)g_graph_total_requests);
}

/* graph_clear() -> void */
AsteronValue graph_clear(int argc, AsteronValue* args) {
    g_graph_data[0] = '\0';
    g_graph_data_len = 0;
    g_graph_client_count = 0;
    g_graph_total_requests = 0;
    return ASTERON_NIL();
}

/* graph_export() -> string */
AsteronValue graph_export(int argc, AsteronValue* args) {
    if (g_graph_data_len == 0) {
        return make_string("Grafo vazio");
    }
    
    char export[8192] = "=== GRAFO DE ACESSOS ===\n";
    const char* pos = g_graph_data;
    
    while (*pos) {
        const char* pipe_pos = strchr(pos, '|');
        size_t entry_len = pipe_pos ? (size_t)(pipe_pos - pos) : strlen(pos);
        
        const char* colon_pos = strchr(pos, ':');
        if (colon_pos && colon_pos < pos + entry_len) {
            size_t client_id_len = colon_pos - pos;
            char client_id[256] = {0};
            if (client_id_len > 0 && client_id_len < sizeof(client_id)) {
                memcpy(client_id, pos, client_id_len);
                client_id[client_id_len] = '\0';
            }
            
            size_t pages_len = entry_len - (colon_pos - pos) - 1;
            char pages[2048] = {0};
            if (pages_len > 0 && pages_len < sizeof(pages)) {
                memcpy(pages, colon_pos + 1, pages_len);
                pages[pages_len] = '\0';
            }
            
            char line[2560];
            snprintf(line, sizeof(line), "%s -> [%s]\n", client_id, pages);
            if (strlen(export) + strlen(line) < sizeof(export)) {
                strcat(export, line);
            }
        }
        
        if (pipe_pos) {
            pos = pipe_pos + 1;
        } else {
            break;
        }
    }
    
    return make_string(export);
}

/* =============================================================================
 * REGISTRO DO MÓDULO
 * ============================================================================= */

ASTERON_MODULE_BEGIN(graph, "1.0.0", "Módulo nativo de grafos para Asteron")
    ASTERON_EXPORT_FUNC("graph_add_edge", graph_module_add_edge, 2, "graph_add_edge(client_id, page)")
    ASTERON_EXPORT_FUNC("graph_get_client_pages", graph_get_client_pages, 1, "graph_get_client_pages(client_id)")
    ASTERON_EXPORT_FUNC("graph_count_page_access", graph_count_page_access, 1, "graph_count_page_access(page)")
    ASTERON_EXPORT_FUNC("graph_find_clients_by_page", graph_find_clients_by_page, 1, "graph_find_clients_by_page(page)")
    ASTERON_EXPORT_FUNC("graph_get_client_count", graph_get_client_count, 0, "graph_get_client_count()")
    ASTERON_EXPORT_FUNC("graph_get_total_requests", graph_get_total_requests, 0, "graph_get_total_requests()")
    ASTERON_EXPORT_FUNC("graph_clear", graph_clear, 0, "graph_clear()")
    ASTERON_EXPORT_FUNC("graph_export", graph_export, 0, "graph_export()")
ASTERON_MODULE_END(graph, "1.0.0", "Módulo nativo de grafos para Asteron", NULL, NULL)

/* Função de registro (chamada pelo loader) */
void graph_module_register(void) {
    // Registra módulo no sistema de módulos
    module_register_builtin(&graph_module);
    
    // Registra funções diretamente na VM também
    vm_register_native("graph_add_edge", graph_module_add_edge, 2, 2);
    vm_register_native("graph_get_client_pages", graph_get_client_pages, 1, 1);
    vm_register_native("graph_count_page_access", graph_count_page_access, 1, 1);
    vm_register_native("graph_find_clients_by_page", graph_find_clients_by_page, 1, 1);
    vm_register_native("graph_get_client_count", graph_get_client_count, 0, 0);
    vm_register_native("graph_get_total_requests", graph_get_total_requests, 0, 0);
    vm_register_native("graph_clear", graph_clear, 0, 0);
    vm_register_native("graph_export", graph_export, 0, 0);
}

