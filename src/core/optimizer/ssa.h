#ifndef SSA_H
#define SSA_H

#include "../ast/ast.h"

// Realiza a transformação SSA na AST
void ssa_transform(ASTNode* node);

// Realiza análise de vida útil (Liveness) para Ownership 2.0
void ssa_analyze_liveness(ASTNode* node);

// Limpa a tabela SSA e libera memória
void ssa_cleanup(void);

#endif // SSA_H
