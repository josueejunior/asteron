#ifndef GRAPH_DECLARATIVE_H
#define GRAPH_DECLARATIVE_H

#include "../core/ast/ast.h"
#include "../graph/graph.h"
#include <stddef.h>

// Grafo declarativo (construído a partir de anotações)
typedef struct DeclarativeGraph DeclarativeGraph;

// Nó no grafo declarativo
typedef struct DeclarativeNode {
    ASTNode* ast_node;              // Nó da AST correspondente
    Annotation* annotations;       // Anotações do nó
    size_t* dependencies;           // IDs dos nós dos quais depende
    size_t dep_count;               // Número de dependências
    int can_execute_parallel;       // 1 se pode executar em paralelo
    int execution_priority;         // Prioridade de execução
} DeclarativeNode;

// Grafo declarativo
struct DeclarativeGraph {
    DeclarativeNode* nodes;
    size_t node_count;
    size_t node_capacity;
    
    Graph* execution_graph;         // Grafo de execução construído
};

// Funções do grafo declarativo
DeclarativeGraph* declarative_graph_create(ASTNode* ast);
void declarative_graph_destroy(DeclarativeGraph* graph);
void declarative_graph_build(DeclarativeGraph* graph);
void declarative_graph_print(DeclarativeGraph* graph);
Graph* declarative_graph_get_execution_graph(DeclarativeGraph* graph);

#endif // GRAPH_DECLARATIVE_H

