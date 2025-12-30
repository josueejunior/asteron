#ifndef AST_H
#define AST_H

#include <stddef.h>

// --- Forward Declarations ---
typedef struct ASTNode ASTNode;

// Sistema de Tipos
typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_STRING,
    TYPE_VOID,
    TYPE_UNKNOWN,
    TYPE_ERROR
} Type;

typedef struct {
    Type type;
    int is_inferred;
} TypeInfo;

// Sistema de Anotações
typedef enum {
    ANNOTATION_PARALLEL,
    ANNOTATION_DEPENDS,
    ANNOTATION_ASYNC,
    ANNOTATION_CACHE
} AnnotationType;

typedef struct Annotation {
    AnnotationType type;
    char** dependencies;
    size_t dep_count;
    struct Annotation* next;
} Annotation;

// Tipos de nós da AST
typedef enum {
    AST_VARIABLE_DECLARATION,
    AST_EXPRESSION,
    AST_BINARY_EXPRESSION,
    AST_UNARY_EXPRESSION,
    AST_LITERAL,
    AST_IDENTIFIER,
    AST_PRINT,
    AST_IF_STATEMENT,
    AST_WHILE_STATEMENT,
    AST_FOR_STATEMENT,
    AST_FUNCTION_DECLARATION,
    AST_FUNCTION_CALL,
    AST_RETURN,
    AST_BLOCK,
    AST_ASSIGNMENT,
    AST_PHI
} ASTNodeType;

// Estruturas de Dados dos Nós
typedef struct {
    char* name;
    ASTNode* value;
} Assignment;

typedef struct {
    char* name;
    int* versions;
    size_t version_count;
    int result_version;
} PhiNode;

typedef struct {
    ASTNode* left;
    ASTNode* right;
    int operator;
} BinaryExpression;

typedef struct {
    ASTNode* operand;
    int operator;
} UnaryExpression;

typedef struct {
    enum { LIT_NUMBER, LIT_STRING, LIT_BOOL } type;
    union { double number; char* string; int boolean; } value;
} Literal;

typedef struct {
    char* name;
    int ssa_version;
    int is_last_use;
} Identifier;

typedef struct {
    char* name;
    ASTNode* value;
    TypeInfo* type_info;
    Annotation* annotations;
    int ssa_version;
    int escapes;
} VariableDeclaration;

typedef struct {
    ASTNode* expression;
    Annotation* annotations;
} PrintStatement;

typedef struct {
    ASTNode* condition;
    ASTNode* then_branch;
    ASTNode* else_branch;
} IfStatement;

typedef struct {
    ASTNode* condition;
    ASTNode* body;
} WhileStatement;

typedef struct {
    ASTNode* init;
    ASTNode* condition;
    ASTNode* increment;
    ASTNode* body;
} ForStatement;

typedef struct {
    char* name;
    char** parameters;
    TypeInfo** param_types;
    size_t parameter_count;
    TypeInfo* return_type;
    ASTNode* body;
} FunctionDeclaration;

typedef struct {
    char* name;
    ASTNode** arguments;
    size_t argument_count;
} FunctionCall;

typedef struct {
    ASTNode* value;
} ReturnStatement;

typedef struct {
    ASTNode** statements;
    size_t count;
    size_t capacity;
} Block;

// Estrutura Principal do Nó
struct ASTNode {
    ASTNodeType type;
    TypeInfo* inferred_type;
    Annotation* annotations;
    int is_readonly;
    size_t start_pc;
    size_t end_pc;
    union {
        VariableDeclaration variable_decl;
        BinaryExpression binary_expr;
        UnaryExpression unary_expr;
        Literal literal;
        Identifier identifier;
        PrintStatement print_stmt;
        IfStatement if_stmt;
        WhileStatement while_stmt;
        ForStatement for_stmt;
        FunctionDeclaration function_decl;
        FunctionCall function_call;
        ReturnStatement return_stmt;
        Block block;
        Assignment assignment;
        PhiNode phi;
    } as;
};

// Funções da AST
ASTNode* ast_create_node(ASTNodeType type);
void ast_destroy_node(ASTNode* node);
void ast_print_node(ASTNode* node, int indent);
void ast_mark_readonly(ASTNode* node);

// Funções de tipo
TypeInfo* type_info_create(Type type, int is_inferred);
void type_info_destroy(TypeInfo* info);
const char* type_to_string(Type type);

// Funções de anotações
Annotation* annotation_create(AnnotationType type);
void annotation_destroy(Annotation* annotation);
void annotation_add_dependency(Annotation* annotation, const char* dep);
const char* annotation_type_to_string(AnnotationType type);

// Construtores
ASTNode* ast_variable_declaration(char* name, ASTNode* value);
ASTNode* ast_variable_declaration_with_type(char* name, ASTNode* value, TypeInfo* type_info);
ASTNode* ast_binary_expression(ASTNode* left, int op, ASTNode* right);
ASTNode* ast_literal_number(double value);
ASTNode* ast_literal_string(char* value);
ASTNode* ast_literal_bool(int value);
ASTNode* ast_identifier(char* name);
ASTNode* ast_print(ASTNode* expression);
ASTNode* ast_if_statement(ASTNode* condition, ASTNode* then_branch, ASTNode* else_branch);
ASTNode* ast_while_statement(ASTNode* condition, ASTNode* body);
ASTNode* ast_for_statement(ASTNode* init, ASTNode* condition, ASTNode* increment, ASTNode* body);
ASTNode* ast_function_declaration(char* name, char** parameters, size_t param_count, ASTNode* body);
ASTNode* ast_function_declaration_with_types(char* name, char** parameters, TypeInfo** param_types, size_t param_count, TypeInfo* return_type, ASTNode* body);
ASTNode* ast_function_call(char* name, ASTNode** arguments, size_t arg_count);
ASTNode* ast_return(ASTNode* value);
ASTNode* ast_assignment(char* name, ASTNode* value);
ASTNode* ast_block(void);
void ast_block_add_statement(ASTNode* block, ASTNode* statement);
ASTNode* ast_phi(char* name);
void ast_phi_add_version(ASTNode* phi_node, int version);

#endif // AST_H
