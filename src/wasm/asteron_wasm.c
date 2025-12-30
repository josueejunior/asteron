/**
 * =============================================================================
 * ASTERON WEBASSEMBLY EXPORT - IMPLEMENTAÇÃO
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "asteron_wasm.h"
#include "../core/lexer/lexer.h"
#include "../core/parser/parser.h"
#include "../core/ast/ast.h"
#include "../graph/unified_graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// =============================================================================
// GLOBAL STATE (para Wasm)
// =============================================================================

static UnifiedGraph* g_graph = NULL;
static ASTNode* g_last_ast = NULL;

// =============================================================================
// JSON SERIALIZATION
// =============================================================================

// Buffer dinâmico para construir JSON
typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} JsonBuffer;

static JsonBuffer* json_buffer_create(void) {
    JsonBuffer* buf = (JsonBuffer*)calloc(1, sizeof(JsonBuffer));
    if (buf == NULL) return NULL;
    buf->capacity = 4096;
    buf->data = (char*)malloc(buf->capacity);
    if (buf->data == NULL) {
        free(buf);
        return NULL;
    }
    buf->data[0] = '\0';
    return buf;
}

static void json_buffer_append(JsonBuffer* buf, const char* str) {
    if (buf == NULL || str == NULL) return;
    
    size_t len = strlen(str);
    while (buf->size + len + 1 >= buf->capacity) {
        buf->capacity *= 2;
        buf->data = (char*)realloc(buf->data, buf->capacity);
    }
    
    strcat(buf->data, str);
    buf->size += len;
}

static void json_buffer_append_int(JsonBuffer* buf, int value) {
    char temp[32];
    snprintf(temp, sizeof(temp), "%d", value);
    json_buffer_append(buf, temp);
}

static void json_buffer_append_double(JsonBuffer* buf, double value) {
    char temp[64];
    snprintf(temp, sizeof(temp), "%.6f", value);
    json_buffer_append(buf, temp);
}

static char* json_buffer_finish(JsonBuffer* buf) {
    if (buf == NULL) return NULL;
    char* result = strdup(buf->data);
    free(buf->data);
    free(buf);
    return result;
}

// Serializa AST para JSON
static void serialize_ast_node(JsonBuffer* buf, ASTNode* node, int depth) {
    if (node == NULL) {
        json_buffer_append(buf, "null");
        return;
    }
    
    json_buffer_append(buf, "{");
    
    // Tipo
    json_buffer_append(buf, "\"type\":\"");
    const char* type_name = "UNKNOWN";
    switch (node->type) {
        case AST_LITERAL: type_name = "LITERAL"; break;
        case AST_IDENTIFIER: type_name = "IDENTIFIER"; break;
        case AST_BINARY_EXPRESSION: type_name = "BINARY_EXPRESSION"; break;
        case AST_UNARY_EXPRESSION: type_name = "UNARY_EXPRESSION"; break;
        case AST_FUNCTION_DECLARATION: type_name = "FUNCTION"; break;
        case AST_FUNCTION_CALL: type_name = "CALL"; break;
        case AST_BLOCK: type_name = "BLOCK"; break;
        case AST_IF_STATEMENT: type_name = "IF"; break;
        case AST_WHILE_STATEMENT: type_name = "WHILE"; break;
        case AST_RETURN: type_name = "RETURN"; break;
        case AST_VARIABLE_DECLARATION: type_name = "VARIABLE"; break;
        case AST_ASSIGNMENT: type_name = "ASSIGNMENT"; break;
        default: break;
    }
    json_buffer_append(buf, type_name);
    json_buffer_append(buf, "\"");
    
    // Dados específicos por tipo
    switch (node->type) {
        case AST_IDENTIFIER:
            if (node->as.identifier.name) {
                json_buffer_append(buf, ",\"name\":\"");
                json_buffer_append(buf, node->as.identifier.name);
                json_buffer_append(buf, "\"");
            }
            break;
            
        case AST_FUNCTION_DECLARATION:
            if (node->as.function_decl.name) {
                json_buffer_append(buf, ",\"name\":\"");
                json_buffer_append(buf, node->as.function_decl.name);
                json_buffer_append(buf, "\"");
            }
            if (node->as.function_decl.parameters) {
                json_buffer_append(buf, ",\"params\":[");
                for (size_t i = 0; i < node->as.function_decl.parameter_count; i++) {
                    if (i > 0) json_buffer_append(buf, ",");
                    json_buffer_append(buf, "\"");
                    if (node->as.function_decl.parameters[i]) {
                        json_buffer_append(buf, node->as.function_decl.parameters[i]);
                    }
                    json_buffer_append(buf, "\"");
                }
                json_buffer_append(buf, "]");
            }
            break;
            
        default:
            break;
    }
    
    json_buffer_append(buf, "}");
}

// Serializa grafo unificado para JSON (formato React Flow)
static char* serialize_graph_to_json(UnifiedGraph* graph) {
    if (graph == NULL) return strdup("{\"nodes\":[],\"edges\":[]}");
    
    JsonBuffer* buf = json_buffer_create();
    if (buf == NULL) return strdup("{\"nodes\":[],\"edges\":[]}");
    
    json_buffer_append(buf, "{\"nodes\":[");
    
    // Serializa nós
    bool first = true;
    for (size_t i = 0; i < graph->node_count; i++) {
        UnifiedNode* node = graph->nodes[i];
        if (node == NULL) continue;
        
        if (!first) json_buffer_append(buf, ",");
        first = false;
        
        json_buffer_append(buf, "{");
        json_buffer_append(buf, "\"id\":\"");
        json_buffer_append(buf, node->name ? node->name : "");
        json_buffer_append(buf, "\"");
        
        json_buffer_append(buf, ",\"data\":{\"label\":\"");
        json_buffer_append(buf, node->name ? node->name : "");
        json_buffer_append(buf, "\"");
        
        // Adiciona métricas
        json_buffer_append(buf, ",\"executionCount\":");
        json_buffer_append_int(buf, (int)node->metrics.execution_count);
        
        json_buffer_append(buf, ",\"avgTime\":");
        json_buffer_append_double(buf, node->metrics.avg_execution_time);
        
        json_buffer_append(buf, ",\"isHot\":");
        json_buffer_append(buf, node->metrics.is_hot ? "true" : "false");
        
        json_buffer_append(buf, "}");
        
        // Estilo baseado em "calor"
        json_buffer_append(buf, ",\"style\":{");
        if (node->metrics.is_hot) {
            json_buffer_append(buf, "\"background\":\"#ff4444\",\"color\":\"#fff\"");
        } else if (node->metrics.execution_count > 10) {
            json_buffer_append(buf, "\"background\":\"#ffaa00\",\"color\":\"#000\"");
        } else {
            json_buffer_append(buf, "\"background\":\"#4CAF50\",\"color\":\"#fff\"");
        }
        json_buffer_append(buf, "}");
        
        json_buffer_append(buf, "}");
    }
    
    json_buffer_append(buf, "],\"edges\":[");
    
    // Serialize edges
    first = true;
    for (size_t i = 0; i < graph->edge_count; i++) {
        UnifiedEdge* edge = graph->edges[i];
        if (edge == NULL || edge->from == NULL || edge->to == NULL) continue;
        
        if (!first) json_buffer_append(buf, ",");
        first = false;
        
        json_buffer_append(buf, "{");
        json_buffer_append(buf, "\"id\":\"");
        char edge_id[256];
        snprintf(edge_id, sizeof(edge_id), "%s-%s",
                 edge->from->name ? edge->from->name : "",
                 edge->to->name ? edge->to->name : "");
        json_buffer_append(buf, edge_id);
        json_buffer_append(buf, "\"");
        
        json_buffer_append(buf, ",\"source\":\"");
        json_buffer_append(buf, edge->from->name ? edge->from->name : "");
        json_buffer_append(buf, "\"");
        
        json_buffer_append(buf, ",\"target\":\"");
        json_buffer_append(buf, edge->to->name ? edge->to->name : "");
        json_buffer_append(buf, "\"");
        
        if (edge->label) {
            json_buffer_append(buf, ",\"label\":\"");
            json_buffer_append(buf, edge->label);
            json_buffer_append(buf, "\"");
        }
        
        json_buffer_append(buf, "}");
    }
    
    json_buffer_append(buf, "]}");
    
    return json_buffer_finish(buf);
}

// =============================================================================
// API PÚBLICA
// =============================================================================

char* compile_to_ast(const char* code) {
    if (code == NULL) return strdup("{\"error\":\"No code provided\"}");
    
    // Tokeniza
    Lexer* lexer = lexer_create(code);
    if (lexer == NULL) return strdup("{\"error\":\"Failed to create lexer\"}");
    
    // Parse
    Parser* parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        return strdup("{\"error\":\"Failed to create parser\"}");
    }
    
    ASTNode* ast = parser_parse(parser);
    
    // Serializa AST
    JsonBuffer* buf = json_buffer_create();
    if (buf == NULL) {
        if (ast) ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        return strdup("{\"error\":\"Failed to create JSON buffer\"}");
    }
    
    json_buffer_append(buf, "{\"ast\":");
    serialize_ast_node(buf, ast, 0);
    json_buffer_append(buf, "}");
    
    // Limpa
    if (g_last_ast) ast_destroy(g_last_ast);
    g_last_ast = ast;
    
    parser_destroy(parser);
    lexer_destroy(lexer);
    
    return json_buffer_finish(buf);
}

char* compile_to_graph(const char* code) {
    if (code == NULL) return strdup("{\"error\":\"No code provided\"}");
    
    // Compila para AST
    char* ast_json = compile_to_ast(code);
    free(ast_json); // Não precisamos do JSON do AST aqui
    
    if (g_last_ast == NULL) {
        return strdup("{\"error\":\"Failed to parse code\"}");
    }
    
    // Cria grafo unificado (simplificado - em produção, construiria grafo completo)
    if (g_graph == NULL) {
        g_graph = unified_graph_create(NULL, NULL, NULL);
    }
    
    // Adiciona nós do AST ao grafo
    // (simplificado - em produção, percorreria AST e adicionaria nós)
    
    // Serializa grafo
    return serialize_graph_to_json(g_graph);
}

char* get_graph_json(void) {
    if (g_graph == NULL) {
        return strdup("{\"nodes\":[],\"edges\":[]}");
    }
    return serialize_graph_to_json(g_graph);
}

char* get_hot_paths(void) {
    if (g_graph == NULL) {
        return strdup("{\"hotPaths\":[]}");
    }
    
    JsonBuffer* buf = json_buffer_create();
    if (buf == NULL) return strdup("{\"hotPaths\":[]}");
    
    json_buffer_append(buf, "{\"hotPaths\":[");
    
    bool first = true;
    for (size_t i = 0; i < g_graph->node_count; i++) {
        UnifiedNode* node = g_graph->nodes[i];
        if (node == NULL || !node->metrics.is_hot) continue;
        
        if (!first) json_buffer_append(buf, ",");
        first = false;
        
        json_buffer_append(buf, "{");
        json_buffer_append(buf, "\"name\":\"");
        json_buffer_append(buf, node->name ? node->name : "");
        json_buffer_append(buf, "\"");
        
        json_buffer_append(buf, ",\"executionCount\":");
        json_buffer_append_int(buf, (int)node->metrics.execution_count);
        
        json_buffer_append(buf, ",\"avgTime\":");
        json_buffer_append_double(buf, node->metrics.avg_execution_time);
        
        json_buffer_append(buf, "}");
    }
    
    json_buffer_append(buf, "]}");
    
    return json_buffer_finish(buf);
}

char* get_node_metrics(const char* node_name) {
    if (node_name == NULL || g_graph == NULL) {
        return strdup("{\"error\":\"Node not found\"}");
    }
    
    UnifiedNode* node = unified_graph_find_node(g_graph, node_name);
    if (node == NULL) {
        return strdup("{\"error\":\"Node not found\"}");
    }
    
    JsonBuffer* buf = json_buffer_create();
    if (buf == NULL) return strdup("{\"error\":\"Failed to create buffer\"}");
    
    json_buffer_append(buf, "{");
    json_buffer_append(buf, "\"name\":\"");
    json_buffer_append(buf, node->name ? node->name : "");
    json_buffer_append(buf, "\"");
    
    json_buffer_append(buf, ",\"executionCount\":");
    json_buffer_append_int(buf, (int)node->metrics.execution_count);
    
    json_buffer_append(buf, ",\"avgTime\":");
    json_buffer_append_double(buf, node->metrics.avg_execution_time);
    
    json_buffer_append(buf, ",\"maxTime\":");
    json_buffer_append_double(buf, node->metrics.max_execution_time);
    
    json_buffer_append(buf, ",\"isHot\":");
    json_buffer_append(buf, node->metrics.is_hot ? "true" : "false");
    
    json_buffer_append(buf, ",\"cacheHits\":");
    json_buffer_append_int(buf, (int)node->metrics.cache_hits);
    
    json_buffer_append(buf, ",\"cacheMisses\":");
    json_buffer_append_int(buf, (int)node->metrics.cache_misses);
    
    json_buffer_append(buf, "}");
    
    return json_buffer_finish(buf);
}

char* execute_with_metrics(const char* code, bool collect_metrics) {
    // Em produção, executaria código e coletaria métricas
    // Por enquanto, apenas compila
    return compile_to_graph(code);
}

char* get_execution_stats(void) {
    JsonBuffer* buf = json_buffer_create();
    if (buf == NULL) return strdup("{}");
    
    json_buffer_append(buf, "{");
    json_buffer_append(buf, "\"totalNodes\":");
    json_buffer_append_int(buf, g_graph ? (int)g_graph->node_count : 0);
    json_buffer_append(buf, "}");
    
    return json_buffer_finish(buf);
}

void wasm_free(void* ptr) {
    free(ptr);
}

const char* get_version(void) {
    return "1.0.0";
}

