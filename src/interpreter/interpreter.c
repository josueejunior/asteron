#include "interpreter.h"
#include "../lexer/lexer.h"
#include "../utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ==================== Environment (Escopos Encadeados) ====================

#define ENV_TABLE_MAX 256

typedef struct {
    char* key;
    Value value;
} Entry;

struct Environment {
    Entry entries[ENV_TABLE_MAX];
    int count;
    Environment* parent;  // Escopo pai (para cadeia de escopos)
};

// Cria novo environment (frame)
Environment* environment_create(Environment* parent) {
    Environment* env = (Environment*)malloc(sizeof(Environment));
    if (env == NULL) {
        return NULL;
    }
    env->count = 0;
    env->parent = parent;
    for (int i = 0; i < ENV_TABLE_MAX; i++) {
        env->entries[i].key = NULL;
        env->entries[i].value = value_nil();
    }
    return env;
}

void environment_destroy(Environment* env) {
    if (env == NULL) return;
    
    // Destrói apenas este frame (não destrói o parent)
    for (int i = 0; i < env->count; i++) {
        if (env->entries[i].key != NULL) {
            free(env->entries[i].key);
            value_destroy(env->entries[i].value);
        }
    }
    free(env);
}

int environment_has(Environment* env, const char* name) {
    if (env == NULL) return 0;
    
    // Busca no escopo atual
    for (int i = 0; i < env->count; i++) {
        if (env->entries[i].key != NULL &&
            strcmp(env->entries[i].key, name) == 0) {
            return 1;
        }
    }
    
    // Se não encontrou, busca no escopo pai (recursivo)
    if (env->parent != NULL) {
        return environment_has(env->parent, name);
    }
    
    return 0;
}

void environment_set(Environment* env, const char* name, Value value) {
    if (env == NULL) return;
    
    value_retain(value);
    
    for (int i = 0; i < env->count; i++) {
        if (env->entries[i].key != NULL &&
            strcmp(env->entries[i].key, name) == 0) {
            value_release(env->entries[i].value);
            env->entries[i].value = value;
            return;
        }
    }
    
    if (env->parent != NULL && environment_has(env->parent, name)) {
        environment_set(env->parent, name, value);
        value_release(value);
        return;
    }
    
    if (env->count < ENV_TABLE_MAX) {
        env->entries[env->count].key = string_copy(name, strlen(name));
        env->entries[env->count].value = value;
        env->count++;
    } else {
        value_release(value);
    }
}

Value environment_get(Environment* env, const char* name) {
    if (env == NULL) return value_nil();
    
    // Busca no escopo atual
    for (int i = 0; i < env->count; i++) {
        if (env->entries[i].key != NULL &&
            strcmp(env->entries[i].key, name) == 0) {
            Value val = env->entries[i].value;
            value_retain(val);
            return val;
        }
    }
    
    // Se não encontrou, busca no escopo pai (recursivo)
    if (env->parent != NULL) {
        return environment_get(env->parent, name);
    }
    
    // Variável não encontrada em nenhum escopo
    fprintf(stderr, "Erro: Variável '%s' não definida\n", name);
    return value_nil();
}

void environment_remove(Environment* env, const char* name) {
    if (env == NULL) return;
    
    for (int i = 0; i < env->count; i++) {
        if (env->entries[i].key != NULL && strcmp(env->entries[i].key, name) == 0) {
            free(env->entries[i].key);
            value_release(env->entries[i].value);
            
            // Move o último elemento para esta posição para manter o array denso
            if (i < env->count - 1) {
                env->entries[i] = env->entries[env->count - 1];
            }
            env->count--;
            return;
        }
    }
    
    if (env->parent != NULL) {
        environment_remove(env->parent, name);
    }
}

// Push frame: cria novo frame filho do atual
Environment* frame_push(Environment* current) {
    return environment_create(current);
}

// Pop frame: retorna o parent (destrói o frame atual)
Environment* frame_pop(Environment* current) {
    if (current == NULL) return NULL;
    Environment* parent = current->parent;
    environment_destroy(current);
    return parent;
}

// ==================== Stack de Valores ====================

Stack* stack_create(size_t capacity) {
    Stack* stack = (Stack*)malloc(sizeof(Stack));
    if (stack == NULL) return NULL;
    
    stack->items = (Value*)malloc(sizeof(Value) * capacity);
    if (stack->items == NULL) {
        free(stack);
        return NULL;
    }
    
    stack->size = 0;
    stack->capacity = capacity;
    return stack;
}

void stack_destroy(Stack* stack) {
    if (stack == NULL) return;
    
    for (size_t i = 0; i < stack->size; i++) {
        value_destroy(stack->items[i]);
    }
    
    free(stack->items);
    free(stack);
}

void stack_push(Stack* stack, Value value) {
    if (stack == NULL) return;
    
    // Esta função assume a posse da referência (não faz retain extra)
    
    if (stack->size >= stack->capacity) {
        stack->capacity *= 2;
        Value* new_items = (Value*)realloc(stack->items, sizeof(Value) * stack->capacity);
        if (new_items == NULL) return;
        stack->items = new_items;
    }
    
    stack->items[stack->size++] = value;
}

Value stack_pop(Stack* stack) {
    if (stack == NULL || stack->size == 0) return value_nil();
    // O caller assume a posse da referência retornada (+1)
    return stack->items[--stack->size];
}

Value stack_peek(Stack* stack, size_t offset) {
    if (stack == NULL || offset >= stack->size) return value_nil();
    Value val = stack->items[stack->size - 1 - offset];
    value_retain(val); // Retorna uma nova posse (+1)
    return val;
}

// ==================== Sistema de Objetos (RefCounting) ====================

ObjString* obj_string_create(const char* chars, size_t length) {
    ObjString* string = (ObjString*)malloc(sizeof(ObjString));
    if (string == NULL) return NULL;
    
    string->obj.type = OBJ_STRING;
    string->obj.ref_count = 1;
    string->chars = string_copy(chars, length);
    string->length = length;
    
    return string;
}

ObjFunction* obj_function_create(Function* func) {
    ObjFunction* obj_func = (ObjFunction*)malloc(sizeof(ObjFunction));
    if (obj_func == NULL) return NULL;
    
    obj_func->obj.type = OBJ_FUNCTION;
    obj_func->obj.ref_count = 1;
    obj_func->func = func;
    
    return obj_func;
}

void obj_destroy(Obj* obj) {
    if (obj == NULL) return;
    
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)obj;
            if (string->chars != NULL) {
                free(string->chars);
            }
            free(string);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* func = (ObjFunction*)obj;
            // No momento, funções pertencem à FunctionTable global
            // Não destruímos o Function* aqui pois ele não é "owned" por esta referência
            free(func);
            break;
        }
    }
}

void value_retain(Value value) {
    if (value.type == VAL_OBJ && value.as.obj != NULL) {
        // Operação atômica para thread-safety
        __sync_add_and_fetch(&value.as.obj->ref_count, 1);
    }
}

void value_release(Value value) {
    if (value.type == VAL_OBJ && value.as.obj != NULL) {
        // Operação atômica para thread-safety
        if (__sync_sub_and_fetch(&value.as.obj->ref_count, 1) == 0) {
            obj_destroy(value.as.obj);
        }
    }
}

// ==================== Construtores de Valores ====================

Value value_number(double number) {
    Value value;
    value.type = VAL_NUMBER;
    value.as.number = number;
    return value;
}

Value value_string(char* string) {
    Value value;
    value.type = VAL_OBJ;
    ObjString* obj = obj_string_create(string, strlen(string));
    value.as.obj = (Obj*)obj;
    // Note: assumimos que 'string' foi passada para ser copiada
    return value;
}

Value value_bool(int boolean) {
    Value value;
    value.type = VAL_BOOL;
    value.as.boolean = boolean;
    return value;
}

Value value_nil(void) {
    Value value;
    value.type = VAL_NIL;
    value.as.number = 0;
    return value;
}

Value value_function(Function* function) {
    Value value;
    value.type = VAL_OBJ;
    ObjFunction* obj = obj_function_create(function);
    value.as.obj = (Obj*)obj;
    return value;
}

int value_is_string(Value value) {
    return value.type == VAL_OBJ && value.as.obj != NULL && value.as.obj->type == OBJ_STRING;
}

const char* value_to_chars(Value value) {
    if (value_is_string(value)) {
        return ((ObjString*)value.as.obj)->chars;
    }
    return "";
}

void value_destroy(Value value) {
    value_release(value);
}

void value_print(Value value) {
    switch (value.type) {
        case VAL_NUMBER:
            printf("%g", value.as.number);
            break;
        case VAL_OBJ:
            if (value.as.obj != NULL) {
                if (value.as.obj->type == OBJ_STRING) {
                    printf("%s", ((ObjString*)value.as.obj)->chars);
                } else if (value.as.obj->type == OBJ_FUNCTION) {
                    printf("<function>");
                }
            } else {
                printf("null obj");
            }
            break;
        case VAL_BOOL:
            printf("%s", value.as.boolean ? "true" : "false");
            break;
        case VAL_NIL:
            printf("nil");
            break;
    }
}

// ==================== Interpretador ====================

// Variável global para o environment atual (topo da stack)
static Environment* g_env = NULL;

// Tabela de funções
#define FUNCTION_TABLE_MAX 64

typedef struct {
    char* key;
    Function value;
} FunctionEntry;

struct FunctionTable {
    FunctionEntry entries[FUNCTION_TABLE_MAX];
    int count;
};

static FunctionTable* g_function_table = NULL;

// Funções da tabela de funções
FunctionTable* function_table_create(void) {
    FunctionTable* table = (FunctionTable*)malloc(sizeof(FunctionTable));
    if (table == NULL) return NULL;
    table->count = 0;
    for (int i = 0; i < FUNCTION_TABLE_MAX; i++) {
        table->entries[i].key = NULL;
    }
    return table;
}

void function_table_destroy(FunctionTable* table) {
    if (table == NULL) return;
    for (int i = 0; i < table->count; i++) {
        if (table->entries[i].key != NULL) {
            free(table->entries[i].key);
            // Não libera o body da função aqui, é gerenciado pela AST
        }
    }
    free(table);
}

void function_table_set(FunctionTable* table, const char* name, Function func) {
    if (table == NULL) return;
    
    // Verifica se já existe
    for (int i = 0; i < table->count; i++) {
        if (table->entries[i].key != NULL && strcmp(table->entries[i].key, name) == 0) {
            // Atualiza função existente
            table->entries[i].value = func;
            return;
        }
    }
    
    // Adiciona nova
    if (table->count < FUNCTION_TABLE_MAX) {
        table->entries[table->count].key = string_copy(name, strlen(name));
        table->entries[table->count].value = func;
        table->count++;
    }
}

Function* function_table_get(FunctionTable* table, const char* name) {
    if (table == NULL) return NULL;
    
    for (int i = 0; i < table->count; i++) {
        if (table->entries[i].key != NULL && strcmp(table->entries[i].key, name) == 0) {
            return &table->entries[i].value;
        }
    }
    
    return NULL;
}

int function_table_has(FunctionTable* table, const char* name) {
    return function_table_get(table, name) != NULL;
}

// Forward declaration
static void interpreter_execute_statement(ASTNode* stmt);

Value interpreter_evaluate_expression(ASTNode* expr) {
    if (expr == NULL) {
        return value_nil();
    }
    
    switch (expr->type) {
        case AST_LITERAL: {
            if (expr->as.literal.type == LIT_NUMBER) {
                return value_number(expr->as.literal.value.number);
            } else if (expr->as.literal.type == LIT_STRING) {
                // String - precisa copiar porque pode ser liberada depois
                char* str = string_copy(expr->as.literal.value.string,
                                       strlen(expr->as.literal.value.string));
                return value_string(str);
            } else if (expr->as.literal.type == LIT_BOOL) {
                return value_bool(expr->as.literal.value.boolean);
            }
            return value_nil();
        }
        
        case AST_IDENTIFIER: {
            // Primeiro tenta buscar no environment
            if (environment_has(g_env, expr->as.identifier.name)) {
                return environment_get(g_env, expr->as.identifier.name);
            }
            
            // Se não está no environment, tenta buscar na tabela de funções
            // (para suportar referências a funções por nome - first-class functions)
            Function* func = function_table_get(g_function_table, expr->as.identifier.name);
            if (func != NULL) {
                // Retorna função como valor (first-class)
                return value_function(func);
            }
            
            // Não encontrou nem no environment nem na tabela de funções
            fprintf(stderr, "Erro: Variável ou função '%s' não definida\n", expr->as.identifier.name);
            return value_nil();
        }
        
        case AST_FUNCTION_CALL: {
            // Suporta chamadas diretas (por nome) e indiretas (por variável)
            Function* func = NULL;
            
            // Tenta buscar função por nome primeiro (chamada direta)
            func = function_table_get(g_function_table, expr->as.function_call.name);
            
            // Se não encontrou, tenta buscar no environment (chamada indireta)
            if (func == NULL) {
                Value func_value = environment_get(g_env, expr->as.function_call.name);
                if (func_value.type == VAL_OBJ && func_value.as.obj->type == OBJ_FUNCTION) {
                    func = ((ObjFunction*)func_value.as.obj)->func;
                }
                value_destroy(func_value);
            }
            
            if (func == NULL) {
                fprintf(stderr, "Erro: Função '%s' não definida\n", expr->as.function_call.name);
                return value_nil();
            }
            
            // Verifica número de argumentos
            if (expr->as.function_call.argument_count != func->parameter_count) {
                fprintf(stderr, "Erro: Função '%s' espera %zu argumentos, mas recebeu %zu\n",
                       expr->as.function_call.name, func->parameter_count, expr->as.function_call.argument_count);
                return value_nil();
            }
            
            // Avalia argumentos (+1 cada)
            Value* arg_values = (Value*)malloc(sizeof(Value) * expr->as.function_call.argument_count);
            if (arg_values == NULL) {
                return value_nil();
            }
            
            for (size_t i = 0; i < expr->as.function_call.argument_count; i++) {
                arg_values[i] = interpreter_evaluate_expression(expr->as.function_call.arguments[i]);
            }
            
            // Push frame: cria novo escopo (frame) filho do atual
            g_env = frame_push(g_env);
            
            // Define parâmetros no novo frame
            for (size_t i = 0; i < func->parameter_count; i++) {
                environment_set(g_env, func->parameters[i], arg_values[i]);
                // Libera posse da avaliação, pois environment_set reteve
                value_destroy(arg_values[i]);
            }
            
            // Executa corpo da função
            Value return_value = value_nil();

            if (func->body->type == AST_BLOCK) {
                for (size_t i = 0; i < func->body->as.block.count; i++) {
                    ASTNode* stmt = func->body->as.block.statements[i];
                    if (stmt->type == AST_RETURN) {
                        if (stmt->as.return_stmt.value) {
                            return_value = interpreter_evaluate_expression(stmt->as.return_stmt.value);
                        }
                        break;
                    }
                    interpreter_execute_statement(stmt);
                }
            } else {
                if (func->body->type == AST_RETURN) {
                    if (func->body->as.return_stmt.value) {
                        return_value = interpreter_evaluate_expression(func->body->as.return_stmt.value);
                    }
                } else {
                    interpreter_execute_statement(func->body);
                }
            }
            
            // Pop frame: restaura escopo anterior (destrói o frame atual)
            g_env = frame_pop(g_env);
            
            free(arg_values);
            
            return return_value;
        }
        
        case AST_UNARY_EXPRESSION: {
            Value operand = interpreter_evaluate_expression(expr->as.unary_expr.operand);
            if (expr->as.unary_expr.operator == TOKEN_NOT) {
                int is_true = 0;
                if (operand.type == VAL_BOOL) {
                    is_true = operand.as.boolean;
                } else if (operand.type == VAL_NUMBER) {
                    is_true = operand.as.number != 0;
                } else if (value_is_string(operand)) {
                    is_true = strlen(value_to_chars(operand)) > 0;
                }
                value_destroy(operand);
                return value_bool(!is_true);
            }
            value_destroy(operand);
            return value_nil();
        }
        
        case AST_BINARY_EXPRESSION: {
            Value left = interpreter_evaluate_expression(expr->as.binary_expr.left);
            Value right = interpreter_evaluate_expression(expr->as.binary_expr.right);
            
            // Operações aritméticas
            if (expr->as.binary_expr.operator == TOKEN_PLUS) {
                // Concatenação de strings ou soma de números
                if (value_is_string(left) || value_is_string(right)) {
                    // Converte para string e concatena
                    char left_buf[64];
                    char right_buf[64];
                    const char* left_chars = "";
                    const char* right_chars = "";
                    
                    if (left.type == VAL_NUMBER) {
                        snprintf(left_buf, 64, "%g", left.as.number);
                        left_chars = left_buf;
                    } else if (value_is_string(left)) {
                        left_chars = value_to_chars(left);
                    }
                    
                    if (right.type == VAL_NUMBER) {
                        snprintf(right_buf, 64, "%g", right.as.number);
                        right_chars = right_buf;
                    } else if (value_is_string(right)) {
                        right_chars = value_to_chars(right);
                    }
                    
                    size_t len = strlen(left_chars) + strlen(right_chars);
                    char* result_str = (char*)malloc(len + 1);
                    snprintf(result_str, len + 1, "%s%s", left_chars, right_chars);
                    
                    Value result = value_string(result_str);
                    free(result_str);
                    
                    value_destroy(left);
                    value_destroy(right);
                    return result;
                } else {
                    // Soma de números
                    double result = left.as.number + right.as.number;
                    value_destroy(left);
                    value_destroy(right);
                    return value_number(result);
                }
            }
            
            // Outras operações aritméticas (apenas números)
            if (left.type != VAL_NUMBER || right.type != VAL_NUMBER) {
                fprintf(stderr, "Erro: Operação aritmética requer números\n");
                value_destroy(left);
                value_destroy(right);
                return value_nil();
            }
            
            double result = 0;
            switch (expr->as.binary_expr.operator) {
                case TOKEN_MINUS:
                    result = left.as.number - right.as.number;
                    break;
                case TOKEN_MULTIPLY:
                    result = left.as.number * right.as.number;
                    break;
                case TOKEN_DIVIDE:
                    if (right.as.number == 0) {
                        fprintf(stderr, "Erro: Divisão por zero\n");
                        value_destroy(left);
                        value_destroy(right);
                        return value_nil();
                    }
                    result = left.as.number / right.as.number;
                    break;
                case TOKEN_GT:
                    value_destroy(left);
                    value_destroy(right);
                    return value_bool(left.as.number > right.as.number);
                case TOKEN_LT:
                    value_destroy(left);
                    value_destroy(right);
                    return value_bool(left.as.number < right.as.number);
                case TOKEN_GTE:
                    value_destroy(left);
                    value_destroy(right);
                    return value_bool(left.as.number >= right.as.number);
                case TOKEN_LTE:
                    value_destroy(left);
                    value_destroy(right);
                    return value_bool(left.as.number <= right.as.number);
                case TOKEN_EQ:
                    // Comparação de igualdade
                    if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
                        int result = left.as.number == right.as.number;
                        value_destroy(left);
                        value_destroy(right);
                        return value_bool(result);
                    } else if (value_is_string(left) && value_is_string(right)) {
                        int result = strcmp(value_to_chars(left), value_to_chars(right)) == 0;
                        value_destroy(left);
                        value_destroy(right);
                        return value_bool(result);
                    } else {
                        value_destroy(left);
                        value_destroy(right);
                        return value_bool(0);
                    }
                case TOKEN_NE:
                    // Comparação de diferença
                    if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
                        int result = left.as.number != right.as.number;
                        value_destroy(left);
                        value_destroy(right);
                        return value_bool(result);
                    } else if (value_is_string(left) && value_is_string(right)) {
                        int result = strcmp(value_to_chars(left), value_to_chars(right)) != 0;
                        value_destroy(left);
                        value_destroy(right);
                        return value_bool(result);
                    } else {
                        value_destroy(left);
                        value_destroy(right);
                        return value_bool(1);
                    }
                case TOKEN_AND:
                    // E lógico
                    {
                        int left_bool = (left.type == VAL_NUMBER && left.as.number != 0) ||
                                        (value_is_string(left) && strlen(value_to_chars(left)) > 0) ||
                                        (left.type == VAL_BOOL && left.as.boolean);
                        int right_bool = (right.type == VAL_NUMBER && right.as.number != 0) ||
                                         (value_is_string(right) && strlen(value_to_chars(right)) > 0) ||
                                         (right.type == VAL_BOOL && right.as.boolean);
                        value_destroy(left);
                        value_destroy(right);
                        return value_bool(left_bool && right_bool);
                    }
                case TOKEN_OR:
                    // OU lógico
                    {
                        int left_bool = (left.type == VAL_NUMBER && left.as.number != 0) ||
                                        (value_is_string(left) && strlen(value_to_chars(left)) > 0) ||
                                        (left.type == VAL_BOOL && left.as.boolean);
                        int right_bool = (right.type == VAL_NUMBER && right.as.number != 0) ||
                                         (value_is_string(right) && strlen(value_to_chars(right)) > 0) ||
                                         (right.type == VAL_BOOL && right.as.boolean);
                        value_destroy(left);
                        value_destroy(right);
                        return value_bool(left_bool || right_bool);
                    }
                default:
                    fprintf(stderr, "Erro: Operador não suportado\n");
                    value_destroy(left);
                    value_destroy(right);
                    return value_nil();
            }
            
            value_destroy(left);
            value_destroy(right);
            return value_number(result);
        }
        
        default:
            fprintf(stderr, "Erro: Tipo de expressão não suportado\n");
            return value_nil();
    }
}

static void interpreter_execute_statement(ASTNode* stmt) {
    if (stmt == NULL) return;
    
    switch (stmt->type) {
        case AST_VARIABLE_DECLARATION: {
            Value value = interpreter_evaluate_expression(stmt->as.variable_decl.value);
            environment_set(g_env, stmt->as.variable_decl.name, value);
            value_destroy(value); // Libera posse da avaliação
            break;
        }
        
        case AST_FUNCTION_DECLARATION: {
            Function func;
            func.name = stmt->as.function_decl.name;
            func.parameters = stmt->as.function_decl.parameters;
            func.parameter_count = stmt->as.function_decl.parameter_count;
            func.body = stmt->as.function_decl.body;
            function_table_set(g_function_table, func.name, func);
            break;
        }
        
        case AST_RETURN: {
            // Return é tratado dentro de funções
            // Por enquanto, apenas avalia o valor
            if (stmt->as.return_stmt.value) {
                Value val = interpreter_evaluate_expression(stmt->as.return_stmt.value);
                value_destroy(val);
            }
            break;
        }
        
        case AST_PRINT: {
            Value value = interpreter_evaluate_expression(stmt->as.print_stmt.expression);
            value_print(value);
            printf("\n");
            value_destroy(value);
            break;
        }
        
        case AST_IF_STATEMENT: {
            Value condition = interpreter_evaluate_expression(stmt->as.if_stmt.condition);
            int is_true = 0;
            
            if (condition.type == VAL_BOOL) {
                is_true = condition.as.boolean;
            } else if (condition.type == VAL_NUMBER) {
                is_true = condition.as.number != 0;
            } else if (value_is_string(condition)) {
                is_true = strlen(value_to_chars(condition)) > 0;
            }
            
            value_destroy(condition);
            
            if (is_true) {
                interpreter_execute_statement(stmt->as.if_stmt.then_branch);
            } else if (stmt->as.if_stmt.else_branch != NULL) {
                interpreter_execute_statement(stmt->as.if_stmt.else_branch);
            }
            break;
        }
        
        case AST_WHILE_STATEMENT: {
            for (;;) {
                Value condition = interpreter_evaluate_expression(stmt->as.while_stmt.condition);
                int is_true = 0;
                
                if (condition.type == VAL_BOOL) {
                    is_true = condition.as.boolean;
                } else if (condition.type == VAL_NUMBER) {
                    is_true = condition.as.number != 0;
                } else if (value_is_string(condition)) {
                    is_true = strlen(value_to_chars(condition)) > 0;
                }
                
                value_destroy(condition);
                
                if (!is_true) {
                    break;
                }
                
                interpreter_execute_statement(stmt->as.while_stmt.body);
            }
            break;
        }
        
        case AST_FOR_STATEMENT: {
            // Executa inicialização
            if (stmt->as.for_stmt.init != NULL) {
                interpreter_execute_statement(stmt->as.for_stmt.init);
            }
            
            // Loop
            for (;;) {
                // Verifica condição
                if (stmt->as.for_stmt.condition != NULL) {
                    Value condition = interpreter_evaluate_expression(stmt->as.for_stmt.condition);
                    int is_true = 0;
                    
                    if (condition.type == VAL_BOOL) {
                        is_true = condition.as.boolean;
                    } else if (condition.type == VAL_NUMBER) {
                        is_true = condition.as.number != 0;
                    } else if (value_is_string(condition)) {
                        is_true = strlen(value_to_chars(condition)) > 0;
                    }
                    
                    value_destroy(condition);
                    
                    if (!is_true) {
                        break;
                    }
                }
                
                // Executa corpo
                interpreter_execute_statement(stmt->as.for_stmt.body);
                
                // Executa incremento (pode ser statement ou expression)
                if (stmt->as.for_stmt.increment != NULL) {
                    if (stmt->as.for_stmt.increment->type == AST_VARIABLE_DECLARATION) {
                        // É uma declaração (let i = i + 1)
                        interpreter_execute_statement(stmt->as.for_stmt.increment);
                    } else {
                        // É uma expressão
                        Value inc = interpreter_evaluate_expression(stmt->as.for_stmt.increment);
                        value_destroy(inc);
                    }
                }
            }
            break;
        }
        
        case AST_BLOCK: {
            for (size_t i = 0; i < stmt->as.block.count; i++) {
                interpreter_execute_statement(stmt->as.block.statements[i]);
            }
            break;
        }
        
        default:
            fprintf(stderr, "Erro: Tipo de instrução não suportado\n");
            break;
    }
}

int interpreter_execute(ASTNode* ast) {
    if (ast == NULL) {
        return 1;
    }
    
    // Cria environment global (frame raiz, sem parent)
    g_env = environment_create(NULL);
    if (g_env == NULL) {
        fprintf(stderr, "Erro: Não foi possível criar environment global\n");
        return 1;
    }
    
    // Cria tabela de funções global
    g_function_table = function_table_create();
    if (g_function_table == NULL) {
        fprintf(stderr, "Erro: Não foi possível criar tabela de funções\n");
        environment_destroy(g_env);
        return 1;
    }
    
    // Executa o bloco principal
    if (ast->type == AST_BLOCK) {
        for (size_t i = 0; i < ast->as.block.count; i++) {
            interpreter_execute_statement(ast->as.block.statements[i]);
        }
    } else {
        interpreter_execute_statement(ast);
    }
    
    // Limpa environment e tabela de funções
    environment_destroy(g_env);
    function_table_destroy(g_function_table);
    g_env = NULL;
    g_function_table = NULL;
    
    return 0;
}

