#define _POSIX_C_SOURCE 200809L
#include "sandbox.h"
#include "../../utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * CRIAÇÃO E DESTRUIÇÃO
 * ============================================================================= */

SandboxManager* sandbox_manager_create(void) {
    SandboxManager* mgr = (SandboxManager*)calloc(1, sizeof(SandboxManager));
    if (mgr == NULL) return NULL;
    
    mgr->sandbox_capacity = 16;
    mgr->sandboxes = (SandboxContext**)calloc(
        mgr->sandbox_capacity, sizeof(SandboxContext*));
    if (mgr->sandboxes == NULL) {
        free(mgr);
        return NULL;
    }
    
    mgr->default_level = SANDBOX_ISOLATED;
    mgr->enforce_globally = 0;
    
    return mgr;
}

void sandbox_manager_destroy(SandboxManager* mgr) {
    if (mgr == NULL) return;
    
    for (size_t i = 0; i < mgr->sandbox_count; i++) {
        sandbox_destroy(mgr, mgr->sandboxes[i]);
    }
    free(mgr->sandboxes);
    free(mgr);
}

/* =============================================================================
 * SANDBOXES
 * ============================================================================= */

SandboxContext* sandbox_create(SandboxManager* mgr, const char* name, 
                               SandboxLevel level, VM* vm) {
    if (mgr == NULL || vm == NULL) return NULL;
    
    if (mgr->sandbox_count >= mgr->sandbox_capacity) {
        mgr->sandbox_capacity *= 2;
        SandboxContext** new_sandboxes = (SandboxContext**)realloc(
            mgr->sandboxes, sizeof(SandboxContext*) * mgr->sandbox_capacity);
        if (new_sandboxes == NULL) return NULL;
        mgr->sandboxes = new_sandboxes;
    }
    
    SandboxContext* ctx = (SandboxContext*)calloc(1, sizeof(SandboxContext));
    if (ctx == NULL) return NULL;
    
    ctx->level = level;
    ctx->name = name ? strdup(name) : strdup("unnamed");
    ctx->original_vm = vm;
    ctx->is_active = 0;
    
    // Configura permissões padrão baseado no nível
    memset(&ctx->perms, 0, sizeof(SandboxPermissions));
    switch (level) {
        case SANDBOX_NONE:
            ctx->perms.can_read_globals = 1;
            ctx->perms.can_write_globals = 1;
            ctx->perms.can_read_locals = 1;
            ctx->perms.can_write_locals = 1;
            ctx->perms.can_call_functions = 1;
            ctx->perms.can_do_io = 1;
            ctx->perms.can_do_network = 1;
            ctx->perms.can_allocate_memory = 1;
            break;
            
        case SANDBOX_READ_ONLY:
            ctx->perms.can_read_globals = 1;
            ctx->perms.can_read_locals = 1;
            ctx->perms.can_call_functions = 1;
            break;
            
        case SANDBOX_ISOLATED:
            ctx->perms.can_read_locals = 1;
            ctx->perms.can_write_locals = 1;
            ctx->perms.can_call_functions = 1;
            // Cria VM isolada
            // (Simplificado - em produção, criaria cópia completa)
            break;
            
        case SANDBOX_STRICT:
            // Nada permitido por padrão
            break;
    }
    
    ctx->perms.allowed_capacity = 8;
    ctx->perms.allowed_variables = (char**)calloc(
        ctx->perms.allowed_capacity, sizeof(char*));
    
    ctx->perms.allowed_func_capacity = 8;
    ctx->perms.allowed_functions = (char**)calloc(
        ctx->perms.allowed_func_capacity, sizeof(char*));
    
    ctx->tracking.log_capacity = 16;
    ctx->tracking.violation_log = (char**)calloc(
        ctx->tracking.log_capacity, sizeof(char*));
    
    mgr->sandboxes[mgr->sandbox_count++] = ctx;
    mgr->stats.total_sandboxes++;
    
    return ctx;
}

void sandbox_destroy(SandboxManager* mgr, SandboxContext* ctx) {
    if (mgr == NULL || ctx == NULL) return;
    
    // Remove da lista
    for (size_t i = 0; i < mgr->sandbox_count; i++) {
        if (mgr->sandboxes[i] == ctx) {
            mgr->sandboxes[i] = mgr->sandboxes[mgr->sandbox_count - 1];
            mgr->sandbox_count--;
            break;
        }
    }
    
    // Libera recursos
    if (ctx->isolated_vm) {
        vm_destroy(ctx->isolated_vm);
    }
    
    if (ctx->name) free((void*)ctx->name);
    
    if (ctx->perms.allowed_variables) {
        for (size_t i = 0; i < ctx->perms.allowed_count; i++) {
            free(ctx->perms.allowed_variables[i]);
        }
        free(ctx->perms.allowed_variables);
    }
    
    if (ctx->perms.allowed_functions) {
        for (size_t i = 0; i < ctx->perms.allowed_func_count; i++) {
            free(ctx->perms.allowed_functions[i]);
        }
        free(ctx->perms.allowed_functions);
    }
    
    if (ctx->tracking.violation_log) {
        for (size_t i = 0; i < ctx->tracking.log_count; i++) {
            free(ctx->tracking.violation_log[i]);
        }
        free(ctx->tracking.violation_log);
    }
    
    free(ctx);
}

int sandbox_enter(SandboxContext* ctx) {
    if (ctx == NULL || ctx->is_active) return 0;
    
    // Backup do estado original
    ctx->original_env = ctx->original_vm->env;
    
    // Cria estado isolado se necessário
    if (ctx->level == SANDBOX_ISOLATED || ctx->level == SANDBOX_STRICT) {
        // (Simplificado - em produção, criaria cópia completa do environment)
    }
    
    ctx->is_active = 1;
    return 1;
}

int sandbox_exit(SandboxContext* ctx) {
    if (ctx == NULL || !ctx->is_active) return 0;
    
    // Restaura estado original
    if (ctx->original_vm && ctx->original_env) {
        ctx->original_vm->env = ctx->original_env;
    }
    
    ctx->is_active = 0;
    return 1;
}

Value sandbox_execute(SandboxContext* ctx, ASTNode* code) {
    if (ctx == NULL || code == NULL || !ctx->is_active) {
        return value_nil();
    }
    
    // Executa código dentro do sandbox
    // (Simplificado - em produção, compilaria e executaria o AST)
    return value_nil();
}

/* =============================================================================
 * PERMISSÕES
 * ============================================================================= */

void sandbox_set_permissions(SandboxContext* ctx, SandboxPermissions* perms) {
    if (ctx == NULL || perms == NULL) return;
    ctx->perms = *perms;
}

void sandbox_allow_variable(SandboxContext* ctx, const char* var_name) {
    if (ctx == NULL || var_name == NULL) return;
    
    if (ctx->perms.allowed_count >= ctx->perms.allowed_capacity) {
        ctx->perms.allowed_capacity *= 2;
        char** new_vars = (char**)realloc(
            ctx->perms.allowed_variables, 
            sizeof(char*) * ctx->perms.allowed_capacity);
        if (new_vars == NULL) return;
        ctx->perms.allowed_variables = new_vars;
    }
    
    ctx->perms.allowed_variables[ctx->perms.allowed_count++] = strdup(var_name);
}

void sandbox_allow_function(SandboxContext* ctx, const char* func_name) {
    if (ctx == NULL || func_name == NULL) return;
    
    if (ctx->perms.allowed_func_count >= ctx->perms.allowed_func_capacity) {
        ctx->perms.allowed_func_capacity *= 2;
        char** new_funcs = (char**)realloc(
            ctx->perms.allowed_functions,
            sizeof(char*) * ctx->perms.allowed_func_capacity);
        if (new_funcs == NULL) return;
        ctx->perms.allowed_functions = new_funcs;
    }
    
    ctx->perms.allowed_functions[ctx->perms.allowed_func_count++] = strdup(func_name);
}

int sandbox_check_permission(SandboxContext* ctx, const char* operation, 
                             const char* resource) {
    if (ctx == NULL || !ctx->is_active) return 0;
    
    // Verifica permissões baseado na operação
    if (strcmp(operation, "read_global") == 0) {
        if (!ctx->perms.can_read_globals) {
            sandbox_log_violation(ctx, operation, resource);
            return 0;
        }
        // Verifica whitelist
        if (ctx->perms.allowed_count > 0) {
            int allowed = 0;
            for (size_t i = 0; i < ctx->perms.allowed_count; i++) {
                if (strcmp(ctx->perms.allowed_variables[i], resource) == 0) {
                    allowed = 1;
                    break;
                }
            }
            if (!allowed) {
                sandbox_log_violation(ctx, operation, resource);
                return 0;
            }
        }
        return 1;
    }
    
    // (Simplificado - em produção, verificaria todas as operações)
    return 1;
}

/* =============================================================================
 * RASTREAMENTO E SEGURANÇA
 * ============================================================================= */

void sandbox_log_violation(SandboxContext* ctx, const char* operation, 
                           const char* resource) {
    if (ctx == NULL) return;
    
    ctx->tracking.violations++;
    
    if (ctx->tracking.log_count >= ctx->tracking.log_capacity) {
        ctx->tracking.log_capacity *= 2;
        char** new_log = (char**)realloc(
            ctx->tracking.violation_log,
            sizeof(char*) * ctx->tracking.log_capacity);
        if (new_log == NULL) return;
        ctx->tracking.violation_log = new_log;
    }
    
    char* entry = (char*)malloc(256);
    if (entry != NULL) {
        snprintf(entry, 256, "%s: %s on %s", ctx->name, operation, resource);
        ctx->tracking.violation_log[ctx->tracking.log_count++] = entry;
    }
}

void sandbox_get_violations(SandboxContext* ctx, char*** violations, size_t* count) {
    if (ctx == NULL) return;
    
    if (violations) *violations = ctx->tracking.violation_log;
    if (count) *count = ctx->tracking.log_count;
}

void sandbox_clear_violations(SandboxContext* ctx) {
    if (ctx == NULL) return;
    
    for (size_t i = 0; i < ctx->tracking.log_count; i++) {
        free(ctx->tracking.violation_log[i]);
    }
    ctx->tracking.log_count = 0;
    ctx->tracking.violations = 0;
}

/* =============================================================================
 * INTEGRAÇÃO COM GRAFOS
 * ============================================================================= */

void sandbox_mark_node(UnifiedNode* node, SandboxContext* ctx) {
    if (node == NULL || ctx == NULL) return;
    // Armazena referência no user_data
    node->user_data = ctx;
}

SandboxContext* sandbox_get_node_context(UnifiedNode* node) {
    if (node == NULL) return NULL;
    return (SandboxContext*)node->user_data;
}

