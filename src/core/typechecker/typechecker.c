#include "typechecker.h"
#include "../lexer/lexer.h"
#include "../../utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void type_error(TypeChecker* checker, int line, const char* message) {
    fprintf(stderr, "[Erro de Tipo - Linha %d] %s\n", line, message);
    checker->had_error = 1;
    checker->error_count++;
}

TypeChecker* typechecker_create(void) {
    TypeChecker* checker = (TypeChecker*)malloc(sizeof(TypeChecker));
    if (checker == NULL) return NULL;
    checker->had_error = 0;
    checker->error_count = 0;
    return checker;
}

void typechecker_destroy(TypeChecker* checker) {
    if (checker != NULL) {
        free(checker);
    }
}

int typechecker_had_error(TypeChecker* checker) {
    return checker != NULL && checker->had_error;
}

// Infere tipo de um literal
Type infer_type_from_literal(ASTNode* node) {
    if (node == NULL || node->type != AST_LITERAL) {
        return TYPE_UNKNOWN;
    }
    
    switch (node->as.literal.type) {
        case LIT_NUMBER: {
            // Verifica se é int ou float
            double num = node->as.literal.value.number;
            if (num == (int)num) {
                return TYPE_INT;
            } else {
                return TYPE_FLOAT;
            }
        }
        case LIT_STRING:
            return TYPE_STRING;
        case LIT_BOOL:
            return TYPE_BOOL;
        default:
            return TYPE_UNKNOWN;
    }
}

// Infere tipo de uma expressão
Type infer_type_from_expression(ASTNode* node) {
    if (node == NULL) return TYPE_UNKNOWN;
    
    // Se já tem tipo inferido, retorna
    if (node->inferred_type != NULL) {
        return node->inferred_type->type;
    }
    
    switch (node->type) {
        case AST_LITERAL:
            return infer_type_from_literal(node);
        
        case AST_IDENTIFIER:
            // Tipo será verificado durante análise de variáveis
            return TYPE_UNKNOWN;
        
        case AST_BINARY_EXPRESSION: {
            Type left_type = infer_type_from_expression(node->as.binary_expr.left);
            Type right_type = infer_type_from_expression(node->as.binary_expr.right);
            
            int op = node->as.binary_expr.operator;
            
            // Operadores aritméticos
            if (op == TOKEN_PLUS || op == TOKEN_MINUS || 
                op == TOKEN_MULTIPLY || op == TOKEN_DIVIDE) {
                // Concatenação de strings (permite string + string, string + number, number + string)
                if (op == TOKEN_PLUS) {
                    if (left_type == TYPE_STRING || right_type == TYPE_STRING) {
                        return TYPE_STRING;
                    }
                }
                // Se ambos são int, resultado é int
                if (left_type == TYPE_INT && right_type == TYPE_INT) {
                    return TYPE_INT;
                }
                // Se pelo menos um é float, resultado é float
                if (left_type == TYPE_FLOAT || right_type == TYPE_FLOAT) {
                    return TYPE_FLOAT;
                }
            }
            
            // Operadores de comparação retornam bool
            if (op == TOKEN_EQ || op == TOKEN_NE || op == TOKEN_GT || 
                op == TOKEN_LT || op == TOKEN_GTE || op == TOKEN_LTE) {
                return TYPE_BOOL;
            }
            
            // Operadores lógicos retornam bool
            if (op == TOKEN_AND || op == TOKEN_OR) {
                return TYPE_BOOL;
            }
            
            return TYPE_UNKNOWN;
        }
        
        case AST_UNARY_EXPRESSION: {
            Type operand_type = infer_type_from_expression(node->as.unary_expr.operand);
            int op = node->as.unary_expr.operator;
            
            if (op == TOKEN_NOT) {
                return TYPE_BOOL;
            }
            
            if (op == TOKEN_MINUS) {
                if (operand_type == TYPE_INT) return TYPE_INT;
                if (operand_type == TYPE_FLOAT) return TYPE_FLOAT;
            }
            
            return operand_type;
        }
        
        case AST_FUNCTION_CALL:
            // Tipo será verificado durante análise de funções
            return TYPE_UNKNOWN;
        
        default:
            return TYPE_UNKNOWN;
    }
}

// Verifica compatibilidade de tipos
int types_compatible(Type expected, Type actual) {
    if (expected == TYPE_UNKNOWN || actual == TYPE_UNKNOWN) {
        return 1;  // Aceita unknown (será inferido)
    }
    
    if (expected == actual) {
        return 1;
    }
    
    // Conversões implícitas permitidas
    if (expected == TYPE_FLOAT && actual == TYPE_INT) {
        return 1;  // int pode ser convertido para float
    }
    
    return 0;
}

const char* type_error_message(Type expected, Type actual) {
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), 
             "Tipo incompatível: esperado %s, encontrado %s",
             type_to_string(expected), type_to_string(actual));
    return buffer;
}

// Função recursiva para verificar tipos
static Type check_node(TypeChecker* checker, ASTNode* node, int line) {
    if (node == NULL) return TYPE_UNKNOWN;
    
    switch (node->type) {
        case AST_VARIABLE_DECLARATION: {
            TypeInfo* declared_type = node->as.variable_decl.type_info;
            Type value_type = check_node(checker, node->as.variable_decl.value, line);
            
            if (declared_type != NULL) {
                // Tipo foi anotado - verifica compatibilidade
                if (!types_compatible(declared_type->type, value_type)) {
                    type_error(checker, line, type_error_message(declared_type->type, value_type));
                    node->inferred_type = type_info_create(TYPE_ERROR, 0);
                    return TYPE_ERROR;
                }
                node->inferred_type = type_info_create(declared_type->type, 0);
                return declared_type->type;
            } else {
                // Inferência de tipo
                if (value_type != TYPE_UNKNOWN && value_type != TYPE_ERROR) {
                    node->inferred_type = type_info_create(value_type, 1);
                    return value_type;
                }
                node->inferred_type = type_info_create(TYPE_UNKNOWN, 1);
                return TYPE_UNKNOWN;
            }
        }
        
        case AST_LITERAL: {
            Type literal_type = infer_type_from_literal(node);
            node->inferred_type = type_info_create(literal_type, 1);
            return literal_type;
        }
        
        case AST_BINARY_EXPRESSION: {
            Type left_type = check_node(checker, node->as.binary_expr.left, line);
            Type right_type = check_node(checker, node->as.binary_expr.right, line);
            
            int op = node->as.binary_expr.operator;
            
            // Verifica tipos compatíveis para operadores
            if (op == TOKEN_PLUS || op == TOKEN_MINUS || 
                op == TOKEN_MULTIPLY || op == TOKEN_DIVIDE) {
                // Concatenação de strings (permite string + string, string + number, number + string)
                if (op == TOKEN_PLUS) {
                    if ((left_type == TYPE_STRING || left_type == TYPE_UNKNOWN) && 
                        (right_type == TYPE_STRING || right_type == TYPE_INT || right_type == TYPE_FLOAT || right_type == TYPE_UNKNOWN)) {
                        Type result = TYPE_STRING;
                        node->inferred_type = type_info_create(result, 1);
                        return result;
                    }
                    if ((left_type == TYPE_INT || left_type == TYPE_FLOAT || left_type == TYPE_UNKNOWN) && 
                        (right_type == TYPE_STRING || right_type == TYPE_UNKNOWN)) {
                        Type result = TYPE_STRING;
                        node->inferred_type = type_info_create(result, 1);
                        return result;
                    }
                }
                // Permite TYPE_UNKNOWN durante inferência
                if (left_type != TYPE_INT && left_type != TYPE_FLOAT && left_type != TYPE_UNKNOWN) {
                    type_error(checker, line, "Operador aritmético requer números");
                    node->inferred_type = type_info_create(TYPE_ERROR, 0);
                    return TYPE_ERROR;
                }
                if (right_type != TYPE_INT && right_type != TYPE_FLOAT && right_type != TYPE_UNKNOWN) {
                    type_error(checker, line, "Operador aritmético requer números");
                    node->inferred_type = type_info_create(TYPE_ERROR, 0);
                    return TYPE_ERROR;
                }
                // Se algum tipo é UNKNOWN, resultado também é UNKNOWN (será inferido depois)
                if (left_type == TYPE_UNKNOWN || right_type == TYPE_UNKNOWN) {
                    node->inferred_type = type_info_create(TYPE_UNKNOWN, 1);
                    return TYPE_UNKNOWN;
                }
                Type result = (left_type == TYPE_FLOAT || right_type == TYPE_FLOAT) ? TYPE_FLOAT : TYPE_INT;
                node->inferred_type = type_info_create(result, 1);
                return result;
            }
            
            // Operadores de comparação
            if (op == TOKEN_EQ || op == TOKEN_NE || op == TOKEN_GT || 
                op == TOKEN_LT || op == TOKEN_GTE || op == TOKEN_LTE) {
                // Permite TYPE_UNKNOWN durante inferência
                if (left_type == TYPE_UNKNOWN || right_type == TYPE_UNKNOWN) {
                    node->inferred_type = type_info_create(TYPE_UNKNOWN, 1);
                    return TYPE_UNKNOWN;
                }
                if (!types_compatible(left_type, right_type)) {
                    type_error(checker, line, "Tipos incompatíveis em comparação");
                    node->inferred_type = type_info_create(TYPE_ERROR, 0);
                    return TYPE_ERROR;
                }
                Type result = TYPE_BOOL;
                node->inferred_type = type_info_create(result, 1);
                return result;
            }
            
            // Operadores lógicos
            if (op == TOKEN_AND || op == TOKEN_OR) {
                // Permite TYPE_UNKNOWN durante inferência
                if (left_type == TYPE_UNKNOWN || right_type == TYPE_UNKNOWN) {
                    node->inferred_type = type_info_create(TYPE_UNKNOWN, 1);
                    return TYPE_UNKNOWN;
                }
                if (left_type != TYPE_BOOL || right_type != TYPE_BOOL) {
                    type_error(checker, line, "Operadores lógicos requerem booleanos");
                    node->inferred_type = type_info_create(TYPE_ERROR, 0);
                    return TYPE_ERROR;
                }
                Type result = TYPE_BOOL;
                node->inferred_type = type_info_create(result, 1);
                return result;
            }
            
            node->inferred_type = type_info_create(TYPE_UNKNOWN, 1);
            return TYPE_UNKNOWN;
        }
        
        case AST_UNARY_EXPRESSION: {
            Type operand_type = check_node(checker, node->as.unary_expr.operand, line);
            int op = node->as.unary_expr.operator;
            
            if (op == TOKEN_NOT) {
                if (operand_type != TYPE_BOOL && operand_type != TYPE_UNKNOWN) {
                    type_error(checker, line, "Operador '!' requer booleano");
                    node->inferred_type = type_info_create(TYPE_ERROR, 0);
                    return TYPE_ERROR;
                }
                if (operand_type == TYPE_UNKNOWN) {
                    node->inferred_type = type_info_create(TYPE_UNKNOWN, 1);
                    return TYPE_UNKNOWN;
                }
                Type result = TYPE_BOOL;
                node->inferred_type = type_info_create(result, 1);
                return result;
            }
            
            if (op == TOKEN_MINUS) {
                if (operand_type != TYPE_INT && operand_type != TYPE_FLOAT && operand_type != TYPE_UNKNOWN) {
                    type_error(checker, line, "Operador '-' requer número");
                    node->inferred_type = type_info_create(TYPE_ERROR, 0);
                    return TYPE_ERROR;
                }
                if (operand_type == TYPE_UNKNOWN) {
                    node->inferred_type = type_info_create(TYPE_UNKNOWN, 1);
                    return TYPE_UNKNOWN;
                }
                node->inferred_type = type_info_create(operand_type, 1);
                return operand_type;
            }
            
            node->inferred_type = type_info_create(operand_type, 1);
            return operand_type;
        }
        
        case AST_IDENTIFIER:
            // Tipo será verificado no contexto de uso durante execução
            // Por enquanto, permite TYPE_UNKNOWN para não bloquear a verificação
            node->inferred_type = type_info_create(TYPE_UNKNOWN, 1);
            return TYPE_UNKNOWN;
        
        case AST_FUNCTION_CALL:
            // Verificação de chamadas de função será implementada depois
            node->inferred_type = type_info_create(TYPE_UNKNOWN, 1);
            return TYPE_UNKNOWN;
        
        case AST_IF_STATEMENT: {
            Type cond_type = check_node(checker, node->as.if_stmt.condition, line);
            // Permite números em condições (truthy/falsy) além de booleanos
            if (cond_type != TYPE_BOOL && cond_type != TYPE_INT && cond_type != TYPE_FLOAT && cond_type != TYPE_UNKNOWN) {
                type_error(checker, line, "Condição do 'if' deve ser booleana ou numérica");
            }
            check_node(checker, node->as.if_stmt.then_branch, line);
            if (node->as.if_stmt.else_branch != NULL) {
                check_node(checker, node->as.if_stmt.else_branch, line);
            }
            return TYPE_VOID;
        }
        
        case AST_WHILE_STATEMENT: {
            Type cond_type = check_node(checker, node->as.while_stmt.condition, line);
            // Permite números em condições (truthy/falsy) além de booleanos
            if (cond_type != TYPE_BOOL && cond_type != TYPE_INT && cond_type != TYPE_FLOAT && cond_type != TYPE_UNKNOWN) {
                type_error(checker, line, "Condição do 'while' deve ser booleana ou numérica");
            }
            check_node(checker, node->as.while_stmt.body, line);
            return TYPE_VOID;
        }
        
        case AST_FOR_STATEMENT: {
            if (node->as.for_stmt.init != NULL) {
                check_node(checker, node->as.for_stmt.init, line);
            }
            if (node->as.for_stmt.condition != NULL) {
                Type cond_type = check_node(checker, node->as.for_stmt.condition, line);
                // Permite números em condições (truthy/falsy) além de booleanos
                if (cond_type != TYPE_BOOL && cond_type != TYPE_INT && cond_type != TYPE_FLOAT && cond_type != TYPE_UNKNOWN) {
                    type_error(checker, line, "Condição do 'for' deve ser booleana ou numérica");
                }
            }
            if (node->as.for_stmt.increment != NULL) {
                check_node(checker, node->as.for_stmt.increment, line);
            }
            check_node(checker, node->as.for_stmt.body, line);
            return TYPE_VOID;
        }
        
        case AST_PRINT: {
            check_node(checker, node->as.print_stmt.expression, line);
            return TYPE_VOID;
        }
        
        case AST_RETURN: {
            if (node->as.return_stmt.value != NULL) {
                check_node(checker, node->as.return_stmt.value, line);
            }
            return TYPE_VOID;
        }
        
        case AST_FUNCTION_DECLARATION: {
            check_node(checker, node->as.function_decl.body, line);
            if (node->as.function_decl.return_type != NULL) {
                return node->as.function_decl.return_type->type;
            }
            return TYPE_VOID;
        }
        
        case AST_BLOCK: {
            for (size_t i = 0; i < node->as.block.count; i++) {
                check_node(checker, node->as.block.statements[i], line);
            }
            return TYPE_VOID;
        }
        
        default:
            return TYPE_UNKNOWN;
    }
}

int typechecker_check(ASTNode* ast) {
    if (ast == NULL) return 0;
    
    TypeChecker* checker = typechecker_create();
    if (checker == NULL) return 1;
    
    check_node(checker, ast, 1);
    
    int had_error = checker->had_error;
    typechecker_destroy(checker);
    
    return had_error ? 1 : 0;
}
