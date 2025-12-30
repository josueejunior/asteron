#define _POSIX_C_SOURCE 200809L
#include "ssa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../utils/utils.h"

#define MAX_SSA_VARS 100

// Tabela SSA aprimorada com Pilha para versionamento real
#define MAX_VERSIONS 10

// Estrutura para rastrear quem usa qual variável (Inverse Dependency)
typedef struct {
    size_t task_id;        // ID da task que consome a variável
    int ssa_version;       // Versão específica consumida
} VariableUser;

#define MAX_USERS 10
typedef struct {
    char* name;
    int version_stack[MAX_VERSIONS];
    int stack_top;
    int total_versions;
    
    // Rastreamento de dependência inversa
    VariableUser users[MAX_USERS];
    int user_count;
} SSAVar;

static SSAVar ssa_table[MAX_SSA_VARS];
static int ssa_var_count = 0;

static SSAVar* find_or_create_var(const char* name) {
    for (int i = 0; i < ssa_var_count; i++) {
        if (strcmp(ssa_table[i].name, name) == 0) return &ssa_table[i];
    }
    if (ssa_var_count < MAX_SSA_VARS) {
        SSAVar* var = &ssa_table[ssa_var_count++];
        var->name = string_copy(name, strlen(name));
        var->stack_top = -1;
        var->total_versions = 0;
        var->user_count = 0; // Inicializa contador de usuários
        return var;
    }
    return NULL;
}

static void register_variable_use(const char* name, int version, size_t task_id) {
    SSAVar* var = find_or_create_var(name);
    if (var != NULL && var->user_count < MAX_USERS) {
        var->users[var->user_count].task_id = task_id;
        var->users[var->user_count].ssa_version = version;
        var->user_count++;
    }
}

static int push_version(const char* name) {
    SSAVar* var = find_or_create_var(name);
    if (var && var->stack_top < MAX_VERSIONS - 1) {
        int new_ver = var->total_versions++;
        var->version_stack[++var->stack_top] = new_ver;
        return new_ver;
    }
    return -1;
}

static int current_version(const char* name) {
    SSAVar* var = find_or_create_var(name);
    if (var && var->stack_top >= 0) {
        return var->version_stack[var->stack_top];
    }
    return -1;
}

static void transform_recursive(ASTNode* node) {
    if (node == NULL) return;

    switch (node->type) {
        case AST_VARIABLE_DECLARATION: {
            transform_recursive(node->as.variable_decl.value);
            // Definição: gera nova versão
            node->as.variable_decl.ssa_version = push_version(node->as.variable_decl.name);
            break;
        }

        case AST_IDENTIFIER: {
            // Uso: pega a versão atual do topo da pilha
            int ver = current_version(node->as.identifier.name);
            node->as.identifier.ssa_version = ver;
            
            // Registra este uso para o motor de invalidação reativa
            register_variable_use(node->as.identifier.name, ver, 0); // 0 = task_id genérico
            break;
        }

        case AST_WHILE_STATEMENT: {
            // No início de um loop, injetaríamos um PHI aqui
            transform_recursive(node->as.while_stmt.condition);
            transform_recursive(node->as.while_stmt.body);
            break;
        }

        case AST_BLOCK:
            for (size_t i = 0; i < node->as.block.count; i++) {
                transform_recursive(node->as.block.statements[i]);
            }
            break;

        case AST_ASSIGNMENT:
            transform_recursive(node->as.assignment.value);
            push_version(node->as.assignment.name);
            break;

        case AST_BINARY_EXPRESSION:
            transform_recursive(node->as.binary_expr.left);
            transform_recursive(node->as.binary_expr.right);
            break;

        case AST_PRINT:
            transform_recursive(node->as.print_stmt.expression);
            break;

        default:
            break;
    }
}

void ssa_transform(ASTNode* node) {
    printf("\n=== Fase: Transformação SSA (Ownership Foundation) ===\n");
    ssa_var_count = 0;
    transform_recursive(node);
    printf("✓ Variáveis versionadas com sucesso\n");
}

// Verifica se uma variável (por nome) é usada em um nó (recursivo)
static int is_var_used_in(ASTNode* node, const char* name) {
    if (node == NULL) return 0;
    if (node->type == AST_IDENTIFIER && strcmp(node->as.identifier.name, name) == 0) return 1;
    
    switch (node->type) {
        case AST_BINARY_EXPRESSION:
            return is_var_used_in(node->as.binary_expr.left, name) || is_var_used_in(node->as.binary_expr.right, name);
        case AST_UNARY_EXPRESSION:
            return is_var_used_in(node->as.unary_expr.operand, name);
        case AST_PRINT:
            return is_var_used_in(node->as.print_stmt.expression, name);
        case AST_ASSIGNMENT:
            return strcmp(node->as.assignment.name, name) == 0 || is_var_used_in(node->as.assignment.value, name);
        case AST_BLOCK:
            for (size_t i = 0; i < node->as.block.count; i++)
                if (is_var_used_in(node->as.block.statements[i], name)) return 1;
            break;
        case AST_IF_STATEMENT:
            return is_var_used_in(node->as.if_stmt.condition, name) || 
                   is_var_used_in(node->as.if_stmt.then_branch, name) || 
                   is_var_used_in(node->as.if_stmt.else_branch, name);
        case AST_WHILE_STATEMENT:
            return is_var_used_in(node->as.while_stmt.condition, name) || is_var_used_in(node->as.while_stmt.body, name);
        default: break;
    }
    return 0;
}

// Uma implementação mais real de liveness percorreria de TRÁS PARA FRENTE
// CORREÇÃO CRÍTICA: Adiciona flag para detectar uso em PRINT (não marca como último uso)
static void mark_last_use_backward(ASTNode* node, const char* name, int version, int* found, int in_loop, int in_print) {
    if (node == NULL || *found) return;

    // --- REGRA DE OURO DOS LOOPS ---
    // Se a variável é usada na condição de um loop que estamos analisando,
    // ela não pode morrer dentro do loop.
    if (node->type == AST_WHILE_STATEMENT) {
        if (is_var_used_in(node->as.while_stmt.condition, name)) {
            // A variável é necessária para o loop. 
            // Se estamos vindo de "fora" do loop (backward), o último uso dela
            // é tecnicamente o próprio nó do loop após o corpo ser processado.
            in_loop = 1; 
        }
    }

    // Se for um bloco, percorre statements de trás para frente
    if (node->type == AST_BLOCK) {
        for (int i = (int)node->as.block.count - 1; i >= 0; i--) {
            mark_last_use_backward(node->as.block.statements[i], name, version, found, in_loop, 0);
            if (*found) return;
        }
    }

    // Se for o identificador correto
    if (node->type == AST_IDENTIFIER && 
        strcmp(node->as.identifier.name, name) == 0 && 
        node->as.identifier.ssa_version == version) {
        
        // CORREÇÃO CRÍTICA: Não marca como último uso se:
        // 1. Estivermos dentro de um loop que depende desta variável
        // 2. O uso está em um PRINT (pode haver uso futuro em função como tcp_close)
        // 
        // REGRA CONSERVADORA: Se o uso está em PRINT, não marca como último uso
        // porque PRINT geralmente é apenas para debug e pode haver uso real depois
        
        if (!in_loop && !in_print) {
            node->as.identifier.is_last_use = 1;
            *found = 1;
        }
        return;
    }

    // Recursão para outros tipos de nós (em ordem inversa de execução)
    switch (node->type) {
        case AST_VARIABLE_DECLARATION:
            mark_last_use_backward(node->as.variable_decl.value, name, version, found, in_loop, 0);
            break;
        case AST_ASSIGNMENT:
            mark_last_use_backward(node->as.assignment.value, name, version, found, in_loop, 0);
            break;
        case AST_BINARY_EXPRESSION:
            mark_last_use_backward(node->as.binary_expr.right, name, version, found, in_loop, 0);
            mark_last_use_backward(node->as.binary_expr.left, name, version, found, in_loop, 0);
            break;
        case AST_PRINT:
            // CORREÇÃO: Marca que estamos dentro de PRINT para não liberar variáveis usadas aqui
            mark_last_use_backward(node->as.print_stmt.expression, name, version, found, in_loop, 1);
            break;
        case AST_IF_STATEMENT:
            mark_last_use_backward(node->as.if_stmt.else_branch, name, version, found, in_loop, 0);
            mark_last_use_backward(node->as.if_stmt.then_branch, name, version, found, in_loop, 0);
            mark_last_use_backward(node->as.if_stmt.condition, name, version, found, in_loop, 0);
            break;
        case AST_WHILE_STATEMENT:
            mark_last_use_backward(node->as.while_stmt.body, name, version, found, in_loop, 0);
            mark_last_use_backward(node->as.while_stmt.condition, name, version, found, in_loop, 0);
            break;
        /* CORREÇÃO: Verificar argumentos de chamadas de função! */
        case AST_FUNCTION_CALL:
            /* Percorre argumentos de trás para frente */
            for (int i = (int)node->as.function_call.argument_count - 1; i >= 0; i--) {
                mark_last_use_backward(node->as.function_call.arguments[i], name, version, found, in_loop, 0);
                if (*found) return;
            }
            break;
        /* Também verificar expressões de retorno */
        case AST_RETURN:
            mark_last_use_backward(node->as.return_stmt.value, name, version, found, in_loop, 0);
            break;
        /* Expressões unárias */
        case AST_UNARY_EXPRESSION:
            mark_last_use_backward(node->as.unary_expr.operand, name, version, found, in_loop, 0);
            break;
        default: break;
    }
}

void ssa_analyze_liveness(ASTNode* node) {
    printf("\n=== Fase: Análise de Liveness (Ownership 2.0) ===\n");
    
    // Para cada variável na tabela SSA
    for (int i = 0; i < ssa_var_count; i++) {
        // Para cada versão dessa variável
        for (int v = 0; v < ssa_table[i].total_versions; v++) {
            int found = 0;
            mark_last_use_backward(node, ssa_table[i].name, v, &found, 0, 0);
        }
    }
    printf("✓ Pontos de liberação automática identificados\n");
}

void ssa_cleanup(void) {
    for (int i = 0; i < ssa_var_count; i++) {
        if (ssa_table[i].name != NULL) {
            string_free(ssa_table[i].name);
            ssa_table[i].name = NULL;
        }
    }
    ssa_var_count = 0;
}
