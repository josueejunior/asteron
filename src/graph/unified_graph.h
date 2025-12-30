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

#ifndef UNIFIED_GRAPH_H
#define UNIFIED_GRAPH_H

#include "graph.h"
#include "../core/ast/ast.h"
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* =============================================================================
 * GRAFO UNIFICADO DE EXECUÇÃO
 * =============================================================================
 * Combina CFG + Call Graph + Dependencies + Data Flow em um único grafo
 * interativo com anotações inteligentes.
 * ============================================================================= */

// Tipos de nós no grafo unificado
typedef enum {
    UNIFIED_NODE_BLOCK,          // Bloco de código
    UNIFIED_NODE_FUNCTION,        // Função
    UNIFIED_NODE_VARIABLE,        // Variável
    UNIFIED_NODE_EXPRESSION,      // Expressão
    UNIFIED_NODE_CALL,            // Chamada de função
    UNIFIED_NODE_CONTROL,         // Controle de fluxo (if/while/for)
    UNIFIED_NODE_DATA,            // Fluxo de dados
    UNIFIED_NODE_IO,              // Operação I/O
    UNIFIED_NODE_NETWORK          // Operação de rede
} UnifiedNodeType;

// Anotações inteligentes para cada nó
typedef struct {
    // Métricas de execução
    double execution_time_us;     // Tempo de execução em microssegundos
    size_t execution_count;       // Quantas vezes foi executado
    double avg_execution_time;    // Tempo médio de execução
    double max_execution_time;    // Tempo máximo de execução
    double min_execution_time;    // Tempo mínimo de execução
    
    // Métricas de memória
    size_t memory_used_bytes;     // Memória usada
    size_t memory_peak_bytes;     // Pico de memória
    size_t allocations;           // Número de alocações
    size_t deallocations;         // Número de desalocações
    
    // Métricas de concorrência
    int locks_active;             // Número de locks ativos
    int wait_time_us;             // Tempo de espera em locks
    int contention_count;         // Número de contenções detectadas
    
    // Métricas de cache
    size_t cache_hits;            // Cache hits
    size_t cache_misses;          // Cache misses
    double cache_hit_rate;        // Taxa de acerto do cache
    
    // Métricas de rede (se aplicável)
    size_t bytes_sent;            // Bytes enviados
    size_t bytes_received;         // Bytes recebidos
    int network_errors;            // Erros de rede
    
    // Timestamps
    time_t first_execution;        // Primeira execução
    time_t last_execution;         // Última execução
    
    // Flags
    int is_hot;                   // É um "hot path"?
    int is_optimized;              // Foi otimizado?
    int can_parallelize;           // Pode ser paralelizado?
    int is_critical;               // É crítico para performance?
} NodeMetrics;

// Versão do grafo (para histórico)
typedef struct GraphVersion {
    uint64_t version_id;           // ID único da versão
    time_t timestamp;              // Quando foi criado
    Graph* snapshot;               // Snapshot do grafo nesta versão
    NodeMetrics* metrics_snapshot; // Métricas nesta versão
    struct GraphVersion* next;     // Próxima versão (lista encadeada)
    struct GraphVersion* prev;     // Versão anterior
} GraphVersion;

// Nó unificado (combina informações de todos os grafos)
typedef struct UnifiedNode {
    char* name;                    // Nome único do nó
    UnifiedNodeType type;          // Tipo do nó
    
    // Referências aos grafos originais
    GraphNode* cfg_node;           // Nó no CFG (se existir)
    GraphNode* call_node;          // Nó no call graph (se existir)
    GraphNode* dep_node;           // Nó no dependency graph (se existir)
    ASTNode* ast_node;             // Nó AST original (se existir)
    
    // Anotações inteligentes
    NodeMetrics metrics;           // Métricas de execução
    
    // Dependências unificadas
    struct UnifiedNode** dependencies;  // Nós dos quais depende
    size_t dep_count;
    size_t dep_capacity;
    
    struct UnifiedNode** dependents;     // Nós que dependem deste
    size_t dependent_count;
    size_t dependent_capacity;
    
    // Arestas unificadas
    struct UnifiedEdge** edges_out;      // Arestas saindo
    struct UnifiedEdge** edges_in;       // Arestas entrando
    size_t edge_out_count;
    size_t edge_in_count;
    
    // Metadados
    void* user_data;               // Dados do usuário
    int visited;                   // Flag para algoritmos
    int marked;                    // Flag genérica
} UnifiedNode;

// Aresta unificada
typedef struct UnifiedEdge {
    UnifiedNode* from;             // Nó origem
    UnifiedNode* to;               // Nó destino
    char* label;                   // Rótulo da aresta
    int weight;                     // Peso (para algoritmos)
    
    // Tipo de dependência
    enum {
        EDGE_CONTROL_FLOW,         // Fluxo de controle
        EDGE_DATA_FLOW,            // Fluxo de dados
        EDGE_CALL,                 // Chamada de função
        EDGE_DEPENDENCY,           // Dependência de variável
        EDGE_IO,                   // Dependência I/O
        EDGE_NETWORK                // Dependência de rede
    } edge_type;
    
    // Métricas da aresta
    size_t traversal_count;        // Quantas vezes foi atravessada
    double avg_weight;              // Peso médio
} UnifiedEdge;

// Grafo unificado
typedef struct UnifiedGraph {
    UnifiedNode** nodes;           // Todos os nós
    size_t node_count;
    size_t node_capacity;
    
    UnifiedEdge** edges;           // Todas as arestas
    size_t edge_count;
    size_t edge_capacity;
    
    // Grafos originais (referências, não ownership)
    Graph* cfg;                    // Control Flow Graph
    Graph* call_graph;             // Call Graph
    Graph* dep_graph;              // Dependency Graph
    
    // Histórico de versões
    GraphVersion* version_history; // Lista de versões
    GraphVersion* current_version; // Versão atual
    uint64_t next_version_id;      // Próximo ID de versão
    
    // Estatísticas globais
    struct {
        uint64_t total_executions;
        double total_execution_time_us;
        size_t total_memory_used;
        int total_network_operations;
    } global_stats;
    
    // Flags
    int is_tracking;               // Está rastreando execuções?
    int auto_version;               // Cria versões automaticamente?
} UnifiedGraph;

/* =============================================================================
 * API - CRIAÇÃO E DESTRUIÇÃO
 * ============================================================================= */

/**
 * Cria um grafo unificado a partir dos grafos individuais
 */
UnifiedGraph* unified_graph_create(Graph* cfg, Graph* call_graph, Graph* dep_graph);

/**
 * Destrói o grafo unificado
 */
void unified_graph_destroy(UnifiedGraph* graph);

/**
 * Reconstrói o grafo unificado (útil após mudanças nos grafos originais)
 */
void unified_graph_rebuild(UnifiedGraph* graph);

/* =============================================================================
 * API - NÓS
 * ============================================================================= */

/**
 * Adiciona um nó ao grafo unificado
 */
UnifiedNode* unified_graph_add_node(UnifiedGraph* graph, const char* name, 
                                    UnifiedNodeType type, void* user_data);

/**
 * Busca um nó pelo nome
 */
UnifiedNode* unified_graph_find_node(UnifiedGraph* graph, const char* name);

/**
 * Adiciona dependência entre dois nós
 */
void unified_graph_add_dependency(UnifiedGraph* graph, UnifiedNode* from, 
                                  UnifiedNode* to, const char* label, int edge_type);

/* =============================================================================
 * API - ANOTAÇÕES E MÉTRICAS
 * ============================================================================= */

/**
 * Inicia rastreamento de execução de um nó
 */
void unified_node_start_execution(UnifiedNode* node);

/**
 * Finaliza rastreamento de execução de um nó
 */
void unified_node_end_execution(UnifiedNode* node);

/**
 * Registra uso de memória
 */
void unified_node_record_memory(UnifiedNode* node, size_t bytes, int is_allocation);

/**
 * Registra operação de rede
 */
void unified_node_record_network(UnifiedNode* node, size_t bytes_sent, 
                                  size_t bytes_received, int error);

/**
 * Obtém métricas de um nó
 */
NodeMetrics* unified_node_get_metrics(UnifiedNode* node);

/**
 * Reseta métricas de um nó
 */
void unified_node_reset_metrics(UnifiedNode* node);

/* =============================================================================
 * API - HISTÓRICO E VERSIONAMENTO
 * ============================================================================= */

/**
 * Cria uma nova versão do grafo (snapshot)
 */
GraphVersion* unified_graph_create_version(UnifiedGraph* graph);

/**
 * Restaura uma versão específica
 */
int unified_graph_restore_version(UnifiedGraph* graph, uint64_t version_id);

/**
 * Lista todas as versões disponíveis
 */
void unified_graph_list_versions(UnifiedGraph* graph);

/**
 * Compara duas versões do grafo
 */
void unified_graph_compare_versions(UnifiedGraph* graph, uint64_t v1, uint64_t v2);

/* =============================================================================
 * API - VISUALIZAÇÃO E EXPORTAÇÃO
 * ============================================================================= */

/**
 * Exporta grafo para DOT (Graphviz) com anotações
 */
void unified_graph_export_dot(UnifiedGraph* graph, const char* filename);

/**
 * Exporta grafo para JSON com métricas
 */
void unified_graph_export_json(UnifiedGraph* graph, const char* filename);

/**
 * Imprime estatísticas do grafo
 */
void unified_graph_print_stats(UnifiedGraph* graph);

/**
 * Imprime nó específico com todas as métricas
 */
void unified_node_print(UnifiedNode* node);

#endif // UNIFIED_GRAPH_H

