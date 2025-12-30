#ifndef SANDBOX_H
#define SANDBOX_H

#include "../vm/vm.h"
#include "../../graph/unified_graph.h"
#include <stddef.h>

/* =============================================================================
 * ISOLAMENTO DE EXECUÇÃO (SANDBOXING)
 * =============================================================================
 * Permite que funções ou blocos sejam executados em sandbox isolado,
 * prevenindo efeitos colaterais e garantindo segurança.
 * ============================================================================= */

// Nível de isolamento
typedef enum {
    SANDBOX_NONE,           // Sem isolamento
    SANDBOX_READ_ONLY,      // Apenas leitura (não pode modificar estado)
    SANDBOX_ISOLATED,        // Isolado (cópia do estado)
    SANDBOX_STRICT          // Estrito (sem acesso a recursos externos)
} SandboxLevel;

// Permissões do sandbox
typedef struct SandboxPermissions {
    int can_read_globals;       // Pode ler variáveis globais?
    int can_write_globals;      // Pode escrever variáveis globais?
    int can_read_locals;        // Pode ler variáveis locais?
    int can_write_locals;       // Pode escrever variáveis locais?
    int can_call_functions;     // Pode chamar funções?
    int can_do_io;              // Pode fazer I/O?
    int can_do_network;         // Pode fazer operações de rede?
    int can_allocate_memory;    // Pode alocar memória?
    
    // Lista de variáveis permitidas (whitelist)
    char** allowed_variables;
    size_t allowed_count;
    size_t allowed_capacity;
    
    // Lista de funções permitidas (whitelist)
    char** allowed_functions;
    size_t allowed_func_count;
    size_t allowed_func_capacity;
} SandboxPermissions;

// Contexto de sandbox
typedef struct SandboxContext {
    SandboxLevel level;         // Nível de isolamento
    SandboxPermissions perms;   // Permissões
    
    // Estado isolado
    VM* isolated_vm;            // VM isolada (se necessário)
    Environment* isolated_env;   // Environment isolado
    
    // Estado original (backup)
    VM* original_vm;            // VM original
    Environment* original_env;   // Environment original
    
    // Rastreamento
    struct {
        int violations;          // Número de violações detectadas
        char** violation_log;   // Log de violações
        size_t log_count;
        size_t log_capacity;
    } tracking;
    
    // Metadados
    const char* name;           // Nome do sandbox
    int is_active;              // Sandbox está ativo?
} SandboxContext;

// Gerenciador de sandboxes
typedef struct SandboxManager {
    SandboxContext** sandboxes; // Todos os sandboxes
    size_t sandbox_count;
    size_t sandbox_capacity;
    
    SandboxContext* current;    // Sandbox atual (se houver)
    
    // Configuração global
    int enforce_globally;       // Aplica sandboxing globalmente?
    SandboxLevel default_level; // Nível padrão
    
    // Estatísticas
    struct {
        size_t total_sandboxes;
        size_t total_violations;
        size_t total_blocks;
    } stats;
} SandboxManager;

/* =============================================================================
 * API - CRIAÇÃO E DESTRUIÇÃO
 * ============================================================================= */

/**
 * Cria gerenciador de sandboxes
 */
SandboxManager* sandbox_manager_create(void);

/**
 * Destrói gerenciador de sandboxes
 */
void sandbox_manager_destroy(SandboxManager* mgr);

/* =============================================================================
 * API - SANDBOXES
 * ============================================================================= */

/**
 * Cria contexto de sandbox
 */
SandboxContext* sandbox_create(SandboxManager* mgr, const char* name, 
                               SandboxLevel level, VM* vm);

/**
 * Destrói sandbox
 */
void sandbox_destroy(SandboxManager* mgr, SandboxContext* ctx);

/**
 * Ativa sandbox (entra no contexto isolado)
 */
int sandbox_enter(SandboxContext* ctx);

/**
 * Desativa sandbox (sai do contexto isolado)
 */
int sandbox_exit(SandboxContext* ctx);

/**
 * Executa código dentro de sandbox
 */
Value sandbox_execute(SandboxContext* ctx, ASTNode* code);

/* =============================================================================
 * API - PERMISSÕES
 * ============================================================================= */

/**
 * Configura permissões do sandbox
 */
void sandbox_set_permissions(SandboxContext* ctx, SandboxPermissions* perms);

/**
 * Adiciona variável à whitelist
 */
void sandbox_allow_variable(SandboxContext* ctx, const char* var_name);

/**
 * Adiciona função à whitelist
 */
void sandbox_allow_function(SandboxContext* ctx, const char* func_name);

/**
 * Verifica se operação é permitida
 */
int sandbox_check_permission(SandboxContext* ctx, const char* operation, 
                             const char* resource);

/* =============================================================================
 * API - RASTREAMENTO E SEGURANÇA
 * ============================================================================= */

/**
 * Registra violação de segurança
 */
void sandbox_log_violation(SandboxContext* ctx, const char* operation, 
                           const char* resource);

/**
 * Obtém log de violações
 */
void sandbox_get_violations(SandboxContext* ctx, char*** violations, size_t* count);

/**
 * Limpa log de violações
 */
void sandbox_clear_violations(SandboxContext* ctx);

/* =============================================================================
 * API - INTEGRAÇÃO COM GRAFOS
 * ============================================================================= */

/**
 * Marca nó do grafo como sandboxed
 */
void sandbox_mark_node(UnifiedNode* node, SandboxContext* ctx);

/**
 * Verifica se nó está em sandbox
 */
SandboxContext* sandbox_get_node_context(UnifiedNode* node);

#endif // SANDBOX_H

