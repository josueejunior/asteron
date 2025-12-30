#include "vm.h"
#include "../utils/utils.h"
#include "../lexer/lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

// ==================== Compilador (AST → Bytecode) ====================

typedef struct {
    char* name;
    int is_local;
} CompilerSymbol;

#define MAX_COMPILER_SYMBOLS 100
static CompilerSymbol g_symbols[MAX_COMPILER_SYMBOLS];
static int g_symbol_count = 0;

static void compiler_add_symbol(const char* name, int is_local) {
    for (int i = 0; i < g_symbol_count; i++) {
        if (strcmp(g_symbols[i].name, name) == 0) {
            g_symbols[i].is_local = is_local;
            return;
        }
    }
    if (g_symbol_count < MAX_COMPILER_SYMBOLS) {
        g_symbols[g_symbol_count].name = string_copy(name, strlen(name));
        g_symbols[g_symbol_count].is_local = is_local;
        g_symbol_count++;
    }
}

static int compiler_is_local(const char* name) {
    for (int i = 0; i < g_symbol_count; i++) {
        if (strcmp(g_symbols[i].name, name) == 0) {
            return g_symbols[i].is_local;
        }
    }
    return 0;
}

static size_t add_constant(BytecodeProgram* program, Constant constant) {
    // Verifica se constante já existe
    for (size_t i = 0; i < program->constant_count; i++) {
        Constant* c = &program->constants[i];
        if (c->type == constant.type) {
            if (constant.type == CONST_NUMBER && c->value.number == constant.value.number) {
                return i;
            }
            if (constant.type == CONST_STRING && 
                strcmp(c->value.string, constant.value.string) == 0) {
                return i;
            }
            if (constant.type == CONST_BOOL && c->value.boolean == constant.value.boolean) {
                return i;
            }
            if (constant.type == CONST_NIL) {
                return i;
            }
        }
    }
    
    // Adiciona nova constante
    if (program->constant_count >= program->constant_capacity) {
        program->constant_capacity *= 2;
        Constant* new_constants = (Constant*)realloc(
            program->constants, sizeof(Constant) * program->constant_capacity);
        if (new_constants == NULL) return SIZE_MAX;
        program->constants = new_constants;
    }
    
    program->constants[program->constant_count] = constant;
    return program->constant_count++;
}

static size_t add_variable(BytecodeProgram* program, const char* name) {
    // Verifica se variável já existe
    for (size_t i = 0; i < program->variable_count; i++) {
        if (strcmp(program->variable_names[i], name) == 0) {
            return i;
        }
    }
    
    // Adiciona nova variável
    if (program->variable_count >= program->variable_capacity) {
        program->variable_capacity *= 2;
        char** new_names = (char**)realloc(
            program->variable_names, sizeof(char*) * program->variable_capacity);
        if (new_names == NULL) return SIZE_MAX;
        program->variable_names = new_names;
    }
    
    program->variable_names[program->variable_count] = string_copy(name, strlen(name));
    return program->variable_count++;
}

static void emit_instruction(BytecodeProgram* program, OpCode op, size_t operand) {
    if (program->instruction_count >= program->instruction_capacity) {
        program->instruction_capacity *= 2;
        Instruction* new_instructions = (Instruction*)realloc(
            program->instructions, sizeof(Instruction) * program->instruction_capacity);
        if (new_instructions == NULL) return;
        program->instructions = new_instructions;
    }
    
    Instruction* instr = &program->instructions[program->instruction_count++];
    instr->op = op;
    instr->operand.constant_index = operand;  // Usa union, qualquer campo serve
}

// Função recursiva para compilar AST
static void compile_node(BytecodeProgram* program, ASTNode* node) {
    if (node == NULL) return;
    
    // Marca o início das instruções deste nó
    node->start_pc = program->instruction_count;
    
    switch (node->type) {
        case AST_LITERAL: {
            Constant constant;
            if (node->as.literal.type == LIT_NUMBER) {
                constant.type = CONST_NUMBER;
                constant.value.number = node->as.literal.value.number;
            } else if (node->as.literal.type == LIT_STRING) {
                constant.type = CONST_STRING;
                constant.value.string = node->as.literal.value.string;
            } else if (node->as.literal.type == LIT_BOOL) {
                constant.type = CONST_BOOL;
                constant.value.boolean = node->as.literal.value.boolean;
            } else {
                constant.type = CONST_NIL;
            }
            size_t const_idx = add_constant(program, constant);
            emit_instruction(program, OP_LOAD_CONST, const_idx);
            break;
        }
        
        case AST_IDENTIFIER: {
            size_t var_idx = add_variable(program, node->as.identifier.name);
            
            // Se a variável foi marcada como não-escape na análise, usa carregamento rápido
            if (compiler_is_local(node->as.identifier.name)) {
                emit_instruction(program, OP_LOAD_LOCAL, var_idx);
            } else {
                emit_instruction(program, OP_LOAD_VAR, var_idx);
            }
            
            // Ownership 2.0: Se for o último uso, injeta comando de liberação
            if (node->as.identifier.is_last_use) {
                emit_instruction(program, OP_RELEASE, var_idx);
            }
            break;
        }
        
        case AST_BINARY_EXPRESSION: {
            // Compila lado esquerdo
            compile_node(program, node->as.binary_expr.left);
            // Compila lado direito
            compile_node(program, node->as.binary_expr.right);
            
            // Emite operação
            int op = node->as.binary_expr.operator;
            if (op == TOKEN_PLUS) {
                emit_instruction(program, OP_ADD, 0);
            } else if (op == TOKEN_MINUS) {
                emit_instruction(program, OP_SUB, 0);
            } else if (op == TOKEN_MULTIPLY) {
                emit_instruction(program, OP_MUL, 0);
            } else if (op == TOKEN_DIVIDE) {
                emit_instruction(program, OP_DIV, 0);
            } else if (op == TOKEN_EQ) {
                emit_instruction(program, OP_EQ, 0);
            } else if (op == TOKEN_NE) {
                emit_instruction(program, OP_NE, 0);
            } else if (op == TOKEN_GT) {
                emit_instruction(program, OP_GT, 0);
            } else if (op == TOKEN_LT) {
                emit_instruction(program, OP_LT, 0);
            } else if (op == TOKEN_GTE) {
                emit_instruction(program, OP_GTE, 0);
            } else if (op == TOKEN_LTE) {
                emit_instruction(program, OP_LTE, 0);
            } else if (op == TOKEN_AND) {
                emit_instruction(program, OP_AND, 0);
            } else if (op == TOKEN_OR) {
                emit_instruction(program, OP_OR, 0);
            }
            break;
        }
        
        case AST_UNARY_EXPRESSION: {
            compile_node(program, node->as.unary_expr.operand);
            int op = node->as.unary_expr.operator;
            if (op == TOKEN_NOT) {
                emit_instruction(program, OP_NOT, 0);
            } else if (op == TOKEN_MINUS) {
                emit_instruction(program, OP_NEG, 0);
            }
            break;
        }
        
        case AST_VARIABLE_DECLARATION: {
            // Compila valor
            compile_node(program, node->as.variable_decl.value);
            
            size_t var_idx = add_variable(program, node->as.variable_decl.name);
            
            // Registra se a variável é local ou global para usos futuros (LOAD)
            compiler_add_symbol(node->as.variable_decl.name, node->as.variable_decl.escapes == 0);
            
            if (node->as.variable_decl.escapes == 0) {
                printf("  [Optimize] Variável '%s' alocada em Registrador Local (Não Escapa)\n", node->as.variable_decl.name);
                emit_instruction(program, OP_STORE_LOCAL, var_idx);
            } else {
                emit_instruction(program, OP_STORE_VAR, var_idx);
            }
            break;
        }

        case AST_ASSIGNMENT: {
            compile_node(program, node->as.assignment.value);
            size_t var_idx = add_variable(program, node->as.assignment.name);
            
            if (compiler_is_local(node->as.assignment.name)) {
                emit_instruction(program, OP_STORE_LOCAL, var_idx);
            } else {
                emit_instruction(program, OP_STORE_VAR, var_idx);
            }
            break;
        }
        
        case AST_PRINT: {
            compile_node(program, node->as.print_stmt.expression);
            emit_instruction(program, OP_PRINT, 0);
            emit_instruction(program, OP_POP, 0);  // Remove valor do stack após print
            break;
        }
        
        case AST_FUNCTION_CALL: {
            // Compila argumentos (da esquerda para direita - ordem natural)
            for (size_t i = 0; i < node->as.function_call.argument_count; i++) {
                compile_node(program, node->as.function_call.arguments[i]);
            }
            // Emite chamada (nome da função será resolvido na VM)
            // Por enquanto, usa variável para armazenar nome da função
            size_t func_idx = add_variable(program, node->as.function_call.name);
            emit_instruction(program, OP_CALL, func_idx);
            // A VM remove os argumentos do stack após a chamada
            break;
        }
        
        case AST_RETURN: {
            if (node->as.return_stmt.value != NULL) {
                compile_node(program, node->as.return_stmt.value);
            } else {
                // Return sem valor - empilha nil
                Constant nil_const = {CONST_NIL, {0}};
                size_t nil_idx = add_constant(program, nil_const);
                emit_instruction(program, OP_LOAD_CONST, nil_idx);
            }
            emit_instruction(program, OP_RETURN, 0);
            break;
        }
        
        case AST_IF_STATEMENT: {
            // Compila condição
            compile_node(program, node->as.if_stmt.condition);
            
            // Salto se falso (pula then branch)
            size_t jump_if_false_pos = program->instruction_count;
            emit_instruction(program, OP_JUMP_IF_FALSE, 0);  // Placeholder
            
            // Compila then branch
            compile_node(program, node->as.if_stmt.then_branch);
            
            // Se há else, pula o else após then
            size_t jump_else_pos = SIZE_MAX;
            if (node->as.if_stmt.else_branch != NULL) {
                jump_else_pos = program->instruction_count;
                emit_instruction(program, OP_JUMP, 0);  // Placeholder
            }
            
            // Atualiza salto do if
            program->instructions[jump_if_false_pos].operand.jump_target = 
                program->instruction_count;
            
            // Compila else branch (se existe)
            if (node->as.if_stmt.else_branch != NULL) {
                compile_node(program, node->as.if_stmt.else_branch);
                // Atualiza salto após then
                program->instructions[jump_else_pos].operand.jump_target = 
                    program->instruction_count;
            }
            break;
        }
        
        case AST_WHILE_STATEMENT: {
            size_t loop_start = program->instruction_count;
            
            // Compila condição
            compile_node(program, node->as.while_stmt.condition);
            
            // Salto se falso (sai do loop)
            size_t jump_if_false_pos = program->instruction_count;
            emit_instruction(program, OP_JUMP_IF_FALSE, 0);  // Placeholder
            
            // Compila corpo
            compile_node(program, node->as.while_stmt.body);
            
            // Salto de volta para o início
            emit_instruction(program, OP_JUMP, loop_start);
            
            // Atualiza salto de saída
            program->instructions[jump_if_false_pos].operand.jump_target = 
                program->instruction_count;
            break;
        }
        
        case AST_FOR_STATEMENT: {
            // Compila inicialização
            if (node->as.for_stmt.init != NULL) {
                compile_node(program, node->as.for_stmt.init);
            }
            
            size_t loop_start = program->instruction_count;
            
            // Compila condição (se existe)
            if (node->as.for_stmt.condition != NULL) {
                compile_node(program, node->as.for_stmt.condition);
                
                // Salto se falso (sai do loop)
                size_t jump_if_false_pos = program->instruction_count;
                emit_instruction(program, OP_JUMP_IF_FALSE, 0);  // Placeholder
                
                // Compila corpo
                compile_node(program, node->as.for_stmt.body);
                
                // Compila incremento
                if (node->as.for_stmt.increment != NULL) {
                    compile_node(program, node->as.for_stmt.increment);
                    emit_instruction(program, OP_POP, 0);  // Remove resultado do incremento
                }
                
                // Salto de volta para condição
                emit_instruction(program, OP_JUMP, loop_start);
                
                // Atualiza salto de saída
                program->instructions[jump_if_false_pos].operand.jump_target = 
                    program->instruction_count;
            } else {
                // Loop infinito (sem condição)
                compile_node(program, node->as.for_stmt.body);
                
                // Compila incremento
                if (node->as.for_stmt.increment != NULL) {
                    compile_node(program, node->as.for_stmt.increment);
                    emit_instruction(program, OP_POP, 0);
                }
                
                // Salto de volta
                emit_instruction(program, OP_JUMP, loop_start);
            }
            break;
        }
        
        case AST_BLOCK: {
            for (size_t i = 0; i < node->as.block.count; i++) {
                compile_node(program, node->as.block.statements[i]);
            }
            break;
        }
        
        case AST_FUNCTION_DECLARATION: {
            // Funções são armazenadas na tabela de funções, não compiladas aqui
            // (serão compiladas quando chamadas ou podem ser pré-compiladas)
            break;
        }
        
        default:
            break;
    }

    // Marca o fim das instruções deste nó
    node->end_pc = program->instruction_count;
}

BytecodeProgram* compiler_compile(ASTNode* ast) {
    if (ast == NULL) return NULL;
    
    // Inicializa tabela de símbolos do compilador
    for (int i = 0; i < g_symbol_count; i++) {
        string_free(g_symbols[i].name);
    }
    g_symbol_count = 0;
    
    BytecodeProgram* program = (BytecodeProgram*)malloc(sizeof(BytecodeProgram));
    if (program == NULL) return NULL;
    
    program->instruction_count = 0;
    program->instruction_capacity = 64;
    program->instructions = (Instruction*)malloc(sizeof(Instruction) * program->instruction_capacity);
    if (program->instructions == NULL) {
        free(program);
        return NULL;
    }
    
    program->constant_count = 0;
    program->constant_capacity = 32;
    program->constants = (Constant*)malloc(sizeof(Constant) * program->constant_capacity);
    if (program->constants == NULL) {
        free(program->instructions);
        free(program);
        return NULL;
    }
    
    program->variable_count = 0;
    program->variable_capacity = 32;
    program->variable_names = (char**)malloc(sizeof(char*) * program->variable_capacity);
    if (program->variable_names == NULL) {
        free(program->constants);
        free(program->instructions);
        free(program);
        return NULL;
    }
    
    // Compila AST
    compile_node(program, ast);
    
    // Adiciona HALT no final
    emit_instruction(program, OP_HALT, 0);
    
    return program;
}

void bytecode_program_destroy(BytecodeProgram* program) {
    if (program == NULL) return;
    
    if (program->instructions != NULL) {
        free(program->instructions);
    }
    
    if (program->constants != NULL) {
        for (size_t i = 0; i < program->constant_count; i++) {
            if (program->constants[i].type == CONST_STRING && 
                program->constants[i].value.string != NULL) {
                // Não libera - strings vêm da AST
            }
        }
        free(program->constants);
    }
    
    if (program->variable_names != NULL) {
        for (size_t i = 0; i < program->variable_count; i++) {
            if (program->variable_names[i] != NULL) {
                free(program->variable_names[i]);
            }
        }
        free(program->variable_names);
    }
    
    free(program);
}

void bytecode_program_print(BytecodeProgram* program) {
    if (program == NULL) {
        printf("Bytecode: NULL\n");
        return;
    }
    
    printf("=== Bytecode Program ===\n");
    printf("Instruções: %zu\n", program->instruction_count);
    printf("Constantes: %zu\n", program->constant_count);
    printf("Variáveis: %zu\n\n", program->variable_count);
    
    const char* op_names[] = {
        "LOAD_CONST", "LOAD_VAR", "STORE_VAR",
        "ADD", "SUB", "MUL", "DIV",
        "EQ", "NE", "GT", "LT", "GTE", "LTE",
        "AND", "OR", "NOT", "NEG",
        "JUMP", "JUMP_IF_FALSE", "JUMP_IF_TRUE",
        "CALL", "RETURN",
        "PRINT", "POP", "RELEASE", 
        "STORE_LOCAL", "LOAD_LOCAL", "HALT"
    };
    
    for (size_t i = 0; i < program->instruction_count; i++) {
        Instruction* instr = &program->instructions[i];
        printf("%4zu: %s", i, op_names[instr->op]);
        
        switch (instr->op) {
            case OP_LOAD_CONST:
                printf(" [%zu]", instr->operand.constant_index);
                if (instr->operand.constant_index < program->constant_count) {
                    Constant* c = &program->constants[instr->operand.constant_index];
                    if (c->type == CONST_NUMBER) {
                        printf(" (%g)", c->value.number);
                    } else if (c->type == CONST_STRING) {
                        printf(" (\"%s\")", c->value.string);
                    } else if (c->type == CONST_BOOL) {
                        printf(" (%s)", c->value.boolean ? "true" : "false");
                    }
                }
                break;
            case OP_LOAD_VAR:
            case OP_STORE_VAR:
                printf(" [%zu]", instr->operand.var_index);
                if (instr->operand.var_index < program->variable_count) {
                    printf(" (%s)", program->variable_names[instr->operand.var_index]);
                }
                break;
            case OP_JUMP:
            case OP_JUMP_IF_FALSE:
            case OP_JUMP_IF_TRUE:
                printf(" -> %zu", instr->operand.jump_target);
                break;
            case OP_CALL:
                printf(" [%zu]", instr->operand.function_index);
                if (instr->operand.function_index < program->variable_count) {
                    printf(" (%s)", program->variable_names[instr->operand.function_index]);
                }
                break;
            case OP_RELEASE:
            case OP_STORE_LOCAL:
            case OP_LOAD_LOCAL:
                printf(" [%zu]", instr->operand.var_index);
                if (instr->operand.var_index < program->variable_count) {
                    printf(" (%s)", program->variable_names[instr->operand.var_index]);
                }
                break;
            default:
                break;
        }
        printf("\n");
    }
}
