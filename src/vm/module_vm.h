/**
 * =============================================================================
 * ASTERON MODULE-VM INTEGRATION
 * =============================================================================
 * 
 * Integração entre o sistema de módulos e a VM.
 * 
 * RESPONSABILIDADES:
 * 
 * 1. Mapear funções de módulo para bytecode
 * 2. SSA versioning para variáveis de módulo
 * 3. Liveness analysis para dependências
 * 4. Paralelização automática
 * 5. First-class function nodes no grafo
 * 
 * =============================================================================
 */

#ifndef ASTERON_MODULE_VM_H
#define ASTERON_MODULE_VM_H

#include "../modules/module_interface.h"
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================= */

typedef struct ModuleBinding ModuleBinding;
typedef struct ModuleFunctionNode ModuleFunctionNode;
typedef struct ModuleCallSite ModuleCallSite;
typedef struct Compiler Compiler;  /* Forward declaration */

/* =============================================================================
 * OPCODES PARA MÓDULOS
 * ============================================================================= */

typedef enum {
    /* Chamada de função de módulo */
    OP_MODULE_CALL = 0xE0,      /* module_id, func_id, argc */
    
    /* Acesso a variável de módulo */
    OP_MODULE_LOAD = 0xE1,      /* module_id, var_id */
    OP_MODULE_STORE = 0xE2,     /* module_id, var_id */
    
    /* Importação */
    OP_IMPORT = 0xE3,           /* module_name_idx */
    OP_IMPORT_ALL = 0xE4,       /* module_name_idx */
    OP_IMPORT_AS = 0xE5,        /* module_name_idx, alias_idx */
    
    /* Eventos reativos */
    OP_WHEN = 0xE6,             /* var_id, handler_addr */
    OP_EMIT = 0xE7,             /* var_id */
    OP_WATCH = 0xE8,            /* var_id */
    
    /* Async/await para módulos */
    OP_AWAIT_MODULE = 0xE9,     /* call_id */
    OP_YIELD_TO = 0xEA          /* scheduler hint */
} ModuleOpcode;

/* =============================================================================
 * BINDING DE MÓDULO NA VM
 * ============================================================================= */

struct ModuleBinding {
    Module* module;
    
    /* Índice de funções para acesso rápido */
    ModuleFunction** func_index;
    uint32_t func_count;
    
    /* Índice de variáveis */
    ModuleVariable** var_index;
    uint32_t var_count;
    
    /* Estatísticas por função */
    struct {
        uint32_t call_count;
        uint64_t total_time_ns;
        bool is_hot;
        void* jit_stub;          /* Stub JIT se compilado */
    }* func_stats;
    
    /* Para SSA */
    uint32_t* var_versions;      /* Versão atual de cada variável */
    
    /* Próximo binding */
    ModuleBinding* next;
};

/* =============================================================================
 * NÓ DE FUNÇÃO NO GRAFO DECLARATIVO
 * ============================================================================= */

/**
 * Representa uma função de módulo como first-class entity no grafo.
 */
struct ModuleFunctionNode {
    ModuleFunction* func;
    
    /* Dependências: variáveis que esta função lê */
    struct {
        ModuleVariable* var;
        uint32_t last_version;   /* Versão quando lida */
    }* read_deps;
    uint32_t read_count;
    
    /* Outputs: variáveis que esta função modifica */
    struct {
        ModuleVariable* var;
        uint32_t new_version;    /* Nova versão após escrita */
    }* write_deps;
    uint32_t write_count;
    
    /* Para scheduling */
    bool can_parallelize;        /* Pode executar em paralelo */
    uint32_t priority;           /* Prioridade de execução */
    
    /* Nó no grafo reativo */
    ReactiveNode* reactive_node;
    
    /* Lista */
    ModuleFunctionNode* next;
};

/* =============================================================================
 * CALL SITE
 * ============================================================================= */

struct ModuleCallSite {
    uint32_t bytecode_offset;    /* Onde está no bytecode */
    ModuleFunction* target;      /* Função alvo */
    
    /* Inline cache */
    void* cached_fn;             /* Função cacheada */
    uint32_t cache_hits;
    
    /* Para devirtualização */
    bool is_monomorphic;         /* Sempre chama mesma função */
    
    ModuleCallSite* next;
};

/* =============================================================================
 * API - BINDING
 * ============================================================================= */

/**
 * Cria binding de módulo na VM
 */
ModuleBinding* vm_bind_module(VM* vm, Module* mod);

/**
 * Remove binding
 */
void vm_unbind_module(VM* vm, ModuleBinding* binding);

/**
 * Obtém binding por nome de módulo
 */
ModuleBinding* vm_get_binding(VM* vm, const char* module_name);

/* =============================================================================
 * API - COMPILAÇÃO
 * ============================================================================= */

/**
 * Compila chamada de função de módulo para bytecode
 */
void compile_module_call(Compiler* compiler, const char* module_name,
                          const char* func_name, int argc);

/**
 * Compila acesso a variável de módulo
 */
void compile_module_load(Compiler* compiler, const char* module_name,
                          const char* var_name);

/**
 * Compila escrita em variável de módulo
 */
void compile_module_store(Compiler* compiler, const char* module_name,
                           const char* var_name);

/**
 * Compila bloco when
 */
void compile_when_block(Compiler* compiler, const char* module_name,
                         const char* var_name, uint32_t handler_addr);

/* =============================================================================
 * API - EXECUÇÃO
 * ============================================================================= */

/**
 * Executa chamada de função de módulo
 */
AsteronValue vm_execute_module_call(VM* vm, uint32_t module_id,
                                     uint32_t func_id, int argc,
                                     AsteronValue* args);

/**
 * Lê variável de módulo
 */
AsteronValue vm_read_module_var(VM* vm, uint32_t module_id, uint32_t var_id);

/**
 * Escreve variável de módulo (dispara reatividade)
 */
void vm_write_module_var(VM* vm, uint32_t module_id, uint32_t var_id,
                          AsteronValue value);

/* =============================================================================
 * API - SSA PARA MÓDULOS
 * ============================================================================= */

/**
 * Obtém versão atual de variável de módulo
 */
uint32_t vm_get_var_version(VM* vm, uint32_t module_id, uint32_t var_id);

/**
 * Incrementa versão de variável (após escrita)
 */
uint32_t vm_inc_var_version(VM* vm, uint32_t module_id, uint32_t var_id);

/**
 * Verifica se variável mudou desde última leitura
 */
bool vm_var_changed(VM* vm, uint32_t module_id, uint32_t var_id,
                     uint32_t last_version);

/* =============================================================================
 * API - PARALELIZAÇÃO
 * ============================================================================= */

/**
 * Analisa se duas chamadas podem executar em paralelo
 */
bool vm_can_parallelize(ModuleFunctionNode* fn1, ModuleFunctionNode* fn2);

/**
 * Agenda funções para execução paralela
 */
void vm_schedule_parallel(VM* vm, ModuleFunctionNode** funcs, uint32_t count);

/**
 * Detecta funções independentes em um bloco
 */
ModuleFunctionNode** vm_find_independent_calls(VM* vm, uint32_t start_ip,
                                                 uint32_t end_ip,
                                                 uint32_t* count);

/* =============================================================================
 * API - JIT
 * ============================================================================= */

/**
 * Compila função de módulo para código nativo
 */
void* vm_jit_module_function(VM* vm, ModuleFunction* func);

/**
 * Invalida JIT quando módulo é recarregado
 */
void vm_invalidate_module_jit(VM* vm, Module* mod);

/**
 * Inline de chamada de módulo
 */
bool vm_inline_module_call(VM* vm, ModuleCallSite* site);

/* =============================================================================
 * API - GRAFO DECLARATIVO
 * ============================================================================= */

/**
 * Adiciona função de módulo ao grafo
 */
ModuleFunctionNode* vm_add_function_to_graph(VM* vm, ModuleFunction* func);

/**
 * Analisa dependências da função
 */
void vm_analyze_function_deps(VM* vm, ModuleFunctionNode* node);

/**
 * Propaga mudança quando variável de módulo muda
 */
void vm_propagate_module_change(VM* vm, ModuleVariable* var);

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_MODULE_VM_H */

