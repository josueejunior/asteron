#ifndef REG_ALLOC_H
#define REG_ALLOC_H

#include "../ast/ast.h"

// Realiza a alocação de registradores via coloração de grafo
void register_allocation_transform(ASTNode* node);

// Loop-Invariant Code Motion (LICM)
void loop_invariant_code_motion(ASTNode* node);

#endif // REG_ALLOC_H

