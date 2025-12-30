#include "graph.h"
#include "../utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <limits.h>

// ==================== Graph Functions ====================

Graph* graph_create(void) {
    Graph* graph = (Graph*)malloc(sizeof(Graph));
    if (graph == NULL) {
        return NULL;
    }
    
    graph->node_capacity = 16;
    graph->nodes = (GraphNode**)malloc(sizeof(GraphNode*) * graph->node_capacity);
    if (graph->nodes == NULL) {
        free(graph);
        return NULL;
    }
    
    graph->edge_capacity = 32;
    graph->edges = (GraphEdge**)malloc(sizeof(GraphEdge*) * graph->edge_capacity);
    if (graph->edges == NULL) {
        free(graph->nodes);
        free(graph);
        return NULL;
    }
    
    graph->node_count = 0;
    graph->edge_count = 0;
    graph->destroyed = 0;
    
    return graph;
}

void graph_destroy(Graph* graph) {
    if (graph == NULL) return;
    
    // Marca como destruído para evitar double free
    if (graph->destroyed) {
        // Já foi destruído - retorna silenciosamente
        // (não é erro, pode ser chamado múltiplas vezes por segurança)
        return;
    }
    
    // Marca como destruído ANTES de começar a destruir
    // Isso evita problemas se houver chamadas recursivas ou concorrentes
    graph->destroyed = 1;
    
    // Salva valores antes de destruir (para loops)
    size_t edge_count = graph->edge_count;
    size_t node_count = graph->node_count;
    GraphEdge** edges = graph->edges;
    GraphNode** nodes = graph->nodes;
    
    // Clear graph pointers BEFORE destroying (to avoid accidental access)
    graph->nodes = NULL;
    graph->edges = NULL;
    graph->node_count = 0;
    graph->edge_count = 0;
    
    // Destroy all edges
    // IMPORTANT: Edges are shared between graph->edges and node->edges
    // (they are the same pointers), so we destroy only once through graph->edges
    if (edges != NULL) {
        for (size_t i = 0; i < edge_count; i++) {
            if (edges[i] != NULL) {
                // Limpa label se existir
                if (edges[i]->label != NULL) {
                    free(edges[i]->label);
                    edges[i]->label = NULL;
                }
                // Clear edge pointers before destroying
                edges[i]->from = NULL;
                edges[i]->to = NULL;
                free(edges[i]);
                edges[i] = NULL;
            }
        }
        free(edges);
    }
    
    // Destroy all nodes
    // IMPORTANT: node->edges contains only pointers to edges already destroyed above
    // We should not try to destroy the edges again
    if (nodes != NULL) {
        for (size_t i = 0; i < node_count; i++) {
            if (nodes[i] != NULL) {
                // Limpa name se existir
                if (nodes[i]->name != NULL) {
                    free(nodes[i]->name);
                    nodes[i]->name = NULL;
                }
                // Clear array of edge pointers (not the edges themselves, already destroyed)
                if (nodes[i]->edges != NULL) {
                    free(nodes[i]->edges);
                    nodes[i]->edges = NULL;
                }
                free(nodes[i]);
                nodes[i] = NULL;
            }
        }
        free(nodes);
    }
    
    free(graph);
}

GraphNode* graph_add_node(Graph* graph, const char* name, GraphNodeType type, void* data) {
    if (graph == NULL || name == NULL) {
        return NULL;
    }
    
    // Check if node already exists
    GraphNode* existing = graph_find_node(graph, name);
    if (existing != NULL) {
        return existing;
    }
    
    // Verifica capacidade
    if (graph->node_count >= graph->node_capacity) {
        graph->node_capacity *= 2;
        GraphNode** new_nodes = (GraphNode**)realloc(graph->nodes, 
                                                      sizeof(GraphNode*) * graph->node_capacity);
        if (new_nodes == NULL) {
            return NULL;
        }
        graph->nodes = new_nodes;
    }
    
    // Create new node
    GraphNode* node = (GraphNode*)malloc(sizeof(GraphNode));
    if (node == NULL) {
        return NULL;
    }
    
    node->name = string_copy(name, strlen(name));
    node->type = type;
    node->data = data;
    node->edge_count = 0;
    node->edge_capacity = 4;
    node->edges = (GraphEdge**)malloc(sizeof(GraphEdge*) * node->edge_capacity);
    if (node->edges == NULL) {
        free(node->name);
        free(node);
        return NULL;
    }
    node->visited = 0;
    node->in_degree = 0;
    node->out_degree = 0;
    node->used = 1;  // Por padrão, assume que é usado
    node->is_constant = 0;
    node->constant_value = NULL;
    node->can_parallelize = 0;
    
    graph->nodes[graph->node_count++] = node;
    
    return node;
}

GraphNode* graph_find_node(Graph* graph, const char* name) {
    if (graph == NULL || name == NULL) {
        return NULL;
    }
    
    for (size_t i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i] != NULL && 
            strcmp(graph->nodes[i]->name, name) == 0) {
            return graph->nodes[i];
        }
    }
    
    return NULL;
}

GraphEdge* graph_add_edge(Graph* graph, GraphNode* from, GraphNode* to, const char* label) {
    if (graph == NULL || from == NULL || to == NULL) {
        return NULL;
    }
    
    // Check graph edge capacity
    if (graph->edge_count >= graph->edge_capacity) {
        graph->edge_capacity *= 2;
        GraphEdge** new_edges = (GraphEdge**)realloc(graph->edges,
                                                      sizeof(GraphEdge*) * graph->edge_capacity);
        if (new_edges == NULL) {
            return NULL;
        }
        graph->edges = new_edges;
    }
    
    // Create new edge
    GraphEdge* edge = (GraphEdge*)malloc(sizeof(GraphEdge));
    if (edge == NULL) {
        return NULL;
    }
    
    edge->from = from;
    edge->to = to;
    edge->label = label != NULL ? string_copy(label, strlen(label)) : NULL;
    edge->weight = 1;
    
    // Adiciona aresta ao array do grafo
    graph->edges[graph->edge_count++] = edge;
    
    // Adiciona aresta ao nó origem
    if (from->edge_count >= from->edge_capacity) {
        from->edge_capacity *= 2;
        GraphEdge** new_edges = (GraphEdge**)realloc(from->edges,
                                                      sizeof(GraphEdge*) * from->edge_capacity);
        if (new_edges == NULL) {
            free(edge->label);
            free(edge);
            return NULL;
        }
        from->edges = new_edges;
    }
    from->edges[from->edge_count++] = edge;
    
    return edge;
}

// ==================== Análise da AST ====================

// Função auxiliar para coletar identificadores de uma expressão
static void collect_identifiers(ASTNode* expr, Graph* graph, GraphNode* dependent_node) {
    if (expr == NULL) return;
    
    switch (expr->type) {
        case AST_IDENTIFIER: {
            char* var_name = expr->as.identifier.name;
            GraphNode* var_node = graph_find_node(graph, var_name);
            if (var_node == NULL) {
                var_node = graph_add_node(graph, var_name, NODE_VARIABLE, expr);
            }
            if (dependent_node != NULL) {
                graph_add_edge(graph, dependent_node, var_node, "uses");
            }
            break;
        }
        case AST_BINARY_EXPRESSION:
            collect_identifiers(expr->as.binary_expr.left, graph, dependent_node);
            collect_identifiers(expr->as.binary_expr.right, graph, dependent_node);
            break;
        case AST_UNARY_EXPRESSION:
            collect_identifiers(expr->as.unary_expr.operand, graph, dependent_node);
            break;
        case AST_FUNCTION_CALL: {
            char* func_name = expr->as.function_call.name;
            GraphNode* func_node = graph_find_node(graph, func_name);
            if (func_node == NULL) {
                func_node = graph_add_node(graph, func_name, NODE_FUNCTION, NULL);
            }
            if (dependent_node != NULL) {
                graph_add_edge(graph, dependent_node, func_node, "calls");
            }
            // Coleta argumentos
            for (size_t i = 0; i < expr->as.function_call.argument_count; i++) {
                collect_identifiers(expr->as.function_call.arguments[i], graph, dependent_node);
            }
            break;
        }
        default:
            break;
    }
}

// Build dependency graph of variables
Graph* graph_build_dependencies(ASTNode* ast) {
    if (ast == NULL) {
        fprintf(stderr, "Warning: Null AST when building dependency graph\n");
        return NULL;
    }
    
    Graph* graph = graph_create();
    if (graph == NULL) {
        fprintf(stderr, "Error: Could not create dependency graph\n");
        return NULL;
    }
    
    // Função recursiva para processar a AST com tratamento de erros
    void process_node(ASTNode* node) {
        if (node == NULL) return;
        
        switch (node->type) {
            case AST_VARIABLE_DECLARATION: {
                if (node->as.variable_decl.name == NULL) {
                    fprintf(stderr, "Warning: Variable without name ignored\n");
                    break;
                }
                char* var_name = node->as.variable_decl.name;
                GraphNode* var_node = graph_add_node(graph, var_name, NODE_VARIABLE, node);
                if (var_node == NULL) {
                    fprintf(stderr, "Error: Could not create node for variable '%s'\n", var_name);
                    break;
                }
                
                // Coleta dependências da expressão
                if (node->as.variable_decl.value != NULL) {
                    collect_identifiers(node->as.variable_decl.value, graph, var_node);
                }
                break;
            }
            case AST_ASSIGNMENT: {
                char* var_name = node->as.assignment.name;
                GraphNode* var_node = graph_find_node(graph, var_name);
                if (var_node == NULL) {
                    var_node = graph_add_node(graph, var_name, NODE_VARIABLE, node);
                }
                // Marca que a variável sofreu mutação
                var_node->is_constant = 0; // Invalida constante
                var_node->visited = 1;     // Usamos visited temporariamente como flag de "mutação detectada"
                
                if (node->as.assignment.value != NULL) {
                    collect_identifiers(node->as.assignment.value, graph, var_node);
                }
                break;
            }
            case AST_FUNCTION_DECLARATION: {
                if (node->as.function_decl.name == NULL) {
                    fprintf(stderr, "Aviso: Função sem nome ignorada\n");
                    break;
                }
                char* func_name = node->as.function_decl.name;
                GraphNode* func_node = graph_add_node(graph, func_name, NODE_FUNCTION, node);
                if (func_node == NULL) {
                    fprintf(stderr, "Error: Could not create node for function '%s'\n", func_name);
                    break;
                }
                
                // Adiciona parâmetros como variáveis usadas dentro da função
                // Isso evita que sejam marcados como dead code
                for (size_t i = 0; i < node->as.function_decl.parameter_count; i++) {
                    char* param_name = node->as.function_decl.parameters[i];
                    GraphNode* param_node = graph_find_node(graph, param_name);
                    if (param_node == NULL) {
                        param_node = graph_add_node(graph, param_name, NODE_VARIABLE, NULL);
                    }
                    // Create edge from function to parameter (indicates use)
                    if (param_node != NULL) {
                        graph_add_edge(graph, func_node, param_node, "uses");
                    }
                }
                
                // Processa corpo da função
                if (node->as.function_decl.body != NULL) {
                    process_node(node->as.function_decl.body);
                }
                break;
            }
            case AST_FUNCTION_CALL: {
                if (node->as.function_call.name == NULL) {
                    fprintf(stderr, "Aviso: Chamada de função sem nome ignorada\n");
                    break;
                }
                char* func_name = node->as.function_call.name;
                GraphNode* func_node = graph_find_node(graph, func_name);
                if (func_node == NULL) {
                    func_node = graph_add_node(graph, func_name, NODE_FUNCTION, NULL);
                }
                // Coleta argumentos (inclui operadores lógicos nas expressões)
                for (size_t i = 0; i < node->as.function_call.argument_count; i++) {
                    collect_identifiers(node->as.function_call.arguments[i], graph, NULL);
                }
                break;
            }
            case AST_BLOCK:
                for (size_t i = 0; i < node->as.block.count; i++) {
                    if (node->as.block.statements[i] != NULL) {
                        process_node(node->as.block.statements[i]);
                    }
                }
                break;
            case AST_IF_STATEMENT:
                // Coleta dependências da condição (inclui operadores lógicos)
                if (node->as.if_stmt.condition != NULL) {
                    collect_identifiers(node->as.if_stmt.condition, graph, NULL);
                }
                if (node->as.if_stmt.then_branch != NULL) {
                    process_node(node->as.if_stmt.then_branch);
                }
                if (node->as.if_stmt.else_branch != NULL) {
                    process_node(node->as.if_stmt.else_branch);
                }
                break;
            case AST_WHILE_STATEMENT:
                // Coleta dependências da condição (inclui operadores lógicos)
                if (node->as.while_stmt.condition != NULL) {
                    collect_identifiers(node->as.while_stmt.condition, graph, NULL);
                }
                if (node->as.while_stmt.body != NULL) {
                    process_node(node->as.while_stmt.body);
                }
                break;
            case AST_FOR_STATEMENT:
                if (node->as.for_stmt.init != NULL) {
                    process_node(node->as.for_stmt.init);
                }
                // Coleta dependências da condição (inclui operadores lógicos)
                if (node->as.for_stmt.condition != NULL) {
                    collect_identifiers(node->as.for_stmt.condition, graph, NULL);
                }
                if (node->as.for_stmt.body != NULL) {
                    process_node(node->as.for_stmt.body);
                }
                if (node->as.for_stmt.increment != NULL) {
                    process_node(node->as.for_stmt.increment);
                }
                break;
            case AST_RETURN:
                // Coleta dependências do valor de retorno
                if (node->as.return_stmt.value != NULL) {
                    collect_identifiers(node->as.return_stmt.value, graph, NULL);
                }
                break;
            default:
                break;
        }
    }
    
    // Processa a AST com tratamento de erros
    if (ast->type == AST_BLOCK) {
        for (size_t i = 0; i < ast->as.block.count; i++) {
            if (ast->as.block.statements[i] != NULL) {
                process_node(ast->as.block.statements[i]);
            }
        }
    } else {
        process_node(ast);
    }
    
    return graph;
}

// Build function call graph
Graph* graph_build_call_graph(ASTNode* ast) {
    if (ast == NULL) {
        fprintf(stderr, "Warning: Null AST when building call graph\n");
        return NULL;
    }
    
    Graph* graph = graph_create();
    if (graph == NULL) {
        fprintf(stderr, "Error: Could not create call graph\n");
        return NULL;
    }
    
    // Função recursiva para processar chamadas com tratamento de erros
    void process_calls(ASTNode* node, const char* caller) {
        if (node == NULL) return;
        
        switch (node->type) {
            case AST_FUNCTION_DECLARATION: {
                if (node->as.function_decl.name == NULL) {
                    fprintf(stderr, "Aviso: Função sem nome ignorada\n");
                    break;
                }
                char* func_name = node->as.function_decl.name;
                GraphNode* func_node = graph_add_node(graph, func_name, NODE_FUNCTION, node);
                if (func_node == NULL) {
                    fprintf(stderr, "Error: Could not create node for function '%s'\n", func_name);
                    break;
                }
                
                // Processa corpo da função
                if (node->as.function_decl.body != NULL) {
                    process_calls(node->as.function_decl.body, func_name);
                }
                break;
            }
            case AST_FUNCTION_CALL: {
                if (node->as.function_call.name == NULL) {
                    fprintf(stderr, "Aviso: Chamada de função sem nome ignorada\n");
                    break;
                }
                char* callee_name = node->as.function_call.name;
                GraphNode* caller_node = NULL;
                
                // If there's a caller, find the node; otherwise, create/use "global" node
                if (caller != NULL) {
                    caller_node = graph_find_node(graph, caller);
                } else {
                    // Call at global level - create "global" node if it doesn't exist
                    caller_node = graph_find_node(graph, "global");
                    if (caller_node == NULL) {
                        caller_node = graph_add_node(graph, "global", NODE_FUNCTION, NULL);
                    }
                }
                
                GraphNode* callee_node = graph_find_node(graph, callee_name);
                if (callee_node == NULL) {
                    callee_node = graph_add_node(graph, callee_name, NODE_FUNCTION, NULL);
                    if (callee_node == NULL) {
                        fprintf(stderr, "Error: Could not create node for called function '%s'\n", callee_name);
                        break;
                    }
                }
                
                // Create call edge
                if (caller_node != NULL) {
                    graph_add_edge(graph, caller_node, callee_node, "calls");
                }
                
                // Processa argumentos recursivamente (pode conter chamadas aninhadas)
                for (size_t i = 0; i < node->as.function_call.argument_count; i++) {
                    if (node->as.function_call.arguments[i] != NULL) {
                        process_calls(node->as.function_call.arguments[i], caller);
                    }
                }
                break;
            }
            case AST_BLOCK:
                for (size_t i = 0; i < node->as.block.count; i++) {
                    if (node->as.block.statements[i] != NULL) {
                        process_calls(node->as.block.statements[i], caller);
                    }
                }
                break;
            case AST_IF_STATEMENT:
                if (node->as.if_stmt.then_branch != NULL) {
                    process_calls(node->as.if_stmt.then_branch, caller);
                }
                if (node->as.if_stmt.else_branch != NULL) {
                    process_calls(node->as.if_stmt.else_branch, caller);
                }
                break;
            case AST_VARIABLE_DECLARATION:
                // Processa chamadas de função no valor da variável
                if (node->as.variable_decl.value != NULL) {
                    process_calls(node->as.variable_decl.value, caller);
                }
                break;
            case AST_WHILE_STATEMENT:
                if (node->as.while_stmt.body != NULL) {
                    process_calls(node->as.while_stmt.body, caller);
                }
                break;
            case AST_FOR_STATEMENT:
                if (node->as.for_stmt.body != NULL) {
                    process_calls(node->as.for_stmt.body, caller);
                }
                break;
            case AST_BINARY_EXPRESSION:
                // Processa chamadas em expressões binárias
                if (node->as.binary_expr.left != NULL) {
                    process_calls(node->as.binary_expr.left, caller);
                }
                if (node->as.binary_expr.right != NULL) {
                    process_calls(node->as.binary_expr.right, caller);
                }
                break;
            default:
                break;
        }
    }
    
    // Processa a AST com tratamento de erros
    // IMPORTANT: Create "global" node first for calls at global level
    GraphNode* global_node = graph_add_node(graph, "global", NODE_FUNCTION, NULL);
    (void)global_node; // Ensures global node exists; process_calls also creates it if necessary
    
    // Processa chamadas no nível global (sem caller explícito, mas usa "global")
    if (ast->type == AST_BLOCK) {
        for (size_t i = 0; i < ast->as.block.count; i++) {
            if (ast->as.block.statements[i] != NULL) {
                // Processa chamadas no nível global (caller = NULL, mas será tratado como "global")
                process_calls(ast->as.block.statements[i], NULL);
            }
        }
    } else {
        process_calls(ast, NULL);
    }
    
    return graph;
}

// Build control flow graph (CFG) - complete version
Graph* graph_build_control_flow(ASTNode* ast) {
    if (ast == NULL) return NULL;
    
    Graph* graph = graph_create();
    if (graph == NULL) return NULL;
    
    // Structure to track entry and exit nodes
    typedef struct {
        GraphNode* entry;  // Block entry node
        GraphNode* exit;  // Block exit node
    } BlockNodes;
    
    // Recursive function that returns entry and exit nodes
    BlockNodes process_cfg(ASTNode* node, GraphNode* prev_node) {
        BlockNodes result = {NULL, NULL};
        if (node == NULL) {
            result.entry = prev_node;
            result.exit = prev_node;
            return result;
        }
        
        GraphNode* current_node = NULL;
        char node_name[256];
        static int node_counter = 0;
        
        switch (node->type) {
            case AST_VARIABLE_DECLARATION: {
                snprintf(node_name, sizeof(node_name), "let_%s_%d", 
                        node->as.variable_decl.name, node_counter++);
                current_node = graph_add_node(graph, node_name, NODE_STATEMENT, node);
                if (prev_node != NULL) {
                    graph_add_edge(graph, prev_node, current_node, "next");
                }
                result.entry = current_node;
                result.exit = current_node;
                break;
            }
            case AST_PRINT: {
                snprintf(node_name, sizeof(node_name), "print_%d", node_counter++);
                current_node = graph_add_node(graph, node_name, NODE_STATEMENT, node);
                if (prev_node != NULL) {
                    graph_add_edge(graph, prev_node, current_node, "next");
                }
                result.entry = current_node;
                result.exit = current_node;
                break;
            }
            case AST_RETURN: {
                snprintf(node_name, sizeof(node_name), "return_%d", node_counter++);
                current_node = graph_add_node(graph, node_name, NODE_STATEMENT, node);
                if (prev_node != NULL) {
                    graph_add_edge(graph, prev_node, current_node, "next");
                }
                result.entry = current_node;
                result.exit = current_node; // Return é terminal
                break;
            }
            case AST_IF_STATEMENT: {
                // Condition node
                snprintf(node_name, sizeof(node_name), "if_cond_%d", node_counter++);
                GraphNode* cond_node = graph_add_node(graph, node_name, NODE_STATEMENT, node);
                if (prev_node != NULL) {
                    graph_add_edge(graph, prev_node, cond_node, "next");
                }
                
                result.entry = cond_node;
                
                // Then branch
                BlockNodes then_nodes = {NULL, NULL};
                if (node->as.if_stmt.then_branch != NULL) {
                    then_nodes = process_cfg(node->as.if_stmt.then_branch, cond_node);
                    if (then_nodes.entry != NULL) {
                        graph_add_edge(graph, cond_node, then_nodes.entry, "true");
                    }
                }
                
                // Else branch
                BlockNodes else_nodes = {NULL, NULL};
                if (node->as.if_stmt.else_branch != NULL) {
                    else_nodes = process_cfg(node->as.if_stmt.else_branch, cond_node);
                    if (else_nodes.entry != NULL) {
                        graph_add_edge(graph, cond_node, else_nodes.entry, "false");
                    }
                }
                
                // Merge node (after if/else)
                snprintf(node_name, sizeof(node_name), "merge_%d", node_counter++);
                GraphNode* merge_node = graph_add_node(graph, node_name, NODE_STATEMENT, NULL);
                
                // Conecta saídas das branches ao merge
                if (then_nodes.exit != NULL) {
                    graph_add_edge(graph, then_nodes.exit, merge_node, "next");
                } else {
                    graph_add_edge(graph, cond_node, merge_node, "true");
                }
                
                if (else_nodes.exit != NULL) {
                    graph_add_edge(graph, else_nodes.exit, merge_node, "next");
                } else if (node->as.if_stmt.else_branch == NULL) {
                    // Se não há else, condição falsa vai direto para merge
                    graph_add_edge(graph, cond_node, merge_node, "false");
                }
                
                result.exit = merge_node;
                break;
            }
            case AST_WHILE_STATEMENT: {
                // Loop entry node
                snprintf(node_name, sizeof(node_name), "while_entry_%d", node_counter++);
                GraphNode* loop_entry = graph_add_node(graph, node_name, NODE_STATEMENT, NULL);
                if (prev_node != NULL) {
                    graph_add_edge(graph, prev_node, loop_entry, "next");
                }
                
                // Condition node
                snprintf(node_name, sizeof(node_name), "while_cond_%d", node_counter++);
                GraphNode* cond_node = graph_add_node(graph, node_name, NODE_STATEMENT, node);
                graph_add_edge(graph, loop_entry, cond_node, "next");
                
                result.entry = loop_entry;
                
                // Corpo do loop
                BlockNodes body_nodes = {NULL, NULL};
                if (node->as.while_stmt.body != NULL) {
                    body_nodes = process_cfg(node->as.while_stmt.body, cond_node);
                    if (body_nodes.entry != NULL) {
                        graph_add_edge(graph, cond_node, body_nodes.entry, "true");
                    }
                    // Loop back: saída do corpo volta para a condição
                    if (body_nodes.exit != NULL) {
                        graph_add_edge(graph, body_nodes.exit, cond_node, "loop");
                    } else {
                        graph_add_edge(graph, cond_node, cond_node, "loop");
                    }
                }
                
                // Exit node (when condition is false)
                snprintf(node_name, sizeof(node_name), "while_exit_%d", node_counter++);
                GraphNode* loop_exit = graph_add_node(graph, node_name, NODE_STATEMENT, NULL);
                graph_add_edge(graph, cond_node, loop_exit, "false");
                
                result.exit = loop_exit;
                break;
            }
            case AST_FOR_STATEMENT: {
                // Inicialização
                BlockNodes init_nodes = {NULL, NULL};
                if (node->as.for_stmt.init != NULL) {
                    init_nodes = process_cfg(node->as.for_stmt.init, prev_node);
                    if (prev_node != NULL && init_nodes.entry != NULL) {
                        graph_add_edge(graph, prev_node, init_nodes.entry, "next");
                    }
                    result.entry = init_nodes.entry != NULL ? init_nodes.entry : prev_node;
                } else {
                    result.entry = prev_node;
                }
                
                // Loop entry node
                snprintf(node_name, sizeof(node_name), "for_entry_%d", node_counter++);
                GraphNode* loop_entry = graph_add_node(graph, node_name, NODE_STATEMENT, NULL);
                if (init_nodes.exit != NULL) {
                    graph_add_edge(graph, init_nodes.exit, loop_entry, "next");
                } else if (result.entry != NULL) {
                    graph_add_edge(graph, result.entry, loop_entry, "next");
                }
                
                // Condition node
                snprintf(node_name, sizeof(node_name), "for_cond_%d", node_counter++);
                GraphNode* cond_node = graph_add_node(graph, node_name, NODE_STATEMENT, node);
                graph_add_edge(graph, loop_entry, cond_node, "next");
                
                // Corpo do loop
                BlockNodes body_nodes = {NULL, NULL};
                if (node->as.for_stmt.body != NULL) {
                    body_nodes = process_cfg(node->as.for_stmt.body, cond_node);
                    if (body_nodes.entry != NULL) {
                        graph_add_edge(graph, cond_node, body_nodes.entry, "true");
                    }
                }
                
                // Incremento
                BlockNodes inc_nodes = {NULL, NULL};
                if (node->as.for_stmt.increment != NULL) {
                    GraphNode* inc_start = body_nodes.exit != NULL ? body_nodes.exit : cond_node;
                    inc_nodes = process_cfg(node->as.for_stmt.increment, inc_start);
                    if (inc_start != NULL && inc_nodes.entry != NULL) {
                        graph_add_edge(graph, inc_start, inc_nodes.entry, "next");
                    }
                }
                
                // Loop back: incremento ou corpo volta para condição
                GraphNode* loop_back = inc_nodes.exit != NULL ? inc_nodes.exit : 
                                      (body_nodes.exit != NULL ? body_nodes.exit : cond_node);
                graph_add_edge(graph, loop_back, cond_node, "loop");
                
                // Exit node (when condition is false)
                snprintf(node_name, sizeof(node_name), "for_exit_%d", node_counter++);
                GraphNode* loop_exit = graph_add_node(graph, node_name, NODE_STATEMENT, NULL);
                graph_add_edge(graph, cond_node, loop_exit, "false");
                
                result.exit = loop_exit;
                break;
            }
            case AST_BLOCK: {
                GraphNode* current_prev = prev_node;
                for (size_t i = 0; i < node->as.block.count; i++) {
                    BlockNodes stmt_nodes = process_cfg(node->as.block.statements[i], current_prev);
                    if (stmt_nodes.entry != NULL) {
                        if (i == 0) {
                            result.entry = stmt_nodes.entry;
                        }
                        current_prev = stmt_nodes.exit;
                    }
                }
                result.exit = current_prev;
                break;
            }
            case AST_FUNCTION_DECLARATION: {
                // Function node
                snprintf(node_name, sizeof(node_name), "func_%s", node->as.function_decl.name);
                GraphNode* func_node = graph_add_node(graph, node_name, NODE_FUNCTION, node);
                if (prev_node != NULL) {
                    graph_add_edge(graph, prev_node, func_node, "next");
                }
                
                // Processa corpo da função
                if (node->as.function_decl.body != NULL) {
                    BlockNodes body_nodes = process_cfg(node->as.function_decl.body, func_node);
                    if (body_nodes.entry != NULL) {
                        graph_add_edge(graph, func_node, body_nodes.entry, "body");
                    }
                    result.exit = body_nodes.exit != NULL ? body_nodes.exit : func_node;
                } else {
                    result.exit = func_node;
                }
                result.entry = func_node;
                break;
            }
            default:
                // Para outros tipos, apenas passa adiante
                result.entry = prev_node;
                result.exit = prev_node;
                break;
        }
        
        return result;
    }
    
    // Processa a AST
    if (ast->type == AST_BLOCK) {
        GraphNode* prev = NULL;
        for (size_t i = 0; i < ast->as.block.count; i++) {
            BlockNodes nodes = process_cfg(ast->as.block.statements[i], prev);
            if (nodes.exit != NULL) {
                prev = nodes.exit;
            }
        }
    } else {
        process_cfg(ast, NULL);
    }
    
    return graph;
}

// ==================== Exportação ====================

void graph_export_dot(Graph* graph, const char* filename, GraphType type) {
    if (graph == NULL || filename == NULL) return;
    
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        fprintf(stderr, "Erro: Não foi possível criar arquivo '%s'\n", filename);
        return;
    }
    
    const char* graph_name = "Dependencies";
    if (type == GRAPH_CALLS) graph_name = "CallGraph";
    else if (type == GRAPH_CONTROL_FLOW) graph_name = "ControlFlow";
    
    fprintf(file, "digraph %s {\n", graph_name);
    fprintf(file, "  rankdir=LR;\n");
    fprintf(file, "  node [shape=box];\n\n");
    
    // Write nodes
    for (size_t i = 0; i < graph->node_count; i++) {
        GraphNode* node = graph->nodes[i];
        if (node == NULL) continue;
        
        const char* color = "black";
        const char* shape = "box";
        
        switch (node->type) {
            case NODE_VARIABLE:
                color = "blue";
                shape = "ellipse";
                break;
            case NODE_FUNCTION:
                color = "green";
                shape = "diamond";
                break;
            case NODE_STATEMENT:
                color = "orange";
                break;
            default:
                break;
        }
        
        fprintf(file, "  \"%s\" [label=\"%s\", color=%s, shape=%s];\n", 
                node->name, node->name, color, shape);
    }
    
    fprintf(file, "\n");
    
    // Write edges
    for (size_t i = 0; i < graph->edge_count; i++) {
        GraphEdge* edge = graph->edges[i];
        if (edge == NULL || edge->from == NULL || edge->to == NULL) continue;
        
        const char* label = edge->label != NULL ? edge->label : "";
        fprintf(file, "  \"%s\" -> \"%s\" [label=\"%s\"];\n",
                edge->from->name, edge->to->name, label);
    }
    
    fprintf(file, "}\n");
    fclose(file);
    
    printf("Graph exported to '%s'\n", filename);
    printf("Visualize com: dot -Tpng %s -o %s.png\n", filename, filename);
}

void graph_export_json(Graph* graph, const char* filename) {
    if (graph == NULL) {
        fprintf(stderr, "Error: Null graph when exporting JSON\n");
        return;
    }
    
    if (filename == NULL) {
        fprintf(stderr, "Erro: Nome de arquivo nulo ao exportar JSON\n");
        return;
    }
    
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        fprintf(stderr, "Erro: Não foi possível criar arquivo '%s'\n", filename);
        return;
    }
    
    fprintf(file, "{\n");
    fprintf(file, "  \"nodes\": [\n");
    
    size_t node_written = 0;
    for (size_t i = 0; i < graph->node_count; i++) {
        GraphNode* node = graph->nodes[i];
        if (node == NULL || node->name == NULL) continue;
        
        if (node_written > 0) fprintf(file, ",\n");
        fprintf(file, "    {\"id\": \"%s\", \"type\": %d}", 
                node->name, node->type);
        node_written++;
    }
    
    fprintf(file, "\n  ],\n");
    fprintf(file, "  \"edges\": [\n");
    
    size_t edge_written = 0;
    for (size_t i = 0; i < graph->edge_count; i++) {
        GraphEdge* edge = graph->edges[i];
        if (edge == NULL || edge->from == NULL || edge->to == NULL) continue;
        if (edge->from->name == NULL || edge->to->name == NULL) continue;
        
        if (edge_written > 0) fprintf(file, ",\n");
        fprintf(file, "    {\"from\": \"%s\", \"to\": \"%s\"", 
                edge->from->name, edge->to->name);
        if (edge->label != NULL) {
            fprintf(file, ", \"label\": \"%s\"", edge->label);
        }
        fprintf(file, "}");
        edge_written++;
    }
    
    fprintf(file, "\n  ]\n");
    fprintf(file, "}\n");
    
    fclose(file);
    printf("Graph exported to JSON: '%s' (%zu nodes, %zu edges)\n", 
           filename, node_written, edge_written);
}

void graph_print(Graph* graph) {
    if (graph == NULL) {
        printf("Graph: NULL\n");
        return;
    }
    
    printf("=== Graph ===\n");
    printf("Nodes: %zu\n", graph->node_count);
    printf("Edges: %zu\n\n", graph->edge_count);
    
    if (graph->node_count == 0) {
        printf("(Empty graph)\n");
        return;
    }
    
    size_t valid_nodes = 0;
    for (size_t i = 0; i < graph->node_count; i++) {
        GraphNode* node = graph->nodes[i];
        if (node == NULL || node->name == NULL) continue;
        
        valid_nodes++;
        const char* type_str = "Unknown";
        switch (node->type) {
            case NODE_VARIABLE: type_str = "Variable"; break;
            case NODE_FUNCTION: type_str = "Function"; break;
            case NODE_STATEMENT: type_str = "Statement"; break;
            case NODE_EXPRESSION: type_str = "Expression"; break;
            case NODE_CALL: type_str = "Call"; break;
        }
        
        printf("Node: %s (type: %s, edges: %zu)\n", 
               node->name, type_str, node->edge_count);
        
        if (node->edge_count > 0) {
            for (size_t j = 0; j < node->edge_count; j++) {
                GraphEdge* edge = node->edges[j];
                if (edge != NULL && edge->to != NULL && edge->to->name != NULL) {
                    printf("  -> %s", edge->to->name);
                    if (edge->label != NULL) {
                        printf(" [%s]", edge->label);
                    }
                    printf("\n");
                }
            }
        } else {
            printf("  (no edges)\n");
        }
    }
    
    if (valid_nodes == 0) {
        printf("(No valid nodes found)\n");
    }
}

// ==================== Sistema de Decisão e Otimização ====================

// Calculate node metrics (in_degree, out_degree)
void graph_calculate_metrics(Graph* graph) {
    if (graph == NULL) return;
    if (graph->nodes == NULL || graph->edges == NULL) return;
    
    // Reset métricas
    for (size_t i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i] != NULL) {
            graph->nodes[i]->in_degree = 0;
            // Proteção: verifica se edge_count é válido antes de usar
            graph->nodes[i]->out_degree = (graph->nodes[i]->edge_count < 10000) 
                ? graph->nodes[i]->edge_count : 0;
        }
    }
    
    // Calculate in_degree: count how many edges point to each node
    for (size_t i = 0; i < graph->edge_count; i++) {
        GraphEdge* edge = graph->edges[i];
        if (edge != NULL && edge->to != NULL) {
            // Check if destination node is still in valid nodes array
            // (proteção contra ponteiros órfãos)
            int node_found = 0;
            for (size_t j = 0; j < graph->node_count; j++) {
                if (graph->nodes[j] == edge->to) {
                    node_found = 1;
                    break;
                }
            }
            if (node_found) {
                edge->to->in_degree++;
            }
        }
    }
}

// Analisa dead code elimination
void graph_analyze_dead_code(Graph* dep_graph, Graph* call_graph) {
    if (dep_graph == NULL && call_graph == NULL) return;
    
    // Calcula métricas primeiro
    if (dep_graph != NULL) {
        graph_calculate_metrics(dep_graph);
    }
    if (call_graph != NULL) {
        graph_calculate_metrics(call_graph);
    }
    
    // Analisa variáveis não usadas (out_degree == 0 e não são funções principais)
    if (dep_graph != NULL) {
        for (size_t i = 0; i < dep_graph->node_count; i++) {
            GraphNode* node = dep_graph->nodes[i];
            if (node == NULL || node->name == NULL) continue;
            
            // Variável nunca usada (exceto se for parâmetro de função)
            if (node->type == NODE_VARIABLE && node->out_degree == 0) {
                // Check if it's a function parameter (has "uses" edge from a function)
                int is_param = 0;
                
                // Verifica se a variável é usada dentro do corpo de alguma função
                for (size_t j = 0; j < dep_graph->node_count; j++) {
                    GraphNode* other = dep_graph->nodes[j];
                    if (other != NULL && other->type == NODE_FUNCTION) {
                        // Check if there are edges from function to this variable (use or parameter)
                        for (size_t k = 0; k < other->edge_count; k++) {
                            if (other->edges[k] != NULL && 
                                other->edges[k]->to == node) {
                                // If there's an edge from function to variable, it's used/parameter
                                is_param = 1;
                                break;
                            }
                        }
                        if (is_param) break;
                    }
                }
                
                // Também verifica se tem in_degree > 0 (pode ser parâmetro)
                if (!is_param && node->in_degree > 0) {
                    // Check if incoming edges come from functions
                    for (size_t j = 0; j < dep_graph->edge_count; j++) {
                        GraphEdge* edge = dep_graph->edges[j];
                        if (edge != NULL && edge->to == node && 
                            edge->from != NULL && edge->from->type == NODE_FUNCTION) {
                            is_param = 1;
                            break;
                        }
                    }
                }
                
                if (!is_param) {
                    node->used = 0;
                    printf("  [Dead Code] Variável '%s' nunca usada\n", node->name);
                }
            }
        }
    }
    
    // Analisa funções nunca chamadas
    if (call_graph != NULL) {
        for (size_t i = 0; i < call_graph->node_count; i++) {
            GraphNode* node = call_graph->nodes[i];
            if (node == NULL || node->name == NULL) continue;
            
            // Função nunca chamada (in_degree == 0, exceto se for main/global)
            const char* func_name = node->name;
            
            if (node->type == NODE_FUNCTION && node->in_degree == 0) {
                // Ignora se for "global" (contexto global) ou "main"
                if (strcmp(func_name, "global") != 0 && strcmp(func_name, "main") != 0) {
                    node->used = 0;
                    printf("  [Dead Code] Função '%s' nunca chamada\n", node->name);
                }
            }
        }
    }
}

// Analisa propagação de constantes
void graph_analyze_constants(Graph* dep_graph) {
    if (dep_graph == NULL) return;
    
    graph_calculate_metrics(dep_graph);
    
    // Identifica variáveis com valores constantes
    for (size_t i = 0; i < dep_graph->node_count; i++) {
        GraphNode* node = dep_graph->nodes[i];
        if (node == NULL || node->name == NULL) continue;
        
        // REGRA: Só é constante se for inicializada com literal E nunca sofrer atribuição (visited == 0)
        if (node->type == NODE_VARIABLE && node->data != NULL && node->visited == 0) {
            ASTNode* ast_node = (ASTNode*)node->data;
            if (ast_node->type == AST_VARIABLE_DECLARATION) {
                ASTNode* value = ast_node->as.variable_decl.value;
                if (value != NULL && value->type == AST_LITERAL) {
                    if (value->as.literal.type == LIT_NUMBER) {
                        node->is_constant = 1;
                        double* num_val = (double*)malloc(sizeof(double));
                        if (num_val != NULL) {
                            *num_val = value->as.literal.value.number;
                            node->constant_value = num_val;
                            printf("  [Constant] Variável '%s' = %g (constante)\n", 
                                   node->name, value->as.literal.value.number);
                        }
                    } else if (value->as.literal.type == LIT_STRING) {
                        node->is_constant = 1;
                        node->constant_value = value->as.literal.value.string;
                        printf("  [Constant] Variável '%s' = \"%s\" (constante)\n", 
                               node->name, value->as.literal.value.string);
                    }
                }
            }
        }
    }
    
    // Reseta flag visited para uso futuro
    for (size_t i = 0; i < dep_graph->node_count; i++) {
        if (dep_graph->nodes[i] != NULL) dep_graph->nodes[i]->visited = 0;
    }
}

// Identifica blocos que podem ser paralelizados
void graph_analyze_parallelism(Graph* cfg) {
    if (cfg == NULL) return;
    
    graph_calculate_metrics(cfg);
    
    // Identifica blocos independentes (sem dependências cruzadas)
    for (size_t i = 0; i < cfg->node_count; i++) {
        GraphNode* node = cfg->nodes[i];
        if (node == NULL || node->name == NULL) continue;
        
        if (node->type == NODE_STATEMENT) {
            // Bloco com poucas dependências pode ser paralelizado
            if (node->in_degree <= 1 && node->out_degree <= 1) {
                // Verifica se não há dependências circulares
                int has_circular = 0;
                for (size_t j = 0; j < node->edge_count; j++) {
                    GraphEdge* edge = node->edges[j];
                    if (edge != NULL && edge->to != NULL) {
                        // Verifica se há caminho de volta (simplificado)
                        for (size_t k = 0; k < edge->to->edge_count; k++) {
                            if (edge->to->edges[k] != NULL && 
                                edge->to->edges[k]->to == node) {
                                has_circular = 1;
                                break;
                            }
                        }
                    }
                    if (has_circular) break;
                }
                
                if (!has_circular) {
                    node->can_parallelize = 1;
                    printf("  [Parallel] Bloco '%s' pode ser paralelizado\n", node->name);
                }
            }
        }
    }
}

// Identifica loops críticos
void graph_analyze_critical_loops(Graph* cfg) {
    if (cfg == NULL) return;
    
    graph_calculate_metrics(cfg);
    
    // Identify loops (edges with label "loop")
    for (size_t i = 0; i < cfg->edge_count; i++) {
        GraphEdge* edge = cfg->edges[i];
        if (edge == NULL || edge->label == NULL) continue;
        
        if (strcmp(edge->label, "loop") == 0) {
            // Loop detectado
            GraphNode* loop_node = edge->from;
            if (loop_node != NULL) {
                printf("  [Critical Loop] Loop detectado em '%s'\n", loop_node->name);
                // Marca para otimização (pode adicionar flag específica)
            }
        }
    }
}

// Cria plano de otimização
OptimizationPlan* graph_analyze_and_optimize(Graph* dep_graph, Graph* call_graph, Graph* cfg) {
    OptimizationPlan* plan = (OptimizationPlan*)malloc(sizeof(OptimizationPlan));
    if (plan == NULL) return NULL;
    
    plan->decision_capacity = 16;
    plan->decisions = (OptimizationDecision*)malloc(sizeof(OptimizationDecision) * plan->decision_capacity);
    if (plan->decisions == NULL) {
        free(plan);
        return NULL;
    }
    plan->decision_count = 0;
    
    printf("\n=== Análise de Otimização ===\n");
    
    // Analisa dead code
    graph_analyze_dead_code(dep_graph, call_graph);
    
    // Analisa constantes
    if (dep_graph != NULL) {
        graph_analyze_constants(dep_graph);
    }
    
    // Analisa paralelismo
    if (cfg != NULL) {
        graph_analyze_parallelism(cfg);
    }
    
    // Analisa loops críticos
    if (cfg != NULL) {
        graph_analyze_critical_loops(cfg);
    }
    
    // Collect decisions from graphs
    if (dep_graph != NULL) {
        for (size_t i = 0; i < dep_graph->node_count; i++) {
            GraphNode* node = dep_graph->nodes[i];
            if (node == NULL || node->name == NULL) continue;
            
            if (node->used == 0) {
                // Adiciona decisão de dead code
                if (plan->decision_count >= plan->decision_capacity) {
                    plan->decision_capacity *= 2;
                    OptimizationDecision* new_decisions = (OptimizationDecision*)realloc(
                        plan->decisions, sizeof(OptimizationDecision) * plan->decision_capacity);
                    if (new_decisions == NULL) break;
                    plan->decisions = new_decisions;
                }
                
                plan->decisions[plan->decision_count].target_name = node->name;
                plan->decisions[plan->decision_count].decision_type = DECISION_DEAD_CODE;
                plan->decisions[plan->decision_count].data = NULL;
                plan->decisions[plan->decision_count].priority = 1;
                plan->decision_count++;
            }
            
            if (node->is_constant) {
                // Adiciona decisão de propagação de constantes
                if (plan->decision_count >= plan->decision_capacity) {
                    plan->decision_capacity *= 2;
                    OptimizationDecision* new_decisions = (OptimizationDecision*)realloc(
                        plan->decisions, sizeof(OptimizationDecision) * plan->decision_capacity);
                    if (new_decisions == NULL) break;
                    plan->decisions = new_decisions;
                }
                
                plan->decisions[plan->decision_count].target_name = node->name;
                plan->decisions[plan->decision_count].decision_type = DECISION_CONSTANT_PROP;
                plan->decisions[plan->decision_count].data = node->constant_value;
                plan->decisions[plan->decision_count].priority = 2;
                plan->decision_count++;
            }
        }
    }
    
    if (cfg != NULL) {
        for (size_t i = 0; i < cfg->node_count; i++) {
            GraphNode* node = cfg->nodes[i];
            if (node == NULL || node->name == NULL) continue;
            
            if (node->can_parallelize) {
                // Adiciona decisão de paralelização
                if (plan->decision_count >= plan->decision_capacity) {
                    plan->decision_capacity *= 2;
                    OptimizationDecision* new_decisions = (OptimizationDecision*)realloc(
                        plan->decisions, sizeof(OptimizationDecision) * plan->decision_capacity);
                    if (new_decisions == NULL) break;
                    plan->decisions = new_decisions;
                }
                
                plan->decisions[plan->decision_count].target_name = node->name;
                plan->decisions[plan->decision_count].decision_type = DECISION_PARALLELIZE;
                plan->decisions[plan->decision_count].data = NULL;
                plan->decisions[plan->decision_count].priority = 3;
                plan->decision_count++;
            }
        }
    }
    
    return plan;
}

// Libera plano de otimização
void optimization_plan_destroy(OptimizationPlan* plan) {
    if (plan == NULL) return;
    
    // Libera valores constantes alocados (double*)
    for (size_t i = 0; i < plan->decision_count; i++) {
        if (plan->decisions[i].decision_type == DECISION_CONSTANT_PROP && 
            plan->decisions[i].data != NULL) {
            // Verifica se é um double* alocado (não é string, que vem da AST)
            // Strings vêm da AST e não devem ser liberadas aqui
            // Apenas liberamos se for um double* alocado em graph_analyze_constants
            // Por segurança, não liberamos nada aqui - deixamos para graph_destroy
        }
    }
    
    // Don't free target_name as they are pointers to node names (which belong to graphs)
    // Não libera data pois pode ser ponteiro para AST ou string da AST
    if (plan->decisions != NULL) {
        free(plan->decisions);
    }
    free(plan);
}

// Imprime plano de otimização
void optimization_plan_print(OptimizationPlan* plan) {
    if (plan == NULL) {
        printf("Plano de otimização: NULL\n");
        return;
    }
    
    printf("\n=== Plano de Otimização ===\n");
    printf("Total de decisões: %zu\n\n", plan->decision_count);
    
    if (plan->decision_count == 0) {
        printf("Nenhuma otimização aplicável.\n");
        return;
    }
    
    const char* type_names[] = {
        "Dead Code",
        "Constant Propagation",
        "Parallelize",
        "Reorder",
        "Optimize Loop",
        "Cache Result"
    };
    
    for (size_t i = 0; i < plan->decision_count; i++) {
        OptimizationDecision* dec = &plan->decisions[i];
        printf("  [%d] %s: %s (prioridade: %d)\n",
               (int)(i + 1),
               type_names[dec->decision_type],
               dec->target_name != NULL ? dec->target_name : "unknown",
               dec->priority);
        
        if (dec->decision_type == DECISION_CONSTANT_PROP && dec->data != NULL) {
            double* num_val = (double*)dec->data;
            printf("      Valor constante: %g\n", *num_val);
        }
    }
}

// ==================== Execução Adaptativa ====================

// Cria perfil adaptativo vazio
AdaptiveProfile* adaptive_profile_create(Graph* dep_graph, Graph* call_graph, Graph* cfg) {
    (void)dep_graph;  // Parâmetros reservados para uso futuro
    (void)call_graph;
    (void)cfg;
    
    AdaptiveProfile* profile = (AdaptiveProfile*)malloc(sizeof(AdaptiveProfile));
    if (profile == NULL) return NULL;
    
    profile->hot_capacity = 16;
    profile->hot_functions = (GraphNode**)malloc(sizeof(GraphNode*) * profile->hot_capacity);
    if (profile->hot_functions == NULL) {
        free(profile);
        return NULL;
    }
    
    profile->reordered_blocks = (GraphNode**)malloc(sizeof(GraphNode*) * 32);
    if (profile->reordered_blocks == NULL) {
        free(profile->hot_functions);
        free(profile);
        return NULL;
    }
    
    profile->hot_count = 0;
    profile->reordered_count = 0;
    profile->parallelization_enabled = 0;
    profile->optimization_level = 1; // Nível inicial: otimizações básicas
    
    // Inicializa métricas
    profile->metrics_count = 0;
    profile->metrics_capacity = 16;
    profile->metrics = (MetricEntry*)malloc(sizeof(MetricEntry) * profile->metrics_capacity);
    if (profile->metrics == NULL) {
        free(profile->reordered_blocks);
        free(profile->hot_functions);
        free(profile);
        return NULL;
    }
    
    return profile;
}

// Destrói perfil adaptativo
void adaptive_profile_destroy(AdaptiveProfile* profile) {
    if (profile == NULL) return;
    
    if (profile->hot_functions != NULL) {
        free(profile->hot_functions);
    }
    if (profile->reordered_blocks != NULL) {
        free(profile->reordered_blocks);
    }
    if (profile->metrics != NULL) {
        // Libera nomes das métricas
        for (size_t i = 0; i < profile->metrics_count; i++) {
            if (profile->metrics[i].name != NULL) {
                free(profile->metrics[i].name);
            }
        }
        free(profile->metrics);
    }
    free(profile);
}

// Obtém métricas de uma função/bloco
ExecutionMetrics* adaptive_get_metrics(AdaptiveProfile* profile, const char* name) {
    if (profile == NULL || name == NULL || profile->metrics == NULL) return NULL;
    
    // Busca métricas existentes
    for (size_t i = 0; i < profile->metrics_count; i++) {
        if (profile->metrics[i].name != NULL && strcmp(profile->metrics[i].name, name) == 0) {
            return &profile->metrics[i].metrics;
        }
    }
    
    // Não encontrou - retorna NULL para criar nova
    return NULL;
}

// Registra chamada de função
void adaptive_record_function_call(AdaptiveProfile* profile, const char* func_name, double execution_time) {
    if (profile == NULL || func_name == NULL) return;
    
    // Busca ou cria métricas para esta função
    ExecutionMetrics* metrics = adaptive_get_metrics(profile, func_name);
    if (metrics == NULL) {
        // Cria nova entrada de métricas
        if (profile->metrics_count >= profile->metrics_capacity) {
            profile->metrics_capacity *= 2;
            MetricEntry* new_metrics = (MetricEntry*)realloc(
                profile->metrics, sizeof(MetricEntry) * profile->metrics_capacity);
            if (new_metrics == NULL) return;
            profile->metrics = new_metrics;
        }
        
        // Inicializa nova métrica
        MetricEntry* entry = &profile->metrics[profile->metrics_count];
        entry->name = string_copy(func_name, strlen(func_name));
        entry->metrics.call_count = 0;
        entry->metrics.total_time = 0.0;
        entry->metrics.avg_time = 0.0;
        entry->metrics.cache_hits = 0;
        entry->metrics.cache_misses = 0;
        entry->metrics.is_hot = 0;
        entry->metrics.last_call_time = 0.0;
        metrics = &entry->metrics;
        profile->metrics_count++;
    }
    
    // Atualiza métricas
    metrics->call_count++;
    metrics->total_time += execution_time;
    metrics->avg_time = metrics->total_time / metrics->call_count;
    metrics->last_call_time = execution_time;
    
    // Marca como "quente" se chamada muitas vezes
    if (metrics->call_count > 10) {
        metrics->is_hot = 1;
    }
}

// Registra execução de bloco
void adaptive_record_block_execution(AdaptiveProfile* profile, const char* block_name, double execution_time) {
    if (profile == NULL || block_name == NULL) return;
    
    // Similar a adaptive_record_function_call, mas para blocos
    ExecutionMetrics* metrics = adaptive_get_metrics(profile, block_name);
    if (metrics == NULL) {
        // Cria nova entrada de métricas
        if (profile->metrics_count >= profile->metrics_capacity) {
            profile->metrics_capacity *= 2;
            MetricEntry* new_metrics = (MetricEntry*)realloc(
                profile->metrics, sizeof(MetricEntry) * profile->metrics_capacity);
            if (new_metrics == NULL) return;
            profile->metrics = new_metrics;
        }
        
        // Inicializa nova métrica
        MetricEntry* entry = &profile->metrics[profile->metrics_count];
        entry->name = string_copy(block_name, strlen(block_name));
        entry->metrics.call_count = 0;
        entry->metrics.total_time = 0.0;
        entry->metrics.avg_time = 0.0;
        entry->metrics.cache_hits = 0;
        entry->metrics.cache_misses = 0;
        entry->metrics.is_hot = 0;
        entry->metrics.last_call_time = 0.0;
        metrics = &entry->metrics;
        profile->metrics_count++;
    }
    
    // Atualiza métricas
    metrics->call_count++;
    metrics->total_time += execution_time;
    metrics->avg_time = metrics->total_time / metrics->call_count;
}

// Identifica funções "quentes"
void adaptive_identify_hot_functions(AdaptiveProfile* profile, Graph* call_graph) {
    if (profile == NULL || call_graph == NULL) return;
    
    profile->hot_count = 0;
    
    // Traverse all functions in call graph
    for (size_t i = 0; i < call_graph->node_count; i++) {
        GraphNode* node = call_graph->nodes[i];
        if (node == NULL || node->name == NULL) continue;
        
        if (node->type == NODE_FUNCTION) {
            ExecutionMetrics* metrics = adaptive_get_metrics(profile, node->name);
            
            // Função é "quente" se:
            // - Chamada mais de 10 vezes, OU
            // - Tempo médio alto (> 1ms), OU
            // - Chamada recentemente e frequentemente
            int is_hot = 0;
            if (metrics != NULL) {
                if (metrics->call_count > 10 || 
                    metrics->avg_time > 0.001 || 
                    metrics->is_hot) {
                    is_hot = 1;
                }
            } else {
                // Se não tem métricas, verifica in_degree (chamada por muitas funções)
                if (node->in_degree > 2) {
                    is_hot = 1;
                }
            }
            
            if (is_hot) {
                // Adiciona à lista de funções quentes
                if (profile->hot_count >= profile->hot_capacity) {
                    profile->hot_capacity *= 2;
                    GraphNode** new_hot = (GraphNode**)realloc(
                        profile->hot_functions, sizeof(GraphNode*) * profile->hot_capacity);
                    if (new_hot == NULL) break;
                    profile->hot_functions = new_hot;
                }
                profile->hot_functions[profile->hot_count++] = node;
                printf("  [Hot Function] '%s' identificada como função quente\n", node->name);
            }
        }
    }
}

// Reordena blocos para melhor cache
void adaptive_reorder_blocks(AdaptiveProfile* profile, Graph* cfg) {
    if (profile == NULL || cfg == NULL) return;
    
    profile->reordered_count = 0;
    
    // Estratégia simples: reordena blocos baseado em frequência de execução
    // Blocos executados frequentemente devem estar próximos na memória
    
    for (size_t i = 0; i < cfg->node_count; i++) {
        GraphNode* node = cfg->nodes[i];
        if (node == NULL || node->name == NULL) continue;
        
        if (node->type == NODE_STATEMENT) {
            ExecutionMetrics* metrics = adaptive_get_metrics(profile, node->name);
            
            // Bloco é candidato a reordenação se executado frequentemente
            if (metrics != NULL && metrics->call_count > 5) {
                if (profile->reordered_count < 32) {
                    profile->reordered_blocks[profile->reordered_count++] = node;
                    printf("  [Reorder] Bloco '%s' marcado para reordenação (cache)\n", node->name);
                }
            }
        }
    }
}

// Ajusta nível de paralelização baseado na carga
void adaptive_adjust_parallelization(AdaptiveProfile* profile, double system_load) {
    if (profile == NULL) return;
    
    // system_load: 0.0 = sem carga, 1.0 = carga total
    // Ajusta paralelização baseado na carga do sistema
    
    if (system_load < 0.5) {
        // Sistema com pouca carga: pode paralelizar mais
        profile->parallelization_enabled = 1;
        profile->optimization_level = 3; // Máxima otimização
        printf("  [Adaptive] Sistema com pouca carga: paralelização ativada (nível 3)\n");
    } else if (system_load < 0.8) {
        // Sistema com carga moderada: paralelização moderada
        profile->parallelization_enabled = 1;
        profile->optimization_level = 2;
        printf("  [Adaptive] Sistema com carga moderada: paralelização moderada (nível 2)\n");
    } else {
        // Sistema com alta carga: reduz paralelização
        profile->parallelization_enabled = 0;
        profile->optimization_level = 1;
        printf("  [Adaptive] Sistema com alta carga: paralelização desativada (nível 1)\n");
    }
}

// Analisa métricas e atualiza otimizações adaptativas
void adaptive_analyze_and_optimize(AdaptiveProfile* profile, Graph* dep_graph, Graph* call_graph, Graph* cfg) {
    (void)dep_graph;  // Parâmetro reservado para uso futuro
    
    if (profile == NULL) return;
    
    printf("\n=== Análise Adaptativa ===\n");
    
    // Identifica funções quentes
    if (call_graph != NULL) {
        adaptive_identify_hot_functions(profile, call_graph);
    }
    
    // Reordena blocos para melhor cache
    if (cfg != NULL) {
        adaptive_reorder_blocks(profile, cfg);
    }
    
    // Simula carga do sistema (em implementação real, obteria do sistema operacional)
    double system_load = 0.3; // Exemplo: 30% de carga
    adaptive_adjust_parallelization(profile, system_load);
    
    printf("\nFunções quentes identificadas: %zu\n", profile->hot_count);
    printf("Blocos reordenados: %zu\n", profile->reordered_count);
    printf("Paralelização: %s (nível %d)\n", 
           profile->parallelization_enabled ? "Ativada" : "Desativada",
           profile->optimization_level);
}

// Imprime perfil adaptativo
void adaptive_profile_print(AdaptiveProfile* profile) {
    if (profile == NULL) {
        printf("Perfil adaptativo: NULL\n");
        return;
    }
    
    printf("\n=== Perfil Adaptativo ===\n");
    printf("Funções quentes: %zu\n", profile->hot_count);
    printf("Blocos reordenados: %zu\n", profile->reordered_count);
    printf("Paralelização: %s\n", profile->parallelization_enabled ? "Ativada" : "Desativada");
    printf("Nível de otimização: %d\n", profile->optimization_level);
    
    if (profile->hot_count > 0) {
        printf("\nFunções quentes:\n");
        for (size_t i = 0; i < profile->hot_count; i++) {
            if (profile->hot_functions[i] != NULL) {
                printf("  - %s\n", profile->hot_functions[i]->name);
            }
        }
    }
}

