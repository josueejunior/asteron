/**
 * =============================================================================
 * ASTERON OWNERSHIP SYSTEM - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "ownership.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * BORROW CHECKER
 * ============================================================================= */

BorrowChecker* borrow_checker_create(void) {
    BorrowChecker* bc = (BorrowChecker*)calloc(1, sizeof(BorrowChecker));
    if (bc == NULL) return NULL;
    
    /* Variáveis */
    bc->var_capacity = 64;
    bc->variables = calloc(bc->var_capacity, sizeof(*bc->variables));
    bc->var_count = 0;
    
    /* Lifetimes */
    bc->lifetime_capacity = 32;
    bc->lifetimes = calloc(bc->lifetime_capacity, sizeof(Lifetime));
    bc->lifetime_count = 0;
    bc->next_lifetime_id = 1;  /* 0 é TEMPORARY */
    
    /* Erros */
    bc->error_capacity = 16;
    bc->errors = calloc(bc->error_capacity, sizeof(*bc->errors));
    bc->error_count = 0;
    
    bc->active_borrows = NULL;
    bc->current_scope = 0;
    bc->current_line = 1;
    
    return bc;
}

void borrow_checker_destroy(BorrowChecker* bc) {
    if (bc == NULL) return;
    
    /* Libera variáveis */
    for (size_t i = 0; i < bc->var_count; i++) {
        free(bc->variables[i].name);
    }
    free(bc->variables);
    
    /* Libera lifetimes */
    for (size_t i = 0; i < bc->lifetime_count; i++) {
        free((void*)bc->lifetimes[i].name);
    }
    free(bc->lifetimes);
    
    /* Libera borrows */
    BorrowRegion* br = bc->active_borrows;
    while (br) {
        BorrowRegion* next = br->next;
        free(br);
        br = next;
    }
    
    /* Libera erros */
    for (size_t i = 0; i < bc->error_count; i++) {
        free(bc->errors[i].message);
    }
    free(bc->errors);
    
    free(bc);
}

static void add_error(BorrowChecker* bc, OwnershipError err, const char* ctx) {
    if (bc->error_count >= bc->error_capacity) {
        bc->error_capacity *= 2;
        bc->errors = realloc(bc->errors, sizeof(*bc->errors) * bc->error_capacity);
    }
    
    char* msg = malloc(256);
    snprintf(msg, 256, "[Linha %u] %s: %s", 
             bc->current_line, ownership_error_message(err), ctx ? ctx : "");
    
    bc->errors[bc->error_count].line = bc->current_line;
    bc->errors[bc->error_count].message = msg;
    bc->error_count++;
}

uint32_t borrow_checker_add_var(BorrowChecker* bc, const char* name,
                                 MemoryLayout layout) {
    if (bc == NULL) return 0;
    
    /* Expande se necessário */
    if (bc->var_count >= bc->var_capacity) {
        bc->var_capacity *= 2;
        bc->variables = realloc(bc->variables, 
            sizeof(*bc->variables) * bc->var_capacity);
    }
    
    uint32_t id = (uint32_t)bc->var_count;
    
    bc->variables[id].name = strdup(name);
    bc->variables[id].id = id;
    bc->variables[id].layout = layout;
    
    /* Ownership inicial: owned */
    bc->variables[id].ownership = (OwnershipInfo){
        .state = OWNER_OWNED,
        .lifetime = LIFETIME_STATIC,
        .borrow_count = 0,
        .is_mut_borrowed = false,
        .owner_id = id,
        .moved_to = 0,
        .def_line = bc->current_line,
        .last_use_line = bc->current_line
    };
    
    bc->var_count++;
    
    return id;
}

static OwnershipInfo* get_ownership(BorrowChecker* bc, uint32_t var_id) {
    if (var_id >= bc->var_count) return NULL;
    return &bc->variables[var_id].ownership;
}

OwnershipError borrow_checker_borrow(BorrowChecker* bc, uint32_t var_id,
                                      LifetimeId lifetime) {
    OwnershipInfo* info = get_ownership(bc, var_id);
    if (info == NULL) return OWN_ERR_DANGLING_REF;
    
    /* Verifica estado atual */
    if (info->state == OWNER_MOVED) {
        add_error(bc, OWN_ERR_USE_AFTER_MOVE, bc->variables[var_id].name);
        return OWN_ERR_USE_AFTER_MOVE;
    }
    
    if (info->state == OWNER_DROPPED) {
        add_error(bc, OWN_ERR_DANGLING_REF, bc->variables[var_id].name);
        return OWN_ERR_DANGLING_REF;
    }
    
    /* Não pode borrow imutável se já tem borrow mutável */
    if (info->is_mut_borrowed) {
        add_error(bc, OWN_ERR_BORROW_WHILE_MUT, bc->variables[var_id].name);
        return OWN_ERR_BORROW_WHILE_MUT;
    }
    
    /* OK - registra borrow */
    info->borrow_count++;
    info->last_use_line = bc->current_line;
    
    /* Adiciona região de borrow */
    BorrowRegion* br = malloc(sizeof(BorrowRegion));
    br->var_id = var_id;
    br->borrow_type = OWNER_BORROWED;
    br->lifetime = lifetime;
    br->start_line = bc->current_line;
    br->end_line = 0;  /* Será preenchido quando o borrow terminar */
    br->next = bc->active_borrows;
    bc->active_borrows = br;
    
    return OWN_ERR_NONE;
}

OwnershipError borrow_checker_borrow_mut(BorrowChecker* bc, uint32_t var_id,
                                          LifetimeId lifetime) {
    OwnershipInfo* info = get_ownership(bc, var_id);
    if (info == NULL) return OWN_ERR_DANGLING_REF;
    
    /* Verifica estado */
    if (info->state == OWNER_MOVED) {
        add_error(bc, OWN_ERR_USE_AFTER_MOVE, bc->variables[var_id].name);
        return OWN_ERR_USE_AFTER_MOVE;
    }
    
    if (info->state == OWNER_DROPPED) {
        add_error(bc, OWN_ERR_DANGLING_REF, bc->variables[var_id].name);
        return OWN_ERR_DANGLING_REF;
    }
    
    /* Não pode mut borrow se já tem qualquer borrow */
    if (info->borrow_count > 0) {
        add_error(bc, OWN_ERR_MUT_BORROW_WHILE_BORROW, bc->variables[var_id].name);
        return OWN_ERR_MUT_BORROW_WHILE_BORROW;
    }
    
    if (info->is_mut_borrowed) {
        add_error(bc, OWN_ERR_BORROW_WHILE_MUT, bc->variables[var_id].name);
        return OWN_ERR_BORROW_WHILE_MUT;
    }
    
    /* OK - registra mut borrow */
    info->is_mut_borrowed = true;
    info->last_use_line = bc->current_line;
    
    /* Adiciona região */
    BorrowRegion* br = malloc(sizeof(BorrowRegion));
    br->var_id = var_id;
    br->borrow_type = OWNER_BORROWED_MUT;
    br->lifetime = lifetime;
    br->start_line = bc->current_line;
    br->end_line = 0;
    br->next = bc->active_borrows;
    bc->active_borrows = br;
    
    return OWN_ERR_NONE;
}

OwnershipError borrow_checker_move(BorrowChecker* bc, uint32_t from_var,
                                    uint32_t to_var) {
    OwnershipInfo* from = get_ownership(bc, from_var);
    OwnershipInfo* to = get_ownership(bc, to_var);
    
    if (from == NULL || to == NULL) return OWN_ERR_DANGLING_REF;
    
    /* Não pode mover se está borrowed */
    if (from->borrow_count > 0 || from->is_mut_borrowed) {
        add_error(bc, OWN_ERR_MOVE_WHILE_BORROWED, bc->variables[from_var].name);
        return OWN_ERR_MOVE_WHILE_BORROWED;
    }
    
    /* Não pode mover se já foi movido */
    if (from->state == OWNER_MOVED) {
        add_error(bc, OWN_ERR_USE_AFTER_MOVE, bc->variables[from_var].name);
        return OWN_ERR_USE_AFTER_MOVE;
    }
    
    /* Realiza move */
    from->state = OWNER_MOVED;
    from->moved_to = to_var;
    from->last_use_line = bc->current_line;
    
    to->state = OWNER_OWNED;
    to->owner_id = from->owner_id;
    to->def_line = bc->current_line;
    
    return OWN_ERR_NONE;
}

OwnershipError borrow_checker_drop(BorrowChecker* bc, uint32_t var_id) {
    OwnershipInfo* info = get_ownership(bc, var_id);
    if (info == NULL) return OWN_ERR_DANGLING_REF;
    
    /* Não pode dropar se está borrowed */
    if (info->borrow_count > 0 || info->is_mut_borrowed) {
        add_error(bc, OWN_ERR_MOVE_WHILE_BORROWED, bc->variables[var_id].name);
        return OWN_ERR_MOVE_WHILE_BORROWED;
    }
    
    /* Já foi dropado? */
    if (info->state == OWNER_DROPPED) {
        add_error(bc, OWN_ERR_DOUBLE_FREE, bc->variables[var_id].name);
        return OWN_ERR_DOUBLE_FREE;
    }
    
    /* Se foi movido, não precisa dropar (o novo dono dropa) */
    if (info->state == OWNER_MOVED) {
        return OWN_ERR_NONE;
    }
    
    info->state = OWNER_DROPPED;
    info->last_use_line = bc->current_line;
    
    return OWN_ERR_NONE;
}

void borrow_checker_unborrow(BorrowChecker* bc, uint32_t var_id) {
    OwnershipInfo* info = get_ownership(bc, var_id);
    if (info == NULL) return;
    
    /* Remove da lista de borrows ativos */
    BorrowRegion** pp = &bc->active_borrows;
    while (*pp != NULL) {
        if ((*pp)->var_id == var_id) {
            BorrowRegion* br = *pp;
            br->end_line = bc->current_line;
            
            if (br->borrow_type == OWNER_BORROWED) {
                if (info->borrow_count > 0) info->borrow_count--;
            } else {
                info->is_mut_borrowed = false;
            }
            
            *pp = br->next;
            free(br);
            return;
        }
        pp = &(*pp)->next;
    }
}

void borrow_checker_enter_scope(BorrowChecker* bc) {
    bc->current_scope++;
}

void borrow_checker_exit_scope(BorrowChecker* bc) {
    /* Dropa todas as variáveis do escopo atual */
    /* TODO: Rastrear escopo de cada variável */
    
    if (bc->current_scope > 0) {
        bc->current_scope--;
    }
}

LifetimeId borrow_checker_new_lifetime(BorrowChecker* bc, const char* name) {
    if (bc->lifetime_count >= bc->lifetime_capacity) {
        bc->lifetime_capacity *= 2;
        bc->lifetimes = realloc(bc->lifetimes, 
            sizeof(Lifetime) * bc->lifetime_capacity);
    }
    
    LifetimeId id = bc->next_lifetime_id++;
    
    bc->lifetimes[bc->lifetime_count] = (Lifetime){
        .id = id,
        .name = name ? strdup(name) : NULL,
        .scope_depth = bc->current_scope,
        .start_line = bc->current_line,
        .end_line = 0
    };
    bc->lifetime_count++;
    
    return id;
}

bool borrow_checker_lifetime_valid(BorrowChecker* bc, LifetimeId inner,
                                    LifetimeId outer) {
    /* 'static outlives everything */
    if (outer == LIFETIME_STATIC) return true;
    if (inner == LIFETIME_STATIC) return true;
    
    /* Temporary never outlives anything */
    if (inner == LIFETIME_TEMPORARY) return false;
    if (outer == LIFETIME_TEMPORARY) return true;
    
    /* Find lifetimes and compare scope depth */
    Lifetime* lt_inner = NULL;
    Lifetime* lt_outer = NULL;
    
    for (size_t i = 0; i < bc->lifetime_count; i++) {
        if (bc->lifetimes[i].id == inner) lt_inner = &bc->lifetimes[i];
        if (bc->lifetimes[i].id == outer) lt_outer = &bc->lifetimes[i];
    }
    
    if (lt_inner == NULL || lt_outer == NULL) return false;
    
    /* Inner must be at same or deeper scope */
    return lt_inner->scope_depth >= lt_outer->scope_depth;
}

bool borrow_checker_verify(BorrowChecker* bc) {
    return bc->error_count == 0;
}

const char* ownership_error_message(OwnershipError err) {
    switch (err) {
        case OWN_ERR_NONE: return "OK";
        case OWN_ERR_USE_AFTER_MOVE: 
            return "uso de valor após move";
        case OWN_ERR_DOUBLE_FREE: 
            return "tentativa de liberar valor já liberado";
        case OWN_ERR_BORROW_WHILE_MUT: 
            return "borrow imutável enquanto borrow mutável ativo";
        case OWN_ERR_MUT_BORROW_WHILE_BORROW: 
            return "borrow mutável enquanto borrow imutável ativo";
        case OWN_ERR_OUTLIVES: 
            return "referência vive mais que o dado referenciado";
        case OWN_ERR_DANGLING_REF: 
            return "referência para dado inválido";
        case OWN_ERR_MOVE_WHILE_BORROWED: 
            return "move de valor enquanto está emprestado";
        case OWN_ERR_MODIFY_WHILE_BORROWED: 
            return "modificação de valor enquanto está emprestado";
        default: return "erro de ownership desconhecido";
    }
}

void borrow_checker_print_state(BorrowChecker* bc) {
    printf("\n=== Borrow Checker State ===\n");
    printf("  Escopo: %u, Linha: %u\n", bc->current_scope, bc->current_line);
    printf("  Variáveis: %zu\n", bc->var_count);
    
    for (size_t i = 0; i < bc->var_count; i++) {
        const char* state_str = "???";
        switch (bc->variables[i].ownership.state) {
            case OWNER_OWNED: state_str = "owned"; break;
            case OWNER_BORROWED: state_str = "borrowed"; break;
            case OWNER_BORROWED_MUT: state_str = "borrowed_mut"; break;
            case OWNER_MOVED: state_str = "MOVED"; break;
            case OWNER_DROPPED: state_str = "DROPPED"; break;
        }
        
        printf("    [%zu] %s: %s", i, bc->variables[i].name, state_str);
        
        if (bc->variables[i].ownership.borrow_count > 0) {
            printf(" (&x%u)", bc->variables[i].ownership.borrow_count);
        }
        if (bc->variables[i].ownership.is_mut_borrowed) {
            printf(" (&mut)");
        }
        printf("\n");
    }
    
    if (bc->error_count > 0) {
        printf("\n  Erros (%zu):\n", bc->error_count);
        for (size_t i = 0; i < bc->error_count; i++) {
            printf("    %s\n", bc->errors[i].message);
        }
    }
    
    printf("============================\n\n");
}

/* =============================================================================
 * BOX<T>
 * ============================================================================= */

Box box_new(size_t size) {
    Box b;
    b.ptr = malloc(size);
    b.size = size;
    b.drop = NULL;
    
    if (b.ptr) {
        memset(b.ptr, 0, size);
    }
    
    return b;
}

void box_drop(Box* box) {
    if (box == NULL) return;
    
    if (box->drop && box->ptr) {
        box->drop(box->ptr);
    }
    
    free(box->ptr);
    box->ptr = NULL;
    box->size = 0;
}

void* box_deref(Box* box) {
    return box ? box->ptr : NULL;
}

void* box_deref_mut(Box* box) {
    return box ? box->ptr : NULL;
}

/* =============================================================================
 * SLICE
 * ============================================================================= */

Slice slice_from_array(void* arr, size_t len, size_t elem_size) {
    return (Slice){
        .ptr = arr,
        .len = len,
        .elem_size = elem_size
    };
}

void* slice_get(Slice* s, size_t index) {
    if (s == NULL || index >= s->len) return NULL;
    return (char*)s->ptr + (index * s->elem_size);
}

Slice slice_sub(Slice* s, size_t start, size_t end) {
    if (s == NULL || start >= s->len) {
        return (Slice){NULL, 0, 0};
    }
    
    if (end > s->len) end = s->len;
    if (start > end) start = end;
    
    return (Slice){
        .ptr = (char*)s->ptr + (start * s->elem_size),
        .len = end - start,
        .elem_size = s->elem_size
    };
}

