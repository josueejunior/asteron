#ifndef GRAPH_H
#define GRAPH_H

#include "../core/ast/ast.h"
#include <stddef.h>

// Node types in the graph
typedef enum {
    NODE_VARIABLE,
    NODE_FUNCTION,
    NODE_EXPRESSION,
    NODE_STATEMENT,
    NODE_CALL
} GraphNodeType;

// Node structure in the graph
typedef struct GraphNode {
    char* name;                    // Node name/identifier
    GraphNodeType type;            // Node type
    void* data;                    // Dados adicionais (pode ser ASTNode*)
    struct GraphEdge** edges;      // List of edges leaving this node
    size_t edge_count;             // Number of edges
    size_t edge_capacity;          // Capacity of edges array
    int visited;                   // Flag for graph algorithms
    
    // Métricas para análise de decisão
    int in_degree;                 // Número de dependências recebidas
    int out_degree;                // Número de dependências que saem
    int used;                      // Flag: 1 = usado, 0 = dead code
    int is_constant;              // Flag: 1 = valor constante
    void* constant_value;          // Valor constante (se is_constant == 1)
    int can_parallelize;           // Flag: 1 = pode executar em paralelo
} GraphNode;

// Edge structure in the graph
typedef struct GraphEdge {
    struct GraphNode* from;        // Source node
    struct GraphNode* to;          // Destination node
    char* label;                   // Edge label (optional)
    int weight;                    // Edge weight (optional)
} GraphEdge;

// Complete graph structure
typedef struct Graph {
    GraphNode** nodes;             // List of nodes
    size_t node_count;             // Number of nodes
    size_t node_capacity;          // Capacity of nodes array
    GraphEdge** edges;              // List of all edges
    size_t edge_count;              // Number of edges
    size_t edge_capacity;           // Capacity of edges array
    int destroyed;                  // Flag: 1 se já foi destruído (para evitar double free)
} Graph;

// Graph types we can generate
typedef enum {
    GRAPH_DEPENDENCIES,            // Variable dependency graph
    GRAPH_CALLS,                   // Function call graph
    GRAPH_CONTROL_FLOW             // Control flow graph (CFG)
} GraphType;

// ==================== Funções do Grafo ====================

// Create a new empty graph
Graph* graph_create(void);

// Destroy a graph and free all memory
void graph_destroy(Graph* graph);

// Add a node to the graph
GraphNode* graph_add_node(Graph* graph, const char* name, GraphNodeType type, void* data);

// Find a node by name
GraphNode* graph_find_node(Graph* graph, const char* name);

// Add an edge between two nodes
GraphEdge* graph_add_edge(Graph* graph, GraphNode* from, GraphNode* to, const char* label);

// ==================== Análise da AST ====================

// Build variable dependency graph from AST
Graph* graph_build_dependencies(ASTNode* ast);

// Build function call graph from AST
Graph* graph_build_call_graph(ASTNode* ast);

// Build control flow graph (CFG) from AST
Graph* graph_build_control_flow(ASTNode* ast);

// ==================== Exportação ====================

// Export graph to DOT format (Graphviz)
void graph_export_dot(Graph* graph, const char* filename, GraphType type);

// Export graph to JSON format
void graph_export_json(Graph* graph, const char* filename);

// Print graph in simple text format
void graph_print(Graph* graph);

// ==================== Sistema de Decisão e Otimização ====================

// Estrutura para decisões de otimização
typedef struct OptimizationDecision {
    char* target_name;             // Node/variable/function name
    int decision_type;             // Tipo de decisão (ver abaixo)
    void* data;                     // Dados específicos da decisão
    int priority;                   // Prioridade da otimização
} OptimizationDecision;

// Tipos de decisões
typedef enum {
    DECISION_DEAD_CODE,            // Código morto (nunca usado)
    DECISION_CONSTANT_PROP,        // Propagação de constantes
    DECISION_PARALLELIZE,          // Pode paralelizar
    DECISION_REORDER,              // Reordenar blocos
    DECISION_OPTIMIZE_LOOP,        // Otimizar loop crítico
    DECISION_CACHE_RESULT          // Cachear resultado
} DecisionType;

// Estrutura para armazenar todas as decisões
typedef struct OptimizationPlan {
    OptimizationDecision* decisions;
    size_t decision_count;
    size_t decision_capacity;
} OptimizationPlan;

// Analyze graphs and generate optimization plan
OptimizationPlan* graph_analyze_and_optimize(Graph* dep_graph, Graph* call_graph, Graph* cfg);

// Aplica análise de dead code elimination
void graph_analyze_dead_code(Graph* dep_graph, Graph* call_graph);

// Aplica análise de propagação de constantes
void graph_analyze_constants(Graph* dep_graph);

// Identifica blocos que podem ser paralelizados
void graph_analyze_parallelism(Graph* cfg);

// Identifica loops críticos
void graph_analyze_critical_loops(Graph* cfg);

// Calculate node metrics (in_degree, out_degree)
void graph_calculate_metrics(Graph* graph);

// Libera plano de otimização
void optimization_plan_destroy(OptimizationPlan* plan);

// Imprime plano de otimização
void optimization_plan_print(OptimizationPlan* plan);

// ==================== Execução Adaptativa ====================

// Estrutura para métricas de execução em tempo real
typedef struct ExecutionMetrics {
    size_t call_count;              // Número de vezes que foi chamado
    double total_time;              // Tempo total de execução (em segundos)
    double avg_time;                // Tempo médio por chamada
    size_t cache_hits;              // Cache hits (para otimizações futuras)
    size_t cache_misses;            // Cache misses
    int is_hot;                     // Flag: função "quente" (chamada frequentemente)
    double last_call_time;          // Timestamp da última chamada
} ExecutionMetrics;

// Estrutura para entrada de métricas (nome + métricas)
typedef struct MetricEntry {
    char* name;                     // Nome da função/bloco
    ExecutionMetrics metrics;       // Métricas de execução
} MetricEntry;

// Estrutura para perfil de execução adaptativa
typedef struct AdaptiveProfile {
    GraphNode** hot_functions;      // Funções "quentes" (chamadas frequentemente)
    size_t hot_count;               // Número de funções quentes
    size_t hot_capacity;            // Capacidade do array
    
    GraphNode** reordered_blocks;   // Blocos reordenados para melhor cache
    size_t reordered_count;         // Número de blocos reordenados
    
    int parallelization_enabled;    // Flag: paralelização ativada
    int optimization_level;         // Nível de otimização (0-3)
    
    MetricEntry* metrics;           // Métricas por nome
    size_t metrics_count;           // Número de métricas
    size_t metrics_capacity;        // Capacidade do array de métricas
} AdaptiveProfile;

// Cria perfil adaptativo vazio
AdaptiveProfile* adaptive_profile_create(Graph* dep_graph, Graph* call_graph, Graph* cfg);

// Destrói perfil adaptativo
void adaptive_profile_destroy(AdaptiveProfile* profile);

// Registra chamada de função (chamado durante execução)
void adaptive_record_function_call(AdaptiveProfile* profile, const char* func_name, double execution_time);

// Registra execução de bloco (chamado durante execução)
void adaptive_record_block_execution(AdaptiveProfile* profile, const char* block_name, double execution_time);

// Analisa métricas e atualiza otimizações adaptativas
void adaptive_analyze_and_optimize(AdaptiveProfile* profile, Graph* dep_graph, Graph* call_graph, Graph* cfg);

// Identifica funções "quentes" (chamadas frequentemente)
void adaptive_identify_hot_functions(AdaptiveProfile* profile, Graph* call_graph);

// Reordena blocos para melhor cache
void adaptive_reorder_blocks(AdaptiveProfile* profile, Graph* cfg);

// Ajusta nível de paralelização baseado na carga
void adaptive_adjust_parallelization(AdaptiveProfile* profile, double system_load);

// Imprime perfil adaptativo
void adaptive_profile_print(AdaptiveProfile* profile);

// Obtém métricas de uma função
ExecutionMetrics* adaptive_get_metrics(AdaptiveProfile* profile, const char* name);

#endif // GRAPH_H

