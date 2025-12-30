#include "graph_declarative.h"
#include "../utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Função recursiva para coletar nós com anotações
static void collect_annotated_nodes(ASTNode* node, DeclarativeGraph* graph) {
    if (node == NULL) return;
    
    // Se o nó tem anotações, adiciona ao grafo declarativo
    if (node->annotations != NULL || 
        (node->type == AST_VARIABLE_DECLARATION && node->as.variable_decl.annotations != NULL) ||
        (node->type == AST_PRINT && node->as.print_stmt.annotations != NULL)) {
        
        if (graph->node_count >= graph->node_capacity) {
            graph->node_capacity *= 2;
            DeclarativeNode* new_nodes = (DeclarativeNode*)realloc(
                graph->nodes, sizeof(DeclarativeNode) * graph->node_capacity);
            if (new_nodes == NULL) return;
            graph->nodes = new_nodes;
        }
        
        DeclarativeNode* dnode = &graph->nodes[graph->node_count++];
        dnode->ast_node = node;
        dnode->annotations = node->annotations;
        if (node->type == AST_VARIABLE_DECLARATION) {
            dnode->annotations = node->as.variable_decl.annotations;
        } else if (node->type == AST_PRINT) {
            dnode->annotations = node->as.print_stmt.annotations;
        }
        dnode->dependencies = NULL;
        dnode->dep_count = 0;
        dnode->can_execute_parallel = 0;
        dnode->execution_priority = 0;
    }
    
    // Processa recursivamente
    switch (node->type) {
        case AST_BLOCK:
            for (size_t i = 0; i < node->as.block.count; i++) {
                collect_annotated_nodes(node->as.block.statements[i], graph);
            }
            break;
        case AST_IF_STATEMENT:
            collect_annotated_nodes(node->as.if_stmt.then_branch, graph);
            if (node->as.if_stmt.else_branch != NULL) {
                collect_annotated_nodes(node->as.if_stmt.else_branch, graph);
            }
            break;
        case AST_WHILE_STATEMENT:
            collect_annotated_nodes(node->as.while_stmt.body, graph);
            break;
        case AST_FOR_STATEMENT:
            collect_annotated_nodes(node->as.for_stmt.body, graph);
            break;
        case AST_FUNCTION_DECLARATION:
            collect_annotated_nodes(node->as.function_decl.body, graph);
            break;
        default:
            break;
    }
}

// Analisa anotações e constrói dependências
static void analyze_annotations(DeclarativeGraph* graph) {
    if (graph == NULL) return;
    
    for (size_t i = 0; i < graph->node_count; i++) {
        DeclarativeNode* dnode = &graph->nodes[i];
        Annotation* annotation = dnode->annotations;
        
        while (annotation != NULL) {
            switch (annotation->type) {
                case ANNOTATION_PARALLEL:
                    dnode->can_execute_parallel = 1;
                    dnode->execution_priority += 10; /* Boost de prioridade */
                    break;
                    
                case ANNOTATION_PURE:
                    /* Funções puras podem ser memoizadas e paralelizadas */
                    dnode->can_execute_parallel = 1;
                    break;
                    
                case ANNOTATION_MEMOIZE:
                    /* Resultados são cacheados */
                    dnode->execution_priority += 5;
                    break;
                    
                case ANNOTATION_LAZY:
                    /* Avaliação preguiçosa - baixa prioridade inicial */
                    dnode->execution_priority -= 5;
                    break;
                    
                case ANNOTATION_HOT:
                    /* Hot path - alta prioridade para JIT */
                    dnode->execution_priority += 20;
                    break;
                    
                case ANNOTATION_INLINE:
                    /* Sugestão de inline */
                    dnode->execution_priority += 15;
                    break;
                    
                case ANNOTATION_NOOPT:
                    /* Não otimizar */
                    dnode->can_execute_parallel = 0;
                    break;
                    
                case ANNOTATION_ASYNC:
                    /* Operação assíncrona */
                    dnode->can_execute_parallel = 1;
                    break;
                    
                case ANNOTATION_DEPENDS:
                    /* Adiciona dependências explícitas */
                    for (size_t j = 0; j < annotation->dep_count; j++) {
                        const char* dep_name = annotation->dependencies[j];
                        
                        /* Encontra nó correspondente à dependência */
                        for (size_t k = 0; k < graph->node_count; k++) {
                            if (k == i) continue;
                            
                            DeclarativeNode* other = &graph->nodes[k];
                            if (other->ast_node->type == AST_VARIABLE_DECLARATION) {
                                if (strcmp(other->ast_node->as.variable_decl.name, dep_name) == 0) {
                                    size_t new_capacity = dnode->dep_count + 1;
                                    size_t* new_deps = (size_t*)realloc(
                                        dnode->dependencies, sizeof(size_t) * new_capacity);
                                    if (new_deps != NULL) {
                                        dnode->dependencies = new_deps;
                                        dnode->dependencies[dnode->dep_count++] = k;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    break;
                    
                case ANNOTATION_CACHE:
                    /* Obsoleto - tratado como MEMOIZE */
                    dnode->execution_priority += 5;
                    break;
            }
            
            annotation = annotation->next;
        }
    }
}

// Constrói grafo de execução a partir do grafo declarativo
static void build_execution_graph(DeclarativeGraph* graph) {
    if (graph == NULL) return;
    
    graph->execution_graph = graph_create();
    if (graph->execution_graph == NULL) return;
    
    // Cria nós no grafo de execução
    for (size_t i = 0; i < graph->node_count; i++) {
        DeclarativeNode* dnode = &graph->nodes[i];
        const char* node_name = "unknown";
        
        if (dnode->ast_node->type == AST_VARIABLE_DECLARATION) {
            node_name = dnode->ast_node->as.variable_decl.name;
        } else if (dnode->ast_node->type == AST_PRINT) {
            node_name = "print";
        }
        
        GraphNode* gnode = graph_add_node(graph->execution_graph, node_name, NODE_STATEMENT, dnode->ast_node);
        
        // Marca como paralelizável se tiver anotação @parallel
        if (dnode->can_execute_parallel) {
            if (gnode != NULL) {
                gnode->can_parallelize = 1;
            }
        }
    }
    
    // CORREÇÃO: Protege contra acesso a estrutura vazia
    if (graph->node_count == 0 || graph->nodes == NULL) {
        return;
    }
    
    // Cria arestas de dependência
    for (size_t i = 0; i < graph->node_count; i++) {
        DeclarativeNode* dnode = &graph->nodes[i];
        
        // CORREÇÃO: Verifica se dnode e ast_node são válidos
        if (dnode == NULL || dnode->ast_node == NULL) {
            continue;
        }
        
        // Encontra nó correspondente no grafo
        const char* from_name = "unknown";
        if (dnode->ast_node->type == AST_VARIABLE_DECLARATION) {
            from_name = dnode->ast_node->as.variable_decl.name;
        }
        GraphNode* from_node = graph_find_node(graph->execution_graph, from_name);
        
        // Cria arestas para dependências
        for (size_t j = 0; j < dnode->dep_count; j++) {
            size_t dep_id = dnode->dependencies[j];
            if (dep_id >= graph->node_count) continue;
            
            DeclarativeNode* dep_node = &graph->nodes[dep_id];
            
            // CORREÇÃO: Verifica se dep_node e ast_node são válidos
            if (dep_node == NULL || dep_node->ast_node == NULL) {
                continue;
            }
            
            const char* to_name = "unknown";
            if (dep_node->ast_node->type == AST_VARIABLE_DECLARATION) {
                to_name = dep_node->ast_node->as.variable_decl.name;
            }
            
            GraphNode* to_node = graph_find_node(graph->execution_graph, to_name);
            if (from_node != NULL && to_node != NULL) {
                graph_add_edge(graph->execution_graph, to_node, from_node, "depends");
            }
        }
    }
}

DeclarativeGraph* declarative_graph_create(ASTNode* ast) {
    if (ast == NULL) return NULL;
    
    DeclarativeGraph* graph = (DeclarativeGraph*)malloc(sizeof(DeclarativeGraph));
    if (graph == NULL) return NULL;
    
    graph->node_count = 0;
    graph->node_capacity = 32;
    graph->nodes = (DeclarativeNode*)malloc(sizeof(DeclarativeNode) * graph->node_capacity);
    if (graph->nodes == NULL) {
        free(graph);
        return NULL;
    }
    
    graph->execution_graph = NULL;
    
    // Coleta nós com anotações
    collect_annotated_nodes(ast, graph);
    
    // Analisa anotações e constrói dependências
    analyze_annotations(graph);
    
    // Constrói grafo de execução
    build_execution_graph(graph);
    
    return graph;
}

void declarative_graph_destroy(DeclarativeGraph* graph) {
    if (graph == NULL) return;
    
    // Destrói execution_graph interno PRIMEIRO
    // IMPORTANTE: O caller deve garantir que o scheduler já terminou
    // antes de chamar esta função, pois o scheduler pode estar usando este grafo
    // O scheduler mantém apenas uma referência, não possui o grafo
    if (graph->execution_graph != NULL) {
        // Verifica se o grafo já foi destruído (proteção extra)
        // Isso pode acontecer se graph_destroy foi chamado diretamente antes
        if (!graph->execution_graph->destroyed) {
            graph_destroy(graph->execution_graph);
        }
        // Marca como NULL mesmo se já foi destruído
        graph->execution_graph = NULL;
    }
    
    // Depois destrói os nós declarativos
    if (graph->nodes != NULL) {
        for (size_t i = 0; i < graph->node_count; i++) {
            if (graph->nodes[i].dependencies != NULL) {
                free(graph->nodes[i].dependencies);
                graph->nodes[i].dependencies = NULL;
            }
        }
        free(graph->nodes);
        graph->nodes = NULL;
    }
    
    graph->node_count = 0;
    graph->node_capacity = 0;
    
    free(graph);
}

void declarative_graph_build(DeclarativeGraph* graph) {
    if (graph == NULL) return;
    
    // Reconstrói grafo de execução
    if (graph->execution_graph != NULL) {
        graph_destroy(graph->execution_graph);
    }
    
    build_execution_graph(graph);
}

void declarative_graph_print(DeclarativeGraph* graph) {
    if (graph == NULL) {
        printf("Grafo Declarativo: NULL\n");
        return;
    }
    
    printf("\n=== Grafo Declarativo (Orientado a Grafos) ===\n");
    printf("Nós com anotações: %zu\n\n", graph->node_count);
    
    // CORREÇÃO CRÍTICA: Protege contra acesso a estrutura vazia
    if (graph->node_count == 0) {
        printf("(Grafo vazio - nenhum nó com anotações)\n");
        return;
    }
    
    // CORREÇÃO: Verifica se nodes é válido antes de iterar
    if (graph->nodes == NULL) {
        printf("(Erro: nodes é NULL)\n");
        return;
    }
    
    for (size_t i = 0; i < graph->node_count; i++) {
        DeclarativeNode* dnode = &graph->nodes[i];
        
        // CORREÇÃO: Verifica se dnode e ast_node são válidos
        if (dnode == NULL || dnode->ast_node == NULL) {
            printf("Nó %zu: (inválido)\n", i);
            continue;
        }
        
        const char* node_name = "unknown";
        
        if (dnode->ast_node->type == AST_VARIABLE_DECLARATION) {
            node_name = dnode->ast_node->as.variable_decl.name;
        } else if (dnode->ast_node->type == AST_PRINT) {
            node_name = "print";
        }
        
        printf("Nó %zu: %s\n", i, node_name);
        
        // Imprime anotações
        Annotation* annotation = dnode->annotations;
        while (annotation != NULL) {
            printf("  @%s", annotation_type_to_string(annotation->type));
            if (annotation->type == ANNOTATION_DEPENDS && annotation->dep_count > 0) {
                printf("(");
                for (size_t j = 0; j < annotation->dep_count; j++) {
                    if (j > 0) printf(", ");
                    printf("%s", annotation->dependencies[j]);
                }
                printf(")");
            }
            printf("\n");
            annotation = annotation->next;
        }
        
        // Imprime dependências
        if (dnode->dep_count > 0) {
            printf("  Dependências: ");
            for (size_t j = 0; j < dnode->dep_count; j++) {
                if (j > 0) printf(", ");
                printf("%zu", dnode->dependencies[j]);
            }
            printf("\n");
        }
        
        if (dnode->can_execute_parallel) {
            printf("  → Pode executar em paralelo\n");
        }
    }
    
    if (graph->execution_graph != NULL) {
        printf("\n--- Grafo de Execução Gerado ---\n");
        graph_print(graph->execution_graph);
    }
}

Graph* declarative_graph_get_execution_graph(DeclarativeGraph* graph) {
    return graph != NULL ? graph->execution_graph : NULL;
}
