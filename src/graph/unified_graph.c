#define _POSIX_C_SOURCE 200809L
#include "unified_graph.h"
#include "../utils/utils.h"
#include "../core/vm/vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

/* =============================================================================
 * FUNÇÕES AUXILIARES
 * ============================================================================= */

static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

static UnifiedNode* create_unified_node(const char* name, UnifiedNodeType type, void* user_data) {
    UnifiedNode* node = (UnifiedNode*)calloc(1, sizeof(UnifiedNode));
    if (node == NULL) return NULL;
    
    node->name = strdup(name);
    node->type = type;
    node->user_data = user_data;
    
    // Inicializa métricas
    memset(&node->metrics, 0, sizeof(NodeMetrics));
    node->metrics.min_execution_time = 1e9; // Valor alto inicial
    
    return node;
}

static void destroy_unified_node(UnifiedNode* node) {
    if (node == NULL) return;
    
    free(node->name);
    if (node->dependencies) free(node->dependencies);
    if (node->dependents) free(node->dependents);
    if (node->edges_out) free(node->edges_out);
    if (node->edges_in) free(node->edges_in);
    free(node);
}

/* =============================================================================
 * CRIAÇÃO E DESTRUIÇÃO
 * ============================================================================= */

UnifiedGraph* unified_graph_create(Graph* cfg, Graph* call_graph, Graph* dep_graph) {
    UnifiedGraph* graph = (UnifiedGraph*)calloc(1, sizeof(UnifiedGraph));
    if (graph == NULL) return NULL;
    
    graph->cfg = cfg;
    graph->call_graph = call_graph;
    graph->dep_graph = dep_graph;
    
    graph->node_capacity = 64;
    graph->nodes = (UnifiedNode**)calloc(graph->node_capacity, sizeof(UnifiedNode*));
    if (graph->nodes == NULL) {
        free(graph);
        return NULL;
    }
    
    graph->edge_capacity = 128;
    graph->edges = (UnifiedEdge**)calloc(graph->edge_capacity, sizeof(UnifiedEdge*));
    if (graph->edges == NULL) {
        free(graph->nodes);
        free(graph);
        return NULL;
    }
    
    graph->is_tracking = 1;
    graph->auto_version = 0;
    graph->next_version_id = 1;
    
    // Constrói nós unificados a partir dos grafos originais
    unified_graph_rebuild(graph);
    
    return graph;
}

void unified_graph_destroy(UnifiedGraph* graph) {
    if (graph == NULL) return;
    
    // Destrói todos os nós
    for (size_t i = 0; i < graph->node_count; i++) {
        destroy_unified_node(graph->nodes[i]);
    }
    free(graph->nodes);
    
    // Destrói todas as arestas
    for (size_t i = 0; i < graph->edge_count; i++) {
        free(graph->edges[i]->label);
        free(graph->edges[i]);
    }
    free(graph->edges);
    
    // Destrói histórico de versões
    GraphVersion* version = graph->version_history;
    while (version != NULL) {
        GraphVersion* next = version->next;
        if (version->snapshot) {
            // Não destruímos o snapshot aqui, apenas a referência
        }
        if (version->metrics_snapshot) {
            free(version->metrics_snapshot);
        }
        free(version);
        version = next;
    }
    
    free(graph);
}

void unified_graph_rebuild(UnifiedGraph* graph) {
    if (graph == NULL) return;
    
    // Limpa nós existentes
    for (size_t i = 0; i < graph->node_count; i++) {
        destroy_unified_node(graph->nodes[i]);
    }
    graph->node_count = 0;
    
    // Limpa arestas existentes
    for (size_t i = 0; i < graph->edge_count; i++) {
        free(graph->edges[i]->label);
        free(graph->edges[i]);
    }
    graph->edge_count = 0;
    
    // Reconstrói a partir dos grafos originais
    // 1. Adiciona nós do CFG
    if (graph->cfg != NULL) {
        for (size_t i = 0; i < graph->cfg->node_count; i++) {
            GraphNode* cfg_node = graph->cfg->nodes[i];
            UnifiedNode* unode = unified_graph_add_node(graph, cfg_node->name, 
                                                       UNIFIED_NODE_CONTROL, cfg_node);
            if (unode != NULL) {
                unode->cfg_node = cfg_node;
            }
        }
    }
    
    // 2. Adiciona nós do Call Graph
    if (graph->call_graph != NULL) {
        for (size_t i = 0; i < graph->call_graph->node_count; i++) {
            GraphNode* call_node = graph->call_graph->nodes[i];
            UnifiedNode* unode = unified_graph_find_node(graph, call_node->name);
            if (unode == NULL) {
                unode = unified_graph_add_node(graph, call_node->name, 
                                               UNIFIED_NODE_FUNCTION, call_node);
            }
            if (unode != NULL) {
                unode->call_node = call_node;
            }
        }
    }
    
    // 3. Adiciona nós do Dependency Graph
    if (graph->dep_graph != NULL) {
        for (size_t i = 0; i < graph->dep_graph->node_count; i++) {
            GraphNode* dep_node = graph->dep_graph->nodes[i];
            UnifiedNode* unode = unified_graph_find_node(graph, dep_node->name);
            if (unode == NULL) {
                unode = unified_graph_add_node(graph, dep_node->name, 
                                               UNIFIED_NODE_VARIABLE, dep_node);
            }
            if (unode != NULL) {
                unode->dep_node = dep_node;
            }
        }
    }
    
    // 4. Reconstrói arestas
    // (Implementação simplificada - em produção, mapearia todas as arestas)
}

/* =============================================================================
 * NÓS
 * ============================================================================= */

UnifiedNode* unified_graph_add_node(UnifiedGraph* graph, const char* name, 
                                     UnifiedNodeType type, void* user_data) {
    if (graph == NULL || name == NULL) return NULL;
    
    // Verifica se já existe
    UnifiedNode* existing = unified_graph_find_node(graph, name);
    if (existing != NULL) {
        return existing;
    }
    
    // Expande capacidade se necessário
    if (graph->node_count >= graph->node_capacity) {
        graph->node_capacity *= 2;
        UnifiedNode** new_nodes = (UnifiedNode**)realloc(
            graph->nodes, sizeof(UnifiedNode*) * graph->node_capacity);
        if (new_nodes == NULL) return NULL;
        graph->nodes = new_nodes;
    }
    
    UnifiedNode* node = create_unified_node(name, type, user_data);
    if (node == NULL) return NULL;
    
    graph->nodes[graph->node_count++] = node;
    return node;
}

UnifiedNode* unified_graph_find_node(UnifiedGraph* graph, const char* name) {
    if (graph == NULL || name == NULL) return NULL;
    
    for (size_t i = 0; i < graph->node_count; i++) {
        if (strcmp(graph->nodes[i]->name, name) == 0) {
            return graph->nodes[i];
        }
    }
    return NULL;
}

void unified_graph_add_dependency(UnifiedGraph* graph, UnifiedNode* from, 
                                  UnifiedNode* to, const char* label, int edge_type) {
    if (graph == NULL || from == NULL || to == NULL) return;
    
    // Cria aresta
    if (graph->edge_count >= graph->edge_capacity) {
        graph->edge_capacity *= 2;
        UnifiedEdge** new_edges = (UnifiedEdge**)realloc(
            graph->edges, sizeof(UnifiedEdge*) * graph->edge_capacity);
        if (new_edges == NULL) return;
        graph->edges = new_edges;
    }
    
    UnifiedEdge* edge = (UnifiedEdge*)calloc(1, sizeof(UnifiedEdge));
    if (edge == NULL) return;
    
    edge->from = from;
    edge->to = to;
    edge->label = label ? strdup(label) : strdup("");
    edge->edge_type = edge_type;
    edge->weight = 1;
    
    graph->edges[graph->edge_count++] = edge;
    
    // Adiciona às listas dos nós
    // (Simplificado - em produção, expandiria arrays dinamicamente)
}

/* =============================================================================
 * ANOTAÇÕES E MÉTRICAS
 * ============================================================================= */

void unified_node_start_execution(UnifiedNode* node) {
    if (node == NULL || !node->metrics.is_hot) return;
    
    // Em produção, armazenaria timestamp aqui
    // Por enquanto, apenas marca início
}

void unified_node_end_execution(UnifiedNode* node) {
    if (node == NULL) return;
    
    double elapsed = get_time_us(); // Simplificado
    
    NodeMetrics* m = &node->metrics;
    m->execution_count++;
    m->execution_time_us += elapsed;
    m->avg_execution_time = m->execution_time_us / m->execution_count;
    
    if (elapsed > m->max_execution_time) {
        m->max_execution_time = elapsed;
    }
    if (elapsed < m->min_execution_time) {
        m->min_execution_time = elapsed;
    }
    
    time_t now = time(NULL);
    if (m->first_execution == 0) {
        m->first_execution = now;
    }
    m->last_execution = now;
    
    // Marca como "hot" se executado muitas vezes
    if (m->execution_count > 100) {
        m->is_hot = 1;
    }
}

void unified_node_record_memory(UnifiedNode* node, size_t bytes, int is_allocation) {
    if (node == NULL) return;
    
    NodeMetrics* m = &node->metrics;
    if (is_allocation) {
        m->allocations++;
        m->memory_used_bytes += bytes;
        if (m->memory_used_bytes > m->memory_peak_bytes) {
            m->memory_peak_bytes = m->memory_used_bytes;
        }
    } else {
        m->deallocations++;
        if (m->memory_used_bytes >= bytes) {
            m->memory_used_bytes -= bytes;
        }
    }
}

void unified_node_record_network(UnifiedNode* node, size_t bytes_sent, 
                                  size_t bytes_received, int error) {
    if (node == NULL) return;
    
    NodeMetrics* m = &node->metrics;
    m->bytes_sent += bytes_sent;
    m->bytes_received += bytes_received;
    if (error) {
        m->network_errors++;
    }
}

NodeMetrics* unified_node_get_metrics(UnifiedNode* node) {
    return node != NULL ? &node->metrics : NULL;
}

void unified_node_reset_metrics(UnifiedNode* node) {
    if (node == NULL) return;
    memset(&node->metrics, 0, sizeof(NodeMetrics));
    node->metrics.min_execution_time = 1e9;
}

/* =============================================================================
 * HISTÓRICO E VERSIONAMENTO
 * ============================================================================= */

GraphVersion* unified_graph_create_version(UnifiedGraph* graph) {
    if (graph == NULL) return NULL;
    
    GraphVersion* version = (GraphVersion*)calloc(1, sizeof(GraphVersion));
    if (version == NULL) return NULL;
    
    version->version_id = graph->next_version_id++;
    version->timestamp = time(NULL);
    
    // Snapshot das métricas (não do grafo completo por enquanto)
    version->metrics_snapshot = (NodeMetrics*)malloc(
        sizeof(NodeMetrics) * graph->node_count);
    if (version->metrics_snapshot != NULL) {
        for (size_t i = 0; i < graph->node_count; i++) {
            version->metrics_snapshot[i] = graph->nodes[i]->metrics;
        }
    }
    
    // Adiciona à lista
    if (graph->version_history == NULL) {
        graph->version_history = version;
        graph->current_version = version;
    } else {
        version->prev = graph->current_version;
        graph->current_version->next = version;
        graph->current_version = version;
    }
    
    return version;
}

int unified_graph_restore_version(UnifiedGraph* graph, uint64_t version_id) {
    if (graph == NULL) return 0;
    
    GraphVersion* version = graph->version_history;
    while (version != NULL) {
        if (version->version_id == version_id) {
            // Restaura métricas
            if (version->metrics_snapshot != NULL) {
                for (size_t i = 0; i < graph->node_count && i < graph->node_count; i++) {
                    graph->nodes[i]->metrics = version->metrics_snapshot[i];
                }
            }
            graph->current_version = version;
            return 1;
        }
        version = version->next;
    }
    return 0;
}

void unified_graph_list_versions(UnifiedGraph* graph) {
    if (graph == NULL) return;
    
    printf("=== Histórico de Versões do Grafo ===\n");
    GraphVersion* version = graph->version_history;
    while (version != NULL) {
        char time_str[64];
        struct tm* tm_info = localtime(&version->timestamp);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        
        printf("Versão %llu: %s %s\n", 
               (unsigned long long)version->version_id, 
               time_str,
               (version == graph->current_version) ? "[ATUAL]" : "");
        version = version->next;
    }
}

void unified_graph_compare_versions(UnifiedGraph* graph, uint64_t v1, uint64_t v2) {
    // Implementação simplificada
    printf("Comparando versões %llu e %llu...\n", (unsigned long long)v1, (unsigned long long)v2);
}

/* =============================================================================
 * VISUALIZAÇÃO E EXPORTAÇÃO
 * ============================================================================= */

void unified_graph_export_dot(UnifiedGraph* graph, const char* filename) {
    if (graph == NULL || filename == NULL) return;
    
    FILE* f = fopen(filename, "w");
    if (f == NULL) return;
    
    fprintf(f, "digraph UnifiedGraph {\n");
    fprintf(f, "  rankdir=LR;\n");
    fprintf(f, "  node [shape=box];\n\n");
    
    // Nós
    for (size_t i = 0; i < graph->node_count; i++) {
        UnifiedNode* node = graph->nodes[i];
        if (node == NULL) continue;  // Proteção contra ponteiros nulos
        
        const char* node_name = node->name != NULL ? node->name : "unnamed";
        const char* color = node->metrics.is_hot ? "red" : "black";
        fprintf(f, "  \"%s\" [label=\"%s\\nExec: %zu\\nTime: %.2fus\", color=%s];\n",
                node_name, node_name, 
                node->metrics.execution_count,
                node->metrics.avg_execution_time,
                color);
    }
    
    // Arestas
    for (size_t i = 0; i < graph->edge_count; i++) {
        UnifiedEdge* edge = graph->edges[i];
        if (edge == NULL || edge->from == NULL || edge->to == NULL) continue;  // Proteção crítica
        
        const char* from_name = edge->from->name != NULL ? edge->from->name : "unnamed";
        const char* to_name = edge->to->name != NULL ? edge->to->name : "unnamed";
        const char* label = edge->label != NULL ? edge->label : "";
        fprintf(f, "  \"%s\" -> \"%s\" [label=\"%s\"];\n",
                from_name, to_name, label);
    }
    
    fprintf(f, "}\n");
    fclose(f);
}

void unified_graph_export_json(UnifiedGraph* graph, const char* filename) {
    // Implementação simplificada
    printf("Exportando grafo para JSON: %s\n", filename);
}

void unified_graph_print_stats(UnifiedGraph* graph) {
    if (graph == NULL) return;
    
    printf("\n=== Estatísticas do Grafo Unificado ===\n");
    printf("Nós: %zu\n", graph->node_count);
    printf("Arestas: %zu\n", graph->edge_count);
    printf("Execuções totais: %llu\n", (unsigned long long)graph->global_stats.total_executions);
    printf("Tempo total: %.2f us\n", graph->global_stats.total_execution_time_us);
    printf("Memória total: %zu bytes\n", graph->global_stats.total_memory_used);
    printf("\n");
}

void unified_node_print(UnifiedNode* node) {
    if (node == NULL) return;
    
    printf("\n=== Nó: %s ===\n", node->name);
    printf("Tipo: %d\n", node->type);
    printf("Execuções: %zu\n", node->metrics.execution_count);
    printf("Tempo médio: %.2f us\n", node->metrics.avg_execution_time);
    printf("Tempo máximo: %.2f us\n", node->metrics.max_execution_time);
    printf("Tempo mínimo: %.2f us\n", node->metrics.min_execution_time);
    printf("Memória usada: %zu bytes\n", node->metrics.memory_used_bytes);
    printf("Memória pico: %zu bytes\n", node->metrics.memory_peak_bytes);
    printf("Alocações: %zu\n", node->metrics.allocations);
    printf("É hot: %s\n", node->metrics.is_hot ? "sim" : "não");
    printf("Pode paralelizar: %s\n", node->metrics.can_parallelize ? "sim" : "não");
    printf("\n");
}

