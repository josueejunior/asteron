#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "../ast/ast.h"

// Função (para armazenar declarações de funções)
typedef struct {
    char* name;
    char** parameters;
    size_t parameter_count;
    ASTNode* body;
} Function;

// --- Sistema de Objetos com Reference Counting ---

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION
} ObjType;

typedef struct Obj {
    ObjType type;
    int ref_count;
} Obj;

typedef struct {
    Obj obj;
    char* chars;
    size_t length;
} ObjString;

typedef struct {
    Obj obj;
    Function* func;
} ObjFunction;

// --- Tipos de valores na linguagem ---

typedef enum {
    VAL_NUMBER,
    VAL_OBJ,      // Objeto com RefCounting (string, função, etc.)
    VAL_BOOL,
    VAL_NIL
} ValueType;

// Estrutura de valor
typedef struct {
    ValueType type;
    union {
        double number;
        Obj* obj;
        int boolean;
    } as;
} Value;

// Environment (escopo com parent para cadeia de escopos)
typedef struct Environment Environment;

// Stack de Valores
typedef struct {
    Value* items;
    size_t size;
    size_t capacity;
} Stack;

Stack* stack_create(size_t capacity);
void stack_destroy(Stack* stack);
void stack_push(Stack* stack, Value value);
Value stack_pop(Stack* stack);
Value stack_peek(Stack* stack, size_t offset);

// Tabela de funções
typedef struct FunctionTable FunctionTable;

// Funções da tabela de funções
FunctionTable* function_table_create(void);
void function_table_destroy(FunctionTable* table);
void function_table_set(FunctionTable* table, const char* name, Function func);
Function* function_table_get(FunctionTable* table, const char* name);
int function_table_has(FunctionTable* table, const char* name);

// Funções do interpretador
int interpreter_execute(ASTNode* ast);
Value interpreter_evaluate_expression(ASTNode* expr);
void value_print(Value value);
void value_destroy(Value value);

// Funções de RefCounting
void value_retain(Value value);
void value_release(Value value);

// Funções de objetos
ObjString* obj_string_create(const char* chars, size_t length);
ObjFunction* obj_function_create(Function* func);
void obj_destroy(Obj* obj);

// Funções do Environment (escopos)
Environment* environment_create(Environment* parent);
void environment_destroy(Environment* env);
void environment_set(Environment* env, const char* name, Value value);
Value environment_get(Environment* env, const char* name);
int environment_has(Environment* env, const char* name);
void environment_remove(Environment* env, const char* name);

// Funções de frame (push/pop)
Environment* frame_push(Environment* current);
Environment* frame_pop(Environment* current);

// Construtores de valores
Value value_number(double number);
Value value_string(char* string);
Value value_bool(int boolean);
Value value_nil(void);
Value value_function(Function* function);

// Auxiliares de valores
int value_is_string(Value value);
const char* value_to_chars(Value value);

#endif // INTERPRETER_H

