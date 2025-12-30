/**
 * =============================================================================
 * ASTERON MODULE INTERFACE v2.0
 * =============================================================================
 * 
 * Interface unificada para todos os módulos Asteron.
 * 
 * ARQUITETURA:
 * 
 *   ┌─────────────────────────────────────────────────────────────────┐
 *   │                        VM / Runtime                             │
 *   ├─────────────────────────────────────────────────────────────────┤
 *   │                     Module Manager                              │
 *   │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │
 *   │  │   net    │ │    fs    │ │   time   │ │    os    │  ...      │
 *   │  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘           │
 *   │       │            │            │            │                  │
 *   │  ┌────▼────────────▼────────────▼────────────▼─────┐           │
 *   │  │              Reactive State Graph               │           │
 *   │  │  (variáveis de módulo são nós reativos)         │           │
 *   │  └─────────────────────────────────────────────────┘           │
 *   └─────────────────────────────────────────────────────────────────┘
 * 
 * CARACTERÍSTICAS:
 * 
 * 1. AUTO-REGISTRO
 *    - Módulos se registram automaticamente na inicialização
 *    - Funções são mapeadas para bytecode
 * 
 * 2. FIRST-CLASS ENTITIES
 *    - Cada função é um nó no grafo declarativo
 *    - Dependências são rastreadas automaticamente
 * 
 * 3. ESTADO REATIVO
 *    - Variáveis de módulo podem ser reativas
 *    - when socket.received { ... }
 * 
 * 4. HOT RELOAD
 *    - Módulos podem ser recarregados em runtime
 *    - JIT invalida código afetado
 * 
 * =============================================================================
 */

#ifndef ASTERON_MODULE_INTERFACE_H
#define ASTERON_MODULE_INTERFACE_H

#include "../core/abi.h"
#include "../reactive/reactive.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================= */

typedef struct Module Module;
typedef struct ModuleManager ModuleManager;
typedef struct ModuleFunction ModuleFunction;
typedef struct ModuleVariable ModuleVariable;
typedef struct ModuleType ModuleType;

/* =============================================================================
 * CALLBACKS DE MÓDULO
 * ============================================================================= */

/**
 * Callback de inicialização do módulo
 */
typedef AsteronResult (*ModuleInitFn)(Module* mod);

/**
 * Callback de cleanup do módulo
 */
typedef void (*ModuleCleanupFn)(Module* mod);

/**
 * Callback chamado quando módulo é recarregado (hot reload)
 */
typedef AsteronResult (*ModuleReloadFn)(Module* mod, Module* old_mod);

/**
 * Callback para verificar se há eventos pendentes
 */
typedef bool (*ModulePollFn)(Module* mod);

/**
 * Callback para processar eventos pendentes
 */
typedef void (*ModuleProcessEventsFn)(Module* mod);

/* =============================================================================
 * ESTADO DO MÓDULO
 * ============================================================================= */

typedef enum {
    MODULE_STATE_UNLOADED,      /* Não carregado */
    MODULE_STATE_LOADING,       /* Carregando */
    MODULE_STATE_READY,         /* Pronto para uso */
    MODULE_STATE_ERROR,         /* Erro no carregamento */
    MODULE_STATE_RELOADING,     /* Sendo recarregado */
    MODULE_STATE_UNLOADING      /* Sendo descarregado */
} ModuleState;

/* =============================================================================
 * FLAGS DE FUNÇÃO
 * ============================================================================= */

typedef enum {
    FUNC_FLAG_NONE          = 0,
    FUNC_FLAG_PURE          = 1 << 0,   /* Função pura (sem side effects) */
    FUNC_FLAG_ASYNC         = 1 << 1,   /* Pode bloquear/async */
    FUNC_FLAG_REACTIVE      = 1 << 2,   /* Dispara em mudança de estado */
    FUNC_FLAG_EXPENSIVE     = 1 << 3,   /* Custo alto (evitar JIT inline) */
    FUNC_FLAG_VARIADIC      = 1 << 4,   /* Número variável de args */
    FUNC_FLAG_DEPRECATED    = 1 << 5,   /* Deprecada */
    FUNC_FLAG_UNSAFE        = 1 << 6,   /* Requer contexto unsafe */
    FUNC_FLAG_HOT           = 1 << 7    /* Frequentemente chamada */
} FunctionFlags;

/* =============================================================================
 * FUNÇÃO DE MÓDULO (FIRST-CLASS)
 * ============================================================================= */

struct ModuleFunction {
    /* Identificação */
    const char* name;
    uint32_t id;                    /* ID único no módulo */
    
    /* Implementação */
    AsteronNativeFn native_fn;      /* Ponteiro para função C */
    void* jit_code;                 /* Código JIT compilado (se houver) */
    
    /* Metadados */
    const char* signature;          /* Ex: "(string, number) -> bool" */
    const char* doc;                /* Documentação */
    int min_args;
    int max_args;                   /* -1 = variádico */
    FunctionFlags flags;
    
    /* Integração com grafo reativo */
    ReactiveNode* graph_node;       /* Nó no grafo declarativo */
    ModuleVariable** deps;          /* Variáveis que esta função lê */
    uint32_t dep_count;
    ModuleVariable** outputs;       /* Variáveis que esta função modifica */
    uint32_t output_count;
    
    /* Estatísticas para JIT */
    uint64_t call_count;
    uint64_t total_time_ns;
    bool is_hot;
    
    /* Linking */
    Module* owner;
    ModuleFunction* next;           /* Lista encadeada */
};

/* =============================================================================
 * VARIÁVEL DE MÓDULO (REATIVA)
 * ============================================================================= */

typedef enum {
    VAR_FLAG_NONE           = 0,
    VAR_FLAG_READONLY       = 1 << 0,   /* Somente leitura */
    VAR_FLAG_REACTIVE       = 1 << 1,   /* Dispara eventos em mudança */
    VAR_FLAG_PERSISTENT     = 1 << 2,   /* Persiste entre reloads */
    VAR_FLAG_EXPORTED       = 1 << 3,   /* Exportada para outros módulos */
    VAR_FLAG_EVENT_SOURCE   = 1 << 4    /* Fonte de eventos (socket, etc) */
} VariableFlags;

struct ModuleVariable {
    /* Identificação */
    const char* name;
    uint32_t id;
    
    /* Valor */
    AsteronValue value;
    AsteronValue prev_value;        /* Para detecção de mudança */
    
    /* Tipo */
    const char* type_name;          /* Ex: "Socket", "File", "number" */
    
    /* Flags */
    VariableFlags flags;
    
    /* Integração reativa */
    ReactiveNode* reactive_node;    /* Nó no grafo reativo */
    
    /* Linking */
    Module* owner;
    ModuleVariable* next;
};

/* =============================================================================
 * TIPO EXPORTADO POR MÓDULO
 * ============================================================================= */

struct ModuleType {
    const char* name;               /* Ex: "Socket", "File" */
    uint32_t id;
    size_t size;                    /* Tamanho em bytes */
    
    /* Métodos */
    ModuleFunction** methods;
    uint32_t method_count;
    
    /* Destrutor */
    void (*destructor)(void* obj);
    
    /* Linking */
    Module* owner;
    ModuleType* next;
};

/* =============================================================================
 * MÓDULO
 * ============================================================================= */

struct Module {
    /* Identificação */
    const char* name;               /* Ex: "net", "fs", "time" */
    const char* version;            /* Semver: "1.0.0" */
    const char* description;
    const char* author;
    uint32_t id;                    /* ID único */
    
    /* Estado */
    ModuleState state;
    const char* error_message;
    
    /* Funções exportadas */
    ModuleFunction* functions;      /* Lista encadeada */
    uint32_t function_count;
    
    /* Variáveis (estado do módulo) */
    ModuleVariable* variables;      /* Lista encadeada */
    uint32_t variable_count;
    
    /* Tipos exportados */
    ModuleType* types;
    uint32_t type_count;
    
    /* Dependências */
    Module** dependencies;
    uint32_t dependency_count;
    
    /* Callbacks */
    ModuleInitFn init;
    ModuleCleanupFn cleanup;
    ModuleReloadFn on_reload;
    ModulePollFn poll;
    ModuleProcessEventsFn process_events;
    
    /* Integração reativa */
    ReactiveRuntime* reactive_rt;   /* Runtime reativo (compartilhado) */
    ReactiveNode* module_node;      /* Nó raiz do módulo no grafo */
    
    /* Dados privados do módulo */
    void* private_data;
    
    /* Estatísticas */
    struct {
        uint64_t load_time_ns;
        uint64_t total_calls;
        uint64_t total_events;
        uint32_t reload_count;
    } stats;
    
    /* Linking */
    ModuleManager* manager;
    Module* next;
};

/* =============================================================================
 * GERENCIADOR DE MÓDULOS
 * ============================================================================= */

struct ModuleManager {
    /* Módulos carregados */
    Module* modules;                /* Lista encadeada */
    uint32_t module_count;
    uint32_t next_module_id;
    
    /* Índice por nome (hash table) */
    struct {
        const char* name;
        Module* module;
    }* name_index;
    uint32_t index_capacity;
    
    /* Runtime reativo compartilhado */
    ReactiveRuntime* reactive_rt;
    
    /* Caminhos de busca */
    const char** search_paths;
    uint32_t search_path_count;
    
    /* Callbacks globais */
    void (*on_module_load)(Module* mod);
    void (*on_module_unload)(Module* mod);
    void (*on_module_error)(Module* mod, const char* error);
    
    /* Event loop integration */
    bool running;
    uint32_t poll_interval_ms;
};

/* =============================================================================
 * API - GERENCIADOR DE MÓDULOS
 * ============================================================================= */

/**
 * Cria gerenciador de módulos
 */
ModuleManager* module_manager_create(void);

/**
 * Destrói gerenciador
 */
void module_manager_destroy(ModuleManager* mgr);

/**
 * Adiciona caminho de busca
 */
void module_manager_add_path(ModuleManager* mgr, const char* path);

/**
 * Carrega módulo por nome
 */
Module* module_manager_load(ModuleManager* mgr, const char* name);

/**
 * Descarrega módulo
 */
void module_manager_unload(ModuleManager* mgr, Module* mod);

/**
 * Recarrega módulo (hot reload)
 */
AsteronResult module_manager_reload(ModuleManager* mgr, Module* mod);

/**
 * Obtém módulo por nome
 */
Module* module_manager_get(ModuleManager* mgr, const char* name);

/**
 * Poll todos os módulos para eventos
 */
void module_manager_poll(ModuleManager* mgr);

/**
 * Executa event loop
 */
void module_manager_run(ModuleManager* mgr);

/**
 * Para event loop
 */
void module_manager_stop(ModuleManager* mgr);

/* =============================================================================
 * API - MÓDULO
 * ============================================================================= */

/**
 * Cria módulo vazio
 */
Module* module_create(const char* name, const char* version);

/**
 * Adiciona função ao módulo
 */
ModuleFunction* module_add_function(Module* mod, const char* name,
                                     AsteronNativeFn fn, int min_args, int max_args,
                                     const char* signature, FunctionFlags flags);

/**
 * Adiciona variável ao módulo
 */
ModuleVariable* module_add_variable(Module* mod, const char* name,
                                     AsteronValue initial, VariableFlags flags);

/**
 * Adiciona tipo ao módulo
 */
ModuleType* module_add_type(Module* mod, const char* name, size_t size);

/**
 * Adiciona dependência
 */
void module_add_dependency(Module* mod, Module* dep);

/**
 * Obtém função por nome
 */
ModuleFunction* module_get_function(Module* mod, const char* name);

/**
 * Obtém variável por nome
 */
ModuleVariable* module_get_variable(Module* mod, const char* name);

/**
 * Define valor de variável (dispara reatividade)
 */
void module_set_variable(Module* mod, const char* name, AsteronValue value);

/* =============================================================================
 * API - INTEGRAÇÃO COM VM
 * ============================================================================= */

/**
 * Registra todas as funções do módulo na VM
 */
AsteronResult module_register_in_vm(Module* mod, void* vm);

/**
 * Cria bytecode para chamar função de módulo
 */
void* module_compile_call(Module* mod, ModuleFunction* fn);

/**
 * Mapeia módulo para tabela de símbolos
 */
void module_add_to_symbols(Module* mod, void* symbol_table);

/* =============================================================================
 * API - INTEGRAÇÃO REATIVA
 * ============================================================================= */

/**
 * Conecta módulo ao runtime reativo
 */
void module_connect_reactive(Module* mod, ReactiveRuntime* rt);

/**
 * Cria efeito quando variável muda
 * 
 * when mod.variable { ... }
 */
ReactiveNode* module_when_variable(Module* mod, const char* var_name,
                                    ComputeFn effect);

/**
 * Marca variável como fonte de eventos
 */
void module_make_event_source(Module* mod, const char* var_name);

/* =============================================================================
 * MACROS PARA DEFINIÇÃO DE MÓDULOS
 * ============================================================================= */

/**
 * Inicia definição de módulo
 */
#define DEFINE_MODULE(mod_name, mod_version, mod_desc) \
    static Module* _create_##mod_name##_module(void) { \
        Module* mod = module_create(#mod_name, mod_version); \
        mod->description = mod_desc;

/**
 * Adiciona função ao módulo
 */
#define MODULE_FUNC(name, fn, min, max, sig, flags) \
        module_add_function(mod, name, fn, min, max, sig, flags);

/**
 * Adiciona variável reativa
 */
#define MODULE_VAR(name, initial, flags) \
        module_add_variable(mod, name, initial, flags);

/**
 * Adiciona tipo
 */
#define MODULE_TYPE(name, size) \
        module_add_type(mod, name, size);

/**
 * Finaliza definição
 */
#define END_MODULE(mod_name) \
        return mod; \
    } \
    __attribute__((constructor)) \
    static void _register_##mod_name##_module(void) { \
        Module* mod = _create_##mod_name##_module(); \
        module_manager_register_builtin(mod); \
    }

/**
 * Registra módulo built-in
 */
void module_manager_register_builtin(Module* mod);

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_MODULE_INTERFACE_H */

