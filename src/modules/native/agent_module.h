#ifndef ASTERON_AGENT_MODULE_H
#define ASTERON_AGENT_MODULE_H

#include "../../core/vm/vm.h"
#include <stdint.h>
#include <stddef.h>

/* =============================================================================
 * TIPOS E ESTRUTURAS FUNDAMENTAIS
 * ============================================================================= */

/* Contexto como objeto de primeira classe */
typedef struct AgentContext {
    /* Memória do agente */
    void* memory;              /* Heap local do agente */
    size_t memory_size;
    size_t memory_capacity;
    
    /* Estado simbólico */
    void* symbol_table;        /* Tabela de símbolos local */
    void* beliefs;             /* Crenças/estado mental */
    void* goals;               /* Objetivos ativos */
    
    /* Eventos recentes */
    void* last_events;         /* Histórico de eventos */
    size_t event_count;
    
    /* Referências a recursos externos */
    void* references;          /* Referências a objetos compartilhados */
    
    /* Metadados */
    uint64_t version;          /* Versão do contexto (para snapshot) */
    uint64_t timestamp;        /* Última atualização */
    char* agent_id;            /* ID do agente dono */
    
    /* Ownership */
    int is_owned;              /* Contexto é owned ou borrowed? */
    int ref_count;             /* Contagem de referências */
} AgentContext;

/* Estratégias de importação de contexto */
typedef enum {
    CONTEXT_MERGE,             /* Mescla com contexto existente */
    CONTEXT_OVERLAY,           /* Sobrescreve valores existentes */
    CONTEXT_SHADOW,            /* Cria cópia isolada */
    CONTEXT_REFERENCE          /* Referência read-only */
} ContextImportStrategy;

/* Permissões de agente */
typedef struct AgentPermissions {
    int can_read_shared;       /* Pode ler memória compartilhada */
    int can_write_shared;       /* Pode escrever em memória compartilhada */
    int can_publish;            /* Pode publicar eventos */
    int can_subscribe;          /* Pode se inscrever em eventos */
    int can_spawn;              /* Pode criar novos agentes */
    
    /* Quotas */
    size_t max_memory;          /* Limite de memória */
    size_t max_events;          /* Limite de eventos por segundo */
    size_t max_context_size;    /* Limite de tamanho de contexto */
} AgentPermissions;

/* Agente individual */
typedef struct Agent {
    char* id;                  /* ID único do agente */
    char* name;                /* Nome legível */
    
    /* Ambiente isolado */
    void* environment;          /* Environment isolado */
    AgentContext* context;      /* Contexto do agente */
    AgentPermissions* permissions;
    
    /* Estado de execução */
    int is_active;              /* Agente está ativo? */
    int is_paused;              /* Agente está pausado? */
    
    /* Callbacks para LLM (serão implementados) */
    void* llm_handler;          /* Handler para LLM (OpenAI, Gemini, etc) */
    void* llm_config;           /* Configuração do LLM */
    
    /* Estatísticas */
    uint64_t message_count;     /* Mensagens enviadas */
    uint64_t context_exports;   /* Contextos exportados */
    uint64_t context_imports;    /* Contextos importados */
} Agent;

/* Message Bus (pub/sub) */
typedef struct Message {
    char* topic;               /* Tópico do evento */
    char* sender_id;            /* ID do agente remetente */
    void* payload;              /* Dados da mensagem */
    size_t payload_size;
    uint64_t timestamp;
    uint64_t sequence;         /* Sequência única */
} Message;

typedef struct MessageBus {
    Message* messages;         /* Fila de mensagens */
    size_t message_count;
    size_t message_capacity;
    
    /* Subscriptions: topic -> lista de agent_ids */
    void* subscriptions;       /* Hash table: topic -> agent_id[] */
    
    /* Sequência global */
    uint64_t next_sequence;
} MessageBus;

/* Shared Space (memória compartilhada controlada) */
typedef struct SharedSymbol {
    char* key;                 /* Chave do símbolo */
    void* value;                /* Valor */
    size_t value_size;
    
    /* Ownership */
    char* owner_id;            /* ID do agente dono */
    int is_readonly;           /* Símbolo é read-only? */
    int ref_count;             /* Referências ativas */
    
    /* Versionamento */
    uint64_t version;
    uint64_t last_modified;
} SharedSymbol;

typedef struct SharedSpace {
    SharedSymbol* symbols;     /* Símbolos compartilhados */
    size_t symbol_count;
    size_t symbol_capacity;
    
    /* Blackboard (dados temporários) */
    void* blackboard;          /* Área de escrita temporária */
    
    /* Resolução de conflitos */
    void* conflict_resolver;   /* Função de resolução de conflitos */
    
    /* Locks e sincronização */
    void* locks;                /* Locks por símbolo */
} SharedSpace;

/* Environment (ambiente de execução) */
typedef struct Environment {
    char* id;                  /* ID único do ambiente */
    char* name;                /* Nome do ambiente */
    
    /* Agentes no ambiente */
    Agent* agents;             /* Lista de agentes */
    size_t agent_count;
    size_t agent_capacity;
    
    /* Espaço compartilhado */
    SharedSpace* shared_space;
    
    /* Message Bus */
    MessageBus* message_bus;
    
    /* Heap global (opcional) */
    void* global_heap;
    size_t global_heap_size;
    
    /* Configuração */
    int max_agents;             /* Máximo de agentes */
    int enable_isolation;       /* Isolamento estrito entre agentes */
} Environment;

/* =============================================================================
 * FUNÇÕES PÚBLICAS
 * ============================================================================= */

/* Environment */
Environment* agent_env_create(const char* name);
void agent_env_destroy(Environment* env);
int agent_env_spawn_agent(Environment* env, const char* agent_name, AgentPermissions* perms);
Agent* agent_env_get_agent(Environment* env, const char* agent_id);
int agent_env_remove_agent(Environment* env, const char* agent_id);

/* Context */
AgentContext* agent_context_create(const char* agent_id);
void agent_context_destroy(AgentContext* ctx);
AgentContext* agent_context_export(Agent* agent, const char** keys, size_t key_count);
int agent_context_import(Agent* agent, AgentContext* imported_ctx, ContextImportStrategy strategy);
int agent_context_merge(AgentContext* dest, AgentContext* src);
int agent_context_overlay(AgentContext* dest, AgentContext* src);
AgentContext* agent_context_shadow_copy(AgentContext* src);

/* Shared Space */
int agent_shared_set(SharedSpace* space, const char* key, void* value, size_t size, const char* owner_id);
void* agent_shared_get(SharedSpace* space, const char* key, size_t* out_size);
int agent_shared_remove(SharedSpace* space, const char* key, const char* requester_id);
int agent_shared_list(SharedSpace* space, char*** out_keys, size_t* out_count);

/* Message Bus */
int agent_bus_publish(MessageBus* bus, const char* topic, const char* sender_id, void* payload, size_t payload_size);
int agent_bus_subscribe(MessageBus* bus, const char* topic, const char* agent_id);
int agent_bus_unsubscribe(MessageBus* bus, const char* topic, const char* agent_id);
Message* agent_bus_receive(MessageBus* bus, const char* agent_id, const char* topic, int blocking);
int agent_bus_poll(MessageBus* bus, const char* agent_id, Message*** out_messages, size_t* out_count);

/* Agent */
Agent* agent_create(const char* id, const char* name, Environment* env);
void agent_destroy(Agent* agent);
int agent_pause(Agent* agent);
int agent_resume(Agent* agent);
int agent_send_message(Agent* from, Agent* to, const char* topic, void* payload, size_t payload_size);

/* =============================================================================
 * FUNÇÕES NATIVAS PARA ASTERON
 * ============================================================================= */

/* Environment */
AsteronValue agent_native_env_create(int argc, AsteronValue* args);
AsteronValue agent_native_env_spawn(int argc, AsteronValue* args);
AsteronValue agent_native_env_get_agent(int argc, AsteronValue* args);

/* Context */
AsteronValue agent_native_context_export(int argc, AsteronValue* args);
AsteronValue agent_native_context_import(int argc, AsteronValue* args);

/* Shared Space */
AsteronValue agent_native_shared_set(int argc, AsteronValue* args);
AsteronValue agent_native_shared_get(int argc, AsteronValue* args);
AsteronValue agent_native_shared_list(int argc, AsteronValue* args);

/* Message Bus */
AsteronValue agent_native_publish(int argc, AsteronValue* args);
AsteronValue agent_native_subscribe(int argc, AsteronValue* args);
AsteronValue agent_native_receive(int argc, AsteronValue* args);

/* Agent */
AsteronValue agent_native_agent_id(int argc, AsteronValue* args);
AsteronValue agent_native_agent_pause(int argc, AsteronValue* args);
AsteronValue agent_native_agent_resume(int argc, AsteronValue* args);

/* Registro de funções nativas */
void agent_module_register(void);

#endif /* ASTERON_AGENT_MODULE_H */

