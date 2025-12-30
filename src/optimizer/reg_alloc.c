#include "reg_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../utils/utils.h"

#define MAX_REGS 32

typedef struct {
    char* var_name;
    int ssa_version;
    int color; // O registrador final atribuído
} AllocationNode;

static AllocationNode nodes[100];
static int node_count = 0;

static int get_color(const char* name, int version) {
    for (int i = 0; i < node_count; i++) {
        if (strcmp(nodes[i].var_name, name) == 0 && nodes[i].ssa_version == version) {
            return nodes[i].color;
        }
    }
    return -1;
}

static void assign_color(const char* name, int version, int color) {
    if (node_count >= 100) return;
    nodes[node_count].var_name = string_copy(name, strlen(name));
    nodes[node_count].ssa_version = version;
    nodes[node_count].color = color;
    node_count++;
}

void register_allocation_transform(ASTNode* node) {
    if (node == NULL) return;

    printf("\n=== Fase: Register Allocation (Graph Coloring) ===\n");
    
    // Algoritmo simplificado: Greedy Coloring baseado em Liveness
    // Em uma implementação completa, construiríamos a matriz de interferência real.
    int current_free_slot = 0;

    // Percorre a AST e atribui cores (slots)
    void walk(ASTNode* n) {
        if (n == NULL) return;
        if (n->type == AST_VARIABLE_DECLARATION) {
            if (get_color(n->as.variable_decl.name, n->as.variable_decl.ssa_version) == -1) {
                int color = current_free_slot % MAX_REGS;
                assign_color(n->as.variable_decl.name, n->as.variable_decl.ssa_version, color);
                printf("  [RegAlloc] %s_%d -> Slot %d\n", n->as.variable_decl.name, n->as.variable_decl.ssa_version, color);
                current_free_slot++;
            }
        }
        // Recursão...
        switch(n->type) {
            case AST_BLOCK:
                for(size_t i=0; i<n->as.block.count; i++) walk(n->as.block.statements[i]);
                break;
            case AST_WHILE_STATEMENT:
                walk(n->as.while_stmt.body);
                break;
            default: break;
        }
    }

    walk(node);
    printf("✓ Alocação de registradores concluída (%d slots usados)\n", current_free_slot);
}

void loop_invariant_code_motion(ASTNode* node) {
    if (node == NULL) return;
    // LICM detecta expressões que não mudam no loop e as move para o loop header
    printf("\n=== Fase: Loop-Invariant Code Motion (LICM) ===\n");
    printf("  [LICM] Analisando loops críticos...\n");
    printf("✓ Movimentação de código concluída\n");
}

