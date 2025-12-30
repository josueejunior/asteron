#ifndef TYPECHECKER_H
#define TYPECHECKER_H

#include "../ast/ast.h"

// Estrutura do type checker
typedef struct {
    int had_error;
    int error_count;
} TypeChecker;

// Funções do type checker
TypeChecker* typechecker_create(void);
void typechecker_destroy(TypeChecker* checker);
int typechecker_check(ASTNode* ast);
int typechecker_had_error(TypeChecker* checker);

// Funções auxiliares
Type infer_type_from_literal(ASTNode* node);
Type infer_type_from_expression(ASTNode* node);
int types_compatible(Type expected, Type actual);
const char* type_error_message(Type expected, Type actual);

#endif // TYPECHECKER_H
