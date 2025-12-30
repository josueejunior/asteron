#ifndef VM_H
#define VM_H

#include <stddef.h>
#include "../core/ast/ast.h"
#include "../core/interpreter/interpreter.h"

// Forward declaration para evitar dependência circular
struct Task;
typedef struct Task Task;

// Opcodes da VM
typedef enum {
    // Stack operations
    OP_LOAD_CONST,      // Carrega constante no stack
    OP_LOAD_VAR,        // Carrega variável no stack
    OP_STORE_VAR,       // Armazena valor do stack em variável
    
    // Arithmetic operations
    OP_ADD,             // Soma dois valores do stack
    OP_SUB,             // Subtrai dois valores do stack
    OP_MUL,             // Multiplica dois valores do stack
    OP_DIV,             // Divide dois valores do stack
    
    // Comparison operations
    OP_EQ,              // Igualdade
    OP_NE,              // Desigualdade
    OP_GT,              // Maior que
    OP_LT,              // Menor que
    OP_GTE,             // Maior ou igual
    OP_LTE,             // Menor ou igual
    
    // Logical operations
    OP_AND,             // E lógico
    OP_OR,              // OU lógico
    OP_NOT,             // NÃO lógico
    OP_NEG,             // Negação aritmética
    
    // Control flow
    OP_JUMP,            // Salto incondicional
    OP_JUMP_IF_FALSE,   // Salto condicional (se falso)
    OP_JUMP_IF_TRUE,   // Salto condicional (se verdadeiro)
    
    // Function operations
    OP_CALL,            // Chama função
    OP_RETURN,          // Retorna de função
    
    // Other
    OP_PRINT,           // Imprime valor do stack
    OP_POP,             // Remove valor do topo do stack
    OP_RELEASE,         // Ownership 2.0: Libera variável explicitamente
    OP_STORE_LOCAL,     // Otimização: Armazena em registrador local (não escapa)
    OP_LOAD_LOCAL,      // Otimização: Carrega de registrador local
    OP_HALT             // Para execução
} OpCode;

// Valor constante (para OP_LOAD_CONST)
typedef struct {
    enum {
        CONST_NUMBER,
        CONST_STRING,
        CONST_BOOL,
        CONST_NIL
    } type;
    union {
        double number;
        char* string;
        int boolean;
    } value;
} Constant;

// Instrução de bytecode
typedef struct {
    OpCode op;
    union {
        size_t constant_index;  // Índice na tabela de constantes
        size_t var_index;       // Índice na tabela de variáveis
        size_t jump_target;     // Índice da instrução destino
        size_t function_index;  // Índice na tabela de funções
        size_t arg_count;       // Número de argumentos
    } operand;
} Instruction;

// Programa compilado (bytecode)
typedef struct {
    Instruction* instructions;
    size_t instruction_count;
    size_t instruction_capacity;
    
    Constant* constants;        // Tabela de constantes
    size_t constant_count;
    size_t constant_capacity;
    
    char** variable_names;      // Nomes de variáveis (para debug)
    size_t variable_count;
    size_t variable_capacity;
} BytecodeProgram;

// VM (Virtual Machine)
typedef struct {
    Stack* stack;               // Stack de valores (isolada)
    Environment* env;           // Environment (isolado)
    Value locals[32];           // Registradores locais rápidos (não escapam)
    FunctionTable* function_table;
    BytecodeProgram* program;
    size_t pc;
    int is_running;             // Flag para controle de execução
    int heat_map[1024];         // Contador de calor por PC
} VM;

// Funções do compilador (AST → Bytecode)
BytecodeProgram* compiler_compile(ASTNode* ast);
void bytecode_program_destroy(BytecodeProgram* program);
void bytecode_program_print(BytecodeProgram* program);

// Snapshot do estado da VM para migração ou resiliência
typedef struct {
    size_t pc;                  // Onde a execução parou
    Value locals[32];           // Estado dos registradores rápidos
    // No futuro: Buffer serializado do Environment
    int is_valid;
} VMSnapshot;

// Funções da VM
VM* vm_create(BytecodeProgram* program);
void vm_destroy(VM* vm);
int vm_execute(VM* vm);
int vm_execute_range(VM* vm, size_t start, size_t end);
int vm_step(VM* vm, ValueType* observed_type, int* stability_count); 
void vm_print_stack(VM* vm);

// Funções de Snapshot
VMSnapshot vm_take_snapshot(VM* vm);
void vm_restore_snapshot(VM* vm, VMSnapshot snapshot);
void vm_snapshot_destroy(VMSnapshot* snapshot);

#endif // VM_H
