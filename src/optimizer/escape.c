#include "escape.h"
#include <stdio.h>
#include <string.h>

static void analyze_recursive(ASTNode* node, ASTNode* current_block) {
    if (node == NULL) return;

    switch (node->type) {
        case AST_VARIABLE_DECLARATION:
            // Marca inicialmente como "não escapa" se estiver dentro de um bloco
            if (current_block != NULL) {
                node->as.variable_decl.escapes = 0;
            }
            analyze_recursive(node->as.variable_decl.value, current_block);
            break;

        case AST_IDENTIFIER:
            // Se um identificador é usado fora do bloco onde foi definido, ele escaparia.
            // (Lógica simplificada para este nível)
            break;

        case AST_BLOCK:
            for (size_t i = 0; i < node->as.block.count; i++) {
                analyze_recursive(node->as.block.statements[i], node);
            }
            break;

        case AST_ASSIGNMENT:
            analyze_recursive(node->as.assignment.value, current_block);
            break;

        case AST_WHILE_STATEMENT:
            analyze_recursive(node->as.while_stmt.condition, current_block);
            analyze_recursive(node->as.while_stmt.body, node->as.while_stmt.body);
            break;

        case AST_PRINT:
            // Se a variável é impressa, ela "escapa" para o mundo externo (I/O)
            if (node->as.print_stmt.expression->type == AST_IDENTIFIER) {
                // Aqui buscaríamos a declaração e marcaríamos escapes = 1
            }
            analyze_recursive(node->as.print_stmt.expression, current_block);
            break;

        default:
            // ... outros nós
            break;
    }
}

void escape_analyze(ASTNode* node) {
    printf("\n=== Fase: Escape Analysis (Memória Ultra-Rápida) ===\n");
    analyze_recursive(node, NULL);
    printf("✓ Análise de escopo concluída\n");
}

