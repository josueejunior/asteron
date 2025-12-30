#ifndef GRAPH_H
#define GRAPH_H

#include "../core/ast/ast.h"
#include <stddef.h>

// Tipos de nós no grafo
typedef enum {
    NODE_VARIABLE,
    NODE_FUNCTION,
    NODE_EXPRESSION,
    NODE_STATEMENT,
    NODE_CALL
} GraphNodeType;

// Estrutura de um nó no grafo
typedef struct GraphNode {
    char* name;                    // Nome/identificador do nó
    GraphNodeType type;            // Tipo do nó
    void* data;                    // Dados adicionais (pode ser ASTNode*)
    struct GraphEdge** edges;      // Lista de arestas saindo deste nó
    size_t edge_count;             // Número de arestas
    size_t edge_capacity;          // Capacidade do array de arestas
    int visited;                   // Flag para algoritmos de grafo
    
    // Métricas para análise de decisão
    int in_degree;                 // Número de dependências recebidas
    int out_degree;                // Número de dependências que saem
    int used;                      // Flag: 1 = usado, 0 = dead code
    int is_constant;              // Flag: 1 = valor constante
    void* constant_value;          // Valor constante (se is_constant == 1)
    int can_parallelize;           // Flag: 1 = pode executar em paralelo
} GraphNode;

// Estrutura de uma aresta no grafo
typedef struct GraphEdge {
    struct GraphNode* from;        // Nó origem
    struct GraphNode* to;          // Nó destino
    char* label;                   // Rótulo da aresta (opcional)
    int weight;                    // Peso da aresta (opcional)
} GraphEdge;

// Estrutura do grafo completo
typedef struct Graph {
    GraphNode** nodes;             // Lista de nós
    size_t node_count;             // Número de nós
    size_t node_capacity;          // Capacidade do array de nós
    GraphEdge** edges;              // Lista de todas as arestas
    size_t edge_count;              // Número de arestas
    size_t edge_capacity;           // Capacidade do array de arestas
    int destroyed;                  // Flag: 1 se já foi destruído (para evitar double free)
} Graph;

// Tipos de grafos que podemos gerar
typedef enum {
    GRAPH_DEPENDENCIES,            // Grafo de dependências de variáveis
    GRAPH_CALLS,                   // Grafo de chamadas de funções
    GRAPH_CONTROL_FLOW             // Grafo de fluxo de controle (CFG)
} GraphType;

// ==================== Funções do Grafo ====================

// Cria um novo grafo vazio
Graph* graph_create(void);

// Destrói um grafo e libera toda a memória
void graph_destroy(Graph* graph);

// Adiciona um nó ao grafo
GraphNode* graph_add_node(Graph* graph, const char* name, GraphNodeType type, void* data);

// Busca um nó pelo nome
GraphNode* graph_find_node(Graph* graph, const char* name);

// Adiciona uma aresta entre dois nós
GraphEdge* graph_add_edge(Graph* graph, GraphNode* from, GraphNode* to, const char* label);

// ==================== Análise da AST ====================

// Constrói grafo de dependências de variáveis a partir da AST
Graph* graph_build_dependencies(ASTNode* ast);

// Constrói grafo de chamadas de funções a partir da AST
Graph* graph_build_call_graph(ASTNode* ast);

// Constrói grafo de fluxo de controle (CFG) a partir da AST
Graph* graph_build_control_flow(ASTNode* ast);

// ==================== Exportação ====================

// Exporta grafo para formato DOT (Graphviz)
void graph_export_dot(Graph* graph, const char* filename, GraphType type);

// Exporta grafo para formato JSON
void graph_export_json(Graph* graph, const char* filename);

// Imprime grafo em formato texto simples
void graph_print(Graph* graph);

// ==================== Sistema de Decisão e Otimização ====================

// Estrutura para decisões de otimização
typedef struct OptimizationDecision {
    char* target_name;             // Nome do nó/variável/função
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

// Analisa grafos e gera plano de otimização
OptimizationPlan* graph_analyze_and_optimize(Graph* dep_graph, Graph* call_graph, Graph* cfg);

// Aplica análise de dead code elimination
void graph_analyze_dead_code(Graph* dep_graph, Graph* call_graph);

// Aplica análise de propagação de constantes
void graph_analyze_constants(Graph* dep_graph);

// Identifica blocos que podem ser paralelizados
void graph_analyze_parallelism(Graph* cfg);

// Identifica loops críticos
void graph_analyze_critical_loops(Graph* cfg);

// Calcula métricas dos nós (in_degree, out_degree)
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

