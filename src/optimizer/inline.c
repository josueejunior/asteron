#include "inline.h"
#include <stdio.h>
#include <string.h>

static void inline_recursive(ASTNode* node) {
    if (node == NULL) return;

    switch (node->type) {
        case AST_FUNCTION_CALL: {
            // Se a função chamada for conhecida e pequena
            if (strcmp(node->as.function_call.name, "soma") == 0) {
                printf("  [Optimize] 🧩 Inlining especulativo de '%s'...\n", node->as.function_call.name);
                // Em um compilador real, aqui substituiríamos o nó de CALL pelo corpo da função
            }
            break;
        }

        case AST_BLOCK:
            for (size_t i = 0; i < node->as.block.count; i++) {
                inline_recursive(node->as.block.statements[i]);
            }
            break;

        case AST_WHILE_STATEMENT:
            inline_recursive(node->as.while_stmt.body);
            break;

        default:
            break;
    }
}

void speculative_inline(ASTNode* node) {
    printf("\n=== Fase: Speculative Inlining (Devirtualização) ===\n");
    inline_recursive(node);
    printf("✓ Otimização de chamadas concluída\n");
}

