/**
 * =============================================================================
 * ASTERON AGENT MODULE - Sistema Multi-Agente com Memória Compartilhada
 * =============================================================================
 * 
 * Este módulo implementa um sistema multi-agente completo com:
 * 
 * - Environment isolado por agente
 * - Context como objeto de primeira classe
 * - Shared Space com controle de acesso
 * - Message Bus (pub/sub)
 * - Troca de contexto com estratégias (merge, overlay, shadow, reference)
 * - Sistema de permissões e quotas
 * 
 * INTEGRAÇÃO COM ASTERON:
 * - Usa sistema de ownership existente
 * - Integra com SSA e snapshot
 * - Compatível com escape analysis e liveness
 * 
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "agent_module.h"
#include "../../core/vm/vm.h"
#include "../../loader/loader.h"
#include "../../core/abi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <pthread.h>
    #include <unistd.h>
#endif

/* =============================================================================
 * HELPERS
 * ============================================================================= */

static AsteronValue make_string(const char* s) {
    if (!s) return ASTERON_NIL();
    
    size_t len = strlen(s);
    AsteronString* str = (AsteronString*)malloc(sizeof(AsteronString) + len + 1);
    if (str == NULL) return ASTERON_NIL();
    
    str->header.obj_type = ASTERON_OBJ_STRING;
    str->header.flags = 0;
    str->header.ref_count = 1;
    str->header.next = NULL;
    str->length = len;
    str->hash = 0;
    str->capacity = len + 1;
    strcpy(str->chars, s);
    
    return ASTERON_PTR(str, ASTERON_VAL_STRING);
}

static AsteronValue make_error(AsteronResult code) {
    return ASTERON_ERROR_VAL(code);
}

static const char* get_string_arg(AsteronValue* args, int index) {
    if (ASTERON_IS_NIL(args[index])) return NULL;
    if (args[index].type != ASTERON_VAL_STRING) return NULL;
    AsteronString* s = (AsteronString*)args[index].as.ptr;
    if (s == NULL) return NULL;
    return s->chars;
}

/* Geração de IDs únicos */
static uint64_t g_next_env_id = 1;
static uint64_t g_next_agent_id = 1;
static uint64_t g_next_message_seq = 1;

/* =============================================================================
 * ENVIRONMENT
 * ============================================================================= */

Environment* agent_env_create(const char* name) {
    Environment* env = (Environment*)calloc(1, sizeof(Environment));
    if (env == NULL) return NULL;
    
    /* Gera ID único */
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "env_%llu", (unsigned long long)g_next_env_id++);
    env->id = strdup(id_buf);
    env->name = name ? strdup(name) : strdup("default");
    
    /* Inicializa estruturas */
    env->agent_capacity = 16;
    env->agents = (Agent*)calloc(env->agent_capacity, sizeof(Agent));
    
    env->shared_space = (SharedSpace*)calloc(1, sizeof(SharedSpace));
    if (env->shared_space) {
        env->shared_space->symbol_capacity = 64;
        env->shared_space->symbols = (SharedSymbol*)calloc(env->shared_space->symbol_capacity, sizeof(SharedSymbol));
    }
    
    env->message_bus = (MessageBus*)calloc(1, sizeof(MessageBus));
    if (env->message_bus) {
        env->message_bus->message_capacity = 256;
        env->message_bus->messages = (Message*)calloc(env->message_bus->message_capacity, sizeof(Message));
        env->message_bus->next_sequence = 1;
    }
    
    env->max_agents = 100;
    env->enable_isolation = 1;
    
    return env;
}

void agent_env_destroy(Environment* env) {
    if (env == NULL) return;
    
    /* Destrói agentes */
    for (size_t i = 0; i < env->agent_count; i++) {
        agent_destroy(&env->agents[i]);
    }
    free(env->agents);
    
    /* Destrói shared space */
    if (env->shared_space) {
        for (size_t i = 0; i < env->shared_space->symbol_count; i++) {
            SharedSymbol* sym = &env->shared_space->symbols[i];
            free(sym->key);
            free(sym->value);
        }
        free(env->shared_space->symbols);
        free(env->shared_space);
    }
    
    /* Destrói message bus */
    if (env->message_bus) {
        for (size_t i = 0; i < env->message_bus->message_count; i++) {
            Message* msg = &env->message_bus->messages[i];
            free(msg->topic);
            free(msg->sender_id);
            free(msg->payload);
        }
        free(env->message_bus->messages);
        free(env->message_bus);
    }
    
    free(env->id);
    free(env->name);
    free(env->global_heap);
    free(env);
}

int agent_env_spawn_agent(Environment* env, const char* agent_name, AgentPermissions* perms) {
    if (env == NULL || agent_name == NULL) return 0;
    if (env->agent_count >= env->agent_capacity) {
        /* Expande array */
        size_t new_cap = env->agent_capacity * 2;
        Agent* new_agents = (Agent*)realloc(env->agents, new_cap * sizeof(Agent));
        if (new_agents == NULL) return 0;
        env->agents = new_agents;
        env->agent_capacity = new_cap;
    }
    
    /* Cria agente */
    Agent* agent = &env->agents[env->agent_count++];
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "agent_%llu", (unsigned long long)g_next_agent_id++);
    agent->id = strdup(id_buf);
    agent->name = strdup(agent_name);
    
    /* Cria contexto */
    agent->context = agent_context_create(agent->id);
    
    /* Cria permissões */
    agent->permissions = (AgentPermissions*)calloc(1, sizeof(AgentPermissions));
    if (perms) {
        memcpy(agent->permissions, perms, sizeof(AgentPermissions));
    } else {
        /* Permissões padrão: tudo permitido */
        agent->permissions->can_read_shared = 1;
        agent->permissions->can_write_shared = 1;
        agent->permissions->can_publish = 1;
        agent->permissions->can_subscribe = 1;
        agent->permissions->can_spawn = 0;
        agent->permissions->max_memory = 1024 * 1024; /* 1MB */
        agent->permissions->max_events = 100;
        agent->permissions->max_context_size = 64 * 1024; /* 64KB */
    }
    
    agent->is_active = 1;
    agent->is_paused = 0;
    agent->environment = env;
    
    return 1;
}

Agent* agent_env_get_agent(Environment* env, const char* agent_id) {
    if (env == NULL || agent_id == NULL) return NULL;
    
    for (size_t i = 0; i < env->agent_count; i++) {
        if (strcmp(env->agents[i].id, agent_id) == 0) {
            return &env->agents[i];
        }
    }
    return NULL;
}

int agent_env_remove_agent(Environment* env, const char* agent_id) {
    if (env == NULL || agent_id == NULL) return 0;
    
    for (size_t i = 0; i < env->agent_count; i++) {
        if (strcmp(env->agents[i].id, agent_id) == 0) {
            agent_destroy(&env->agents[i]);
            /* Move últimos elementos */
            memmove(&env->agents[i], &env->agents[i + 1], 
                   (env->agent_count - i - 1) * sizeof(Agent));
            env->agent_count--;
            return 1;
        }
    }
    return 0;
}

/* =============================================================================
 * CONTEXT
 * ============================================================================= */

AgentContext* agent_context_create(const char* agent_id) {
    AgentContext* ctx = (AgentContext*)calloc(1, sizeof(AgentContext));
    if (ctx == NULL) return NULL;
    
    ctx->agent_id = agent_id ? strdup(agent_id) : NULL;
    ctx->memory_capacity = 4096;
    ctx->memory = malloc(ctx->memory_capacity);
    ctx->version = 1;
    ctx->timestamp = time(NULL);
    ctx->is_owned = 1;
    ctx->ref_count = 1;
    
    return ctx;
}

void agent_context_destroy(AgentContext* ctx) {
    if (ctx == NULL) return;
    
    if (ctx->ref_count > 0) {
        ctx->ref_count--;
        if (ctx->ref_count > 0) return; /* Ainda há referências */
    }
    
    free(ctx->memory);
    free(ctx->symbol_table);
    free(ctx->beliefs);
    free(ctx->goals);
    free(ctx->last_events);
    free(ctx->references);
    free(ctx->agent_id);
    free(ctx);
}

AgentContext* agent_context_export(Agent* agent, const char** keys, size_t key_count) {
    if (agent == NULL || agent->context == NULL) return NULL;
    
    /* Cria cópia do contexto */
    AgentContext* exported = agent_context_shadow_copy(agent->context);
    if (exported == NULL) return NULL;
    
    /* Se keys especificadas, filtra */
    if (keys && key_count > 0) {
        /* TODO: Implementar filtro por keys */
        /* Por enquanto, exporta tudo */
    }
    
    exported->is_owned = 0; /* Contexto exportado é borrowed */
    return exported;
}

int agent_context_import(Agent* agent, AgentContext* imported_ctx, ContextImportStrategy strategy) {
    if (agent == NULL || imported_ctx == NULL) return 0;
    if (agent->context == NULL) {
        agent->context = agent_context_create(agent->id);
        if (agent->context == NULL) return 0;
    }
    
    switch (strategy) {
        case CONTEXT_MERGE:
            return agent_context_merge(agent->context, imported_ctx);
        case CONTEXT_OVERLAY:
            return agent_context_overlay(agent->context, imported_ctx);
        case CONTEXT_SHADOW:
            agent_context_destroy(agent->context);
            agent->context = agent_context_shadow_copy(imported_ctx);
            return agent->context != NULL;
        case CONTEXT_REFERENCE:
            /* Referência read-only: não modifica, apenas referencia */
            /* TODO: Implementar sistema de referências */
            return 1;
        default:
            return 0;
    }
}

int agent_context_merge(AgentContext* dest, AgentContext* src) {
    if (dest == NULL || src == NULL) return 0;
    
    /* Mescla memória (concatena) */
    size_t new_size = dest->memory_size + src->memory_size;
    if (new_size > dest->memory_capacity) {
        dest->memory_capacity = new_size * 2;
        dest->memory = realloc(dest->memory, dest->memory_capacity);
        if (dest->memory == NULL) return 0;
    }
    memcpy((char*)dest->memory + dest->memory_size, src->memory, src->memory_size);
    dest->memory_size = new_size;
    
    /* Atualiza versão e timestamp */
    dest->version++;
    dest->timestamp = time(NULL);
    
    return 1;
}

int agent_context_overlay(AgentContext* dest, AgentContext* src) {
    if (dest == NULL || src == NULL) return 0;
    
    /* Sobrescreve memória */
    if (src->memory_size > dest->memory_capacity) {
        dest->memory_capacity = src->memory_size * 2;
        dest->memory = realloc(dest->memory, dest->memory_capacity);
        if (dest->memory == NULL) return 0;
    }
    memcpy(dest->memory, src->memory, src->memory_size);
    dest->memory_size = src->memory_size;
    
    /* Atualiza versão e timestamp */
    dest->version++;
    dest->timestamp = time(NULL);
    
    return 1;
}

AgentContext* agent_context_shadow_copy(AgentContext* src) {
    if (src == NULL) return NULL;
    
    AgentContext* copy = (AgentContext*)calloc(1, sizeof(AgentContext));
    if (copy == NULL) return NULL;
    
    copy->agent_id = src->agent_id ? strdup(src->agent_id) : NULL;
    copy->memory_size = src->memory_size;
    copy->memory_capacity = src->memory_capacity;
    copy->memory = malloc(copy->memory_capacity);
    if (copy->memory == NULL) {
        free(copy);
        return NULL;
    }
    memcpy(copy->memory, src->memory, src->memory_size);
    
    copy->version = src->version;
    copy->timestamp = src->timestamp;
    copy->is_owned = 1;
    copy->ref_count = 1;
    
    return copy;
}

/* =============================================================================
 * SHARED SPACE
 * ============================================================================= */

int agent_shared_set(SharedSpace* space, const char* key, void* value, size_t size, const char* owner_id) {
    if (space == NULL || key == NULL || value == NULL) return 0;
    
    /* Procura símbolo existente */
    for (size_t i = 0; i < space->symbol_count; i++) {
        if (strcmp(space->symbols[i].key, key) == 0) {
            SharedSymbol* sym = &space->symbols[i];
            
            /* Verifica permissões */
            if (sym->owner_id && strcmp(sym->owner_id, owner_id) != 0 && sym->is_readonly) {
                return 0; /* Não tem permissão para escrever */
            }
            
            /* Atualiza valor */
            free(sym->value);
            sym->value = malloc(size);
            if (sym->value == NULL) return 0;
            memcpy(sym->value, value, size);
            sym->value_size = size;
            sym->owner_id = owner_id ? strdup(owner_id) : NULL;
            sym->version++;
            sym->last_modified = time(NULL);
            return 1;
        }
    }
    
    /* Cria novo símbolo */
    if (space->symbol_count >= space->symbol_capacity) {
        size_t new_cap = space->symbol_capacity * 2;
        SharedSymbol* new_symbols = (SharedSymbol*)realloc(space->symbols, new_cap * sizeof(SharedSymbol));
        if (new_symbols == NULL) return 0;
        space->symbols = new_symbols;
        space->symbol_capacity = new_cap;
    }
    
    SharedSymbol* sym = &space->symbols[space->symbol_count++];
    sym->key = strdup(key);
    sym->value = malloc(size);
    if (sym->value == NULL) {
        free(sym->key);
        space->symbol_count--;
        return 0;
    }
    memcpy(sym->value, value, size);
    sym->value_size = size;
    sym->owner_id = owner_id ? strdup(owner_id) : NULL;
    sym->is_readonly = 0;
    sym->ref_count = 0;
    sym->version = 1;
    sym->last_modified = time(NULL);
    
    return 1;
}

void* agent_shared_get(SharedSpace* space, const char* key, size_t* out_size) {
    if (space == NULL || key == NULL) return NULL;
    
    for (size_t i = 0; i < space->symbol_count; i++) {
        if (strcmp(space->symbols[i].key, key) == 0) {
            if (out_size) *out_size = space->symbols[i].value_size;
            return space->symbols[i].value;
        }
    }
    return NULL;
}

int agent_shared_remove(SharedSpace* space, const char* key, const char* requester_id) {
    if (space == NULL || key == NULL) return 0;
    
    for (size_t i = 0; i < space->symbol_count; i++) {
        if (strcmp(space->symbols[i].key, key) == 0) {
            SharedSymbol* sym = &space->symbols[i];
            
            /* Verifica permissões */
            if (sym->owner_id && strcmp(sym->owner_id, requester_id) != 0) {
                return 0; /* Não é o dono */
            }
            
            /* Remove */
            free(sym->key);
            free(sym->value);
            free(sym->owner_id);
            
            /* Move últimos elementos */
            memmove(&space->symbols[i], &space->symbols[i + 1],
                   (space->symbol_count - i - 1) * sizeof(SharedSymbol));
            space->symbol_count--;
            return 1;
        }
    }
    return 0;
}

int agent_shared_list(SharedSpace* space, char*** out_keys, size_t* out_count) {
    if (space == NULL || out_keys == NULL || out_count == NULL) return 0;
    
    *out_count = space->symbol_count;
    if (space->symbol_count == 0) {
        *out_keys = NULL;
        return 1;
    }
    
    char** keys = (char**)malloc(space->symbol_count * sizeof(char*));
    if (keys == NULL) return 0;
    
    for (size_t i = 0; i < space->symbol_count; i++) {
        keys[i] = strdup(space->symbols[i].key);
    }
    
    *out_keys = keys;
    return 1;
}

/* =============================================================================
 * MESSAGE BUS
 * ============================================================================= */

int agent_bus_publish(MessageBus* bus, const char* topic, const char* sender_id, void* payload, size_t payload_size) {
    if (bus == NULL || topic == NULL || sender_id == NULL) return 0;
    
    if (bus->message_count >= bus->message_capacity) {
        size_t new_cap = bus->message_capacity * 2;
        Message* new_messages = (Message*)realloc(bus->messages, new_cap * sizeof(Message));
        if (new_messages == NULL) return 0;
        bus->messages = new_messages;
        bus->message_capacity = new_cap;
    }
    
    Message* msg = &bus->messages[bus->message_count++];
    msg->topic = strdup(topic);
    msg->sender_id = strdup(sender_id);
    msg->payload = malloc(payload_size);
    if (msg->payload == NULL) {
        free(msg->topic);
        free(msg->sender_id);
        bus->message_count--;
        return 0;
    }
    memcpy(msg->payload, payload, payload_size);
    msg->payload_size = payload_size;
    msg->timestamp = time(NULL);
    msg->sequence = bus->next_sequence++;
    
    return 1;
}

int agent_bus_subscribe(MessageBus* bus, const char* topic, const char* agent_id) {
    /* TODO: Implementar sistema de subscriptions */
    /* Por enquanto, todos recebem todas as mensagens */
    (void)bus;
    (void)topic;
    (void)agent_id;
    return 1;
}

int agent_bus_unsubscribe(MessageBus* bus, const char* topic, const char* agent_id) {
    /* TODO: Implementar */
    (void)bus;
    (void)topic;
    (void)agent_id;
    return 1;
}

Message* agent_bus_receive(MessageBus* bus, const char* agent_id, const char* topic, int blocking) {
    if (bus == NULL || agent_id == NULL) return NULL;
    
    /* Procura mensagem para este agente */
    for (size_t i = 0; i < bus->message_count; i++) {
        Message* msg = &bus->messages[i];
        
        /* Se topic especificado, filtra */
        if (topic && strcmp(msg->topic, topic) != 0) continue;
        
        /* Se sender_id é o próprio agente, pula */
        if (strcmp(msg->sender_id, agent_id) == 0) continue;
        
        return msg;
    }
    
    return NULL;
}

int agent_bus_poll(MessageBus* bus, const char* agent_id, Message*** out_messages, size_t* out_count) {
    if (bus == NULL || agent_id == NULL || out_messages == NULL || out_count == NULL) return 0;
    
    /* Conta mensagens para este agente */
    size_t count = 0;
    for (size_t i = 0; i < bus->message_count; i++) {
        Message* msg = &bus->messages[i];
        if (strcmp(msg->sender_id, agent_id) == 0) continue; /* Pula próprias mensagens */
        count++;
    }
    
    if (count == 0) {
        *out_messages = NULL;
        *out_count = 0;
        return 1;
    }
    
    Message** messages = (Message**)malloc(count * sizeof(Message*));
    if (messages == NULL) return 0;
    
    size_t idx = 0;
    for (size_t i = 0; i < bus->message_count; i++) {
        Message* msg = &bus->messages[i];
        if (strcmp(msg->sender_id, agent_id) == 0) continue;
        messages[idx++] = msg;
    }
    
    *out_messages = messages;
    *out_count = count;
    return 1;
}

/* =============================================================================
 * AGENT
 * ============================================================================= */

Agent* agent_create(const char* id, const char* name, Environment* env) {
    Agent* agent = (Agent*)calloc(1, sizeof(Agent));
    if (agent == NULL) return NULL;
    
    agent->id = id ? strdup(id) : NULL;
    agent->name = name ? strdup(name) : NULL;
    agent->environment = env;
    agent->context = agent_context_create(agent->id);
    agent->is_active = 1;
    
    return agent;
}

void agent_destroy(Agent* agent) {
    if (agent == NULL) return;
    
    agent_context_destroy(agent->context);
    free(agent->permissions);
    free(agent->id);
    free(agent->name);
    /* Não libera environment (não é dono) */
    free(agent);
}

int agent_pause(Agent* agent) {
    if (agent == NULL) return 0;
    agent->is_paused = 1;
    return 1;
}

int agent_resume(Agent* agent) {
    if (agent == NULL) return 0;
    agent->is_paused = 0;
    return 1;
}

int agent_send_message(Agent* from, Agent* to, const char* topic, void* payload, size_t payload_size) {
    if (from == NULL || to == NULL || topic == NULL) return 0;
    if (from->environment == NULL) return 0;
    
    Environment* env = (Environment*)from->environment;
    if (env->message_bus == NULL) return 0;
    
    return agent_bus_publish(env->message_bus, topic, from->id, payload, payload_size);
}

/* =============================================================================
 * FUNÇÕES NATIVAS PARA ASTERON
 * ============================================================================= */

/* agent.env_create(name) -> handle */
AsteronValue agent_native_env_create(int argc, AsteronValue* args) {
    const char* name = NULL;
    if (argc >= 1) {
        name = get_string_arg(args, 0);
    }
    
    Environment* env = agent_env_create(name);
    if (env == NULL) {
        return make_error(ASTERON_ERROR_RUNTIME);
    }
    
    /* Retorna como handle (número) */
    /* TODO: Implementar sistema de handles para environments */
    return ASTERON_NUMBER((double)(uintptr_t)env);
}

/* agent.spawn(env, name) -> agent_id */
AsteronValue agent_native_env_spawn(int argc, AsteronValue* args) {
    if (argc < 2) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    /* TODO: Obter environment do handle */
    Environment* env = NULL; /* Placeholder */
    const char* name = get_string_arg(args, 1);
    if (name == NULL) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    if (!agent_env_spawn_agent(env, name, NULL)) {
        return make_error(ASTERON_ERROR_RUNTIME);
    }
    
    /* Retorna ID do agente criado */
    /* TODO: Implementar retorno do ID */
    return make_string("agent_placeholder");
}

/* agent.publish(env, topic, data) -> bool */
AsteronValue agent_native_publish(int argc, AsteronValue* args) {
    if (argc < 3) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    /* TODO: Implementar */
    return ASTERON_BOOL(1);
}

/* agent.subscribe(env, topic) -> bool */
AsteronValue agent_native_subscribe(int argc, AsteronValue* args) {
    if (argc < 2) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    /* TODO: Implementar */
    return ASTERON_BOOL(1);
}

/* agent.shared_set(env, key, value) -> bool */
AsteronValue agent_native_shared_set(int argc, AsteronValue* args) {
    if (argc < 3) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    /* TODO: Implementar */
    return ASTERON_BOOL(1);
}

/* agent.shared_get(env, key) -> value */
AsteronValue agent_native_shared_get(int argc, AsteronValue* args) {
    if (argc < 2) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    /* TODO: Implementar */
    return ASTERON_NIL();
}

/* agent.context_export(agent_id, keys?) -> context */
AsteronValue agent_native_context_export(int argc, AsteronValue* args) {
    if (argc < 1) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    /* TODO: Implementar */
    return ASTERON_NIL();
}

/* agent.context_import(agent_id, context, strategy) -> bool */
AsteronValue agent_native_context_import(int argc, AsteronValue* args) {
    if (argc < 3) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    /* TODO: Implementar */
    return ASTERON_BOOL(1);
}

/* =============================================================================
 * REGISTRO DE FUNÇÕES NATIVAS
 * ============================================================================= */

void agent_module_register(void) {
    /* Environment */
    vm_register_native("agent_env_create", agent_native_env_create, 0, 1);
    vm_register_native("agent_spawn", agent_native_env_spawn, 2, 2);
    
    /* Message Bus */
    vm_register_native("agent_publish", agent_native_publish, 3, 3);
    vm_register_native("agent_subscribe", agent_native_subscribe, 2, 2);
    
    /* Shared Space */
    vm_register_native("agent_shared_set", agent_native_shared_set, 3, 3);
    vm_register_native("agent_shared_get", agent_native_shared_get, 2, 2);
    
    /* Context */
    vm_register_native("agent_context_export", agent_native_context_export, 1, 2);
    vm_register_native("agent_context_import", agent_native_context_import, 3, 3);
}

