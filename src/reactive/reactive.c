/**
 * =============================================================================
 * ASTERON STATE-DRIVEN RUNTIME - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "reactive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* =============================================================================
 * HELPERS
 * ============================================================================= */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

static bool default_equals(AsteronValue a, AsteronValue b) {
    if (a.type != b.type) return false;
    
    switch (a.type) {
        case ASTERON_VAL_NIL:
            return true;
        case ASTERON_VAL_BOOL:
            return a.as.boolean == b.as.boolean;
        case ASTERON_VAL_NUMBER:
            return a.as.number == b.as.number;
        case ASTERON_VAL_STRING: {
            AsteronString* sa = ASTERON_AS_STRING(a);
            AsteronString* sb = ASTERON_AS_STRING(b);
            if (sa == sb) return true;
            if (sa == NULL || sb == NULL) return false;
            if (sa->length != sb->length) return false;
            return memcmp(sa->chars, sb->chars, sa->length) == 0;
        }
        default:
            return a.as.ptr == b.as.ptr;
    }
}

/* =============================================================================
 * CRIAÇÃO DO RUNTIME
 * ============================================================================= */

ReactiveRuntime* reactive_runtime_create(void) {
    ReactiveRuntime* rt = (ReactiveRuntime*)calloc(1, sizeof(ReactiveRuntime));
    if (!rt) return NULL;
    
    /* Nós */
    rt->node_capacity = 256;
    rt->nodes = (ReactiveNode**)calloc(rt->node_capacity, sizeof(ReactiveNode*));
    rt->node_count = 0;
    rt->next_node_id = 1;
    
    /* Batch de propagação */
    rt->dirty_queue.capacity = REACTIVE_BATCH_SIZE;
    rt->dirty_queue.nodes = (ReactiveNode**)calloc(rt->dirty_queue.capacity, 
                                                     sizeof(ReactiveNode*));
    rt->dirty_queue.count = 0;
    rt->dirty_queue.generation = 0;
    
    /* Transações */
    rt->transaction_changes = (ReactiveNode**)calloc(256, sizeof(ReactiveNode*));
    
    /* Contextos */
    rt->contexts = calloc(32, sizeof(*rt->contexts));
    
    return rt;
}

void reactive_runtime_destroy(ReactiveRuntime* rt) {
    if (!rt) return;
    
    /* Limpa todos os nós */
    for (uint32_t i = 0; i < rt->node_count; i++) {
        ReactiveNode* node = rt->nodes[i];
        if (node) {
            if (node->cleanup) node->cleanup(node);
            free(node->deps);
            free(node->dependents);
            free((void*)node->name);
            free(node);
        }
    }
    
    free(rt->nodes);
    free(rt->dirty_queue.nodes);
    free(rt->transaction_changes);
    free(rt->contexts);
    free(rt);
}

/* =============================================================================
 * CRIAÇÃO DE NÓS
 * ============================================================================= */

static ReactiveNode* create_node(ReactiveRuntime* rt, const char* name,
                                  ReactiveNodeType type) {
    /* Expande se necessário */
    if (rt->node_count >= rt->node_capacity) {
        rt->node_capacity *= 2;
        rt->nodes = (ReactiveNode**)realloc(rt->nodes, 
            rt->node_capacity * sizeof(ReactiveNode*));
    }
    
    ReactiveNode* node = (ReactiveNode*)calloc(1, sizeof(ReactiveNode));
    node->id = rt->next_node_id++;
    node->name = name ? strdup(name) : NULL;
    node->type = type;
    node->state = STATE_CLEAN;
    atomic_init(&node->version, 1);
    node->value = ASTERON_NIL();
    node->prev_value = ASTERON_NIL();
    
    node->dep_capacity = 8;
    node->deps = (ReactiveNode**)calloc(node->dep_capacity, sizeof(ReactiveNode*));
    
    node->dependent_capacity = 8;
    node->dependents = (ReactiveNode**)calloc(node->dependent_capacity, 
                                               sizeof(ReactiveNode*));
    
    node->equals = default_equals;
    node->created_at = (uint32_t)(get_timestamp_us() / 1000);
    
    rt->nodes[rt->node_count++] = node;
    
    return node;
}

ReactiveNode* reactive_state(ReactiveRuntime* rt, const char* name,
                              AsteronValue initial) {
    ReactiveNode* node = create_node(rt, name, NODE_STATE);
    node->value = initial;
    node->prev_value = initial;
    return node;
}

ReactiveNode* reactive_derived(ReactiveRuntime* rt, const char* name,
                                ComputeFn compute, void* user_data) {
    ReactiveNode* node = create_node(rt, name, NODE_DERIVED);
    node->compute = compute;
    node->user_data = user_data;
    node->state = STATE_DIRTY;  /* Precisa computar inicialmente */
    return node;
}

ReactiveNode* reactive_effect(ReactiveRuntime* rt, const char* name,
                               ComputeFn effect, void* user_data) {
    ReactiveNode* node = create_node(rt, name, NODE_EFFECT);
    node->compute = effect;
    node->user_data = user_data;
    node->state = STATE_DIRTY;
    return node;
}

ReactiveNode* reactive_context(ReactiveRuntime* rt, const char* name) {
    ReactiveNode* node = create_node(rt, name, NODE_CONTEXT);
    node->context.context_name = strdup(name);
    node->context.fields = (ReactiveNode**)calloc(32, sizeof(ReactiveNode*));
    node->context.field_count = 0;
    
    /* Registra contexto */
    rt->contexts[rt->context_count].name = node->context.context_name;
    rt->contexts[rt->context_count].root = node;
    rt->context_count++;
    
    return node;
}

ReactiveNode* reactive_context_field(ReactiveRuntime* rt, ReactiveNode* ctx,
                                      const char* field_name, AsteronValue initial) {
    if (ctx->type != NODE_CONTEXT) return NULL;
    
    char full_name[256];
    snprintf(full_name, sizeof(full_name), "%s.%s", 
             ctx->context.context_name, field_name);
    
    ReactiveNode* field = reactive_state(rt, full_name, initial);
    
    /* Adiciona ao contexto */
    ctx->context.fields[ctx->context.field_count++] = field;
    
    /* Campo depende do contexto (para propagação) */
    reactive_add_dep(field, ctx);
    
    return field;
}

/* =============================================================================
 * LEITURA E ESCRITA
 * ============================================================================= */

AsteronValue reactive_get(ReactiveRuntime* rt, ReactiveNode* node) {
    if (!node) return ASTERON_NIL();
    
    /* Se está sendo computado, registra dependência */
    if (rt->computing && rt->computing != node) {
        reactive_add_dep(rt->computing, node);
    }
    
    /* Se está sujo e é derivado, re-computa */
    if (node->state == STATE_DIRTY && node->type == NODE_DERIVED) {
        reactive_recompute(rt, node);
    }
    
    return node->value;
}

void reactive_set(ReactiveRuntime* rt, ReactiveNode* node, AsteronValue value) {
    if (!node || node->type != NODE_STATE) return;
    
    /* Verifica se realmente mudou */
    if (node->equals(node->value, value)) {
        rt->stats.skipped_unchanged++;
        return;
    }
    
    /* Salva valor anterior */
    node->prev_value = node->value;
    node->value = value;
    
    /* Incrementa versão */
    atomic_fetch_add(&node->version, 1);
    node->last_updated = (uint32_t)(get_timestamp_us() / 1000);
    
    rt->stats.total_updates++;
    
    /* Em transação, adia propagação */
    if (rt->in_transaction) {
        rt->transaction_changes[rt->transaction_count++] = node;
        return;
    }
    
    /* Callback de mudança */
    if (rt->on_change) {
        rt->on_change(rt, node);
    }
    
    /* Invalida dependentes */
    reactive_invalidate(rt, node);
    
    /* Propaga */
    reactive_propagate(rt);
}

void reactive_update(ReactiveRuntime* rt, ReactiveNode* node,
                      AsteronValue (*updater)(AsteronValue current)) {
    if (!node || !updater) return;
    AsteronValue new_val = updater(node->value);
    reactive_set(rt, node, new_val);
}

bool reactive_changed(ReactiveNode* node) {
    return !node->equals(node->value, node->prev_value);
}

/* =============================================================================
 * DEPENDÊNCIAS
 * ============================================================================= */

void reactive_add_dep(ReactiveNode* node, ReactiveNode* dep) {
    if (!node || !dep) return;
    
    /* Verifica se já existe */
    for (uint32_t i = 0; i < node->dep_count; i++) {
        if (node->deps[i] == dep) return;
    }
    
    /* Expande se necessário */
    if (node->dep_count >= node->dep_capacity) {
        node->dep_capacity *= 2;
        node->deps = (ReactiveNode**)realloc(node->deps,
            node->dep_capacity * sizeof(ReactiveNode*));
    }
    
    node->deps[node->dep_count++] = dep;
    
    /* Adiciona este nó como dependente do dep */
    if (dep->dependent_count >= dep->dependent_capacity) {
        dep->dependent_capacity *= 2;
        dep->dependents = (ReactiveNode**)realloc(dep->dependents,
            dep->dependent_capacity * sizeof(ReactiveNode*));
    }
    dep->dependents[dep->dependent_count++] = node;
}

void reactive_remove_dep(ReactiveNode* node, ReactiveNode* dep) {
    if (!node || !dep) return;
    
    /* Remove das deps do node */
    for (uint32_t i = 0; i < node->dep_count; i++) {
        if (node->deps[i] == dep) {
            memmove(&node->deps[i], &node->deps[i + 1],
                    (node->dep_count - i - 1) * sizeof(ReactiveNode*));
            node->dep_count--;
            break;
        }
    }
    
    /* Remove dos dependents do dep */
    for (uint32_t i = 0; i < dep->dependent_count; i++) {
        if (dep->dependents[i] == node) {
            memmove(&dep->dependents[i], &dep->dependents[i + 1],
                    (dep->dependent_count - i - 1) * sizeof(ReactiveNode*));
            dep->dependent_count--;
            break;
        }
    }
}

void reactive_clear_deps(ReactiveNode* node) {
    if (!node) return;
    
    /* Remove este nó dos dependents de todas as deps */
    for (uint32_t i = 0; i < node->dep_count; i++) {
        ReactiveNode* dep = node->deps[i];
        for (uint32_t j = 0; j < dep->dependent_count; j++) {
            if (dep->dependents[j] == node) {
                memmove(&dep->dependents[j], &dep->dependents[j + 1],
                        (dep->dependent_count - j - 1) * sizeof(ReactiveNode*));
                dep->dependent_count--;
                break;
            }
        }
    }
    
    node->dep_count = 0;
}

/* =============================================================================
 * PROPAGAÇÃO
 * ============================================================================= */

void reactive_invalidate(ReactiveRuntime* rt, ReactiveNode* node) {
    if (!node || node->disposed) return;
    
    /* Adiciona dependentes à fila de dirty */
    for (uint32_t i = 0; i < node->dependent_count; i++) {
        ReactiveNode* dep = node->dependents[i];
        
        if (dep->state == STATE_CLEAN) {
            dep->state = STATE_DIRTY;
            
            /* Adiciona à fila */
            if (rt->dirty_queue.count < rt->dirty_queue.capacity) {
                rt->dirty_queue.nodes[rt->dirty_queue.count++] = dep;
            }
            
            /* Recursivamente invalida dependentes */
            reactive_invalidate(rt, dep);
        }
    }
}

void reactive_recompute(ReactiveRuntime* rt, ReactiveNode* node) {
    if (!node || !node->compute) return;
    
    node->state = STATE_COMPUTING;
    
    /* Limpa deps antigas e rastreia novas automaticamente */
    reactive_clear_deps(node);
    
    /* Salva nó atual */
    ReactiveNode* prev_computing = rt->computing;
    rt->computing = node;
    
    /* Computa novo valor */
    AsteronValue new_value = node->compute(rt, node);
    
    rt->computing = prev_computing;
    
    node->compute_count++;
    
    /* Verifica se mudou */
    if (!node->equals(node->value, new_value)) {
        node->prev_value = node->value;
        node->value = new_value;
        atomic_fetch_add(&node->version, 1);
        node->last_updated = (uint32_t)(get_timestamp_us() / 1000);
        
        /* Para effects, notifica */
        if (node->type == NODE_EFFECT && rt->on_effect) {
            rt->on_effect(rt, node);
        }
    }
    
    node->state = STATE_CLEAN;
}

void reactive_propagate(ReactiveRuntime* rt) {
    if (rt->is_propagating) return;
    
    rt->is_propagating = true;
    rt->dirty_queue.generation++;
    
    uint64_t start = get_timestamp_us();
    
    /* Processa todos os nós sujos */
    while (rt->dirty_queue.count > 0) {
        /* Pega próximo nó */
        ReactiveNode* node = rt->dirty_queue.nodes[--rt->dirty_queue.count];
        
        if (node->state != STATE_DIRTY || node->disposed) continue;
        
        /* Re-computa */
        reactive_recompute(rt, node);
        
        rt->stats.total_propagations++;
        
        /* Para effects, verifica triggers */
        if (node->type == NODE_EFFECT) {
            rt->stats.effects_triggered++;
        }
    }
    
    /* Verifica when blocks */
    reactive_check_triggers(rt);
    
    uint64_t elapsed = get_timestamp_us() - start;
    rt->stats.avg_propagation_time_us = 
        (rt->stats.avg_propagation_time_us * 0.9) + (elapsed * 0.1);
    
    rt->is_propagating = false;
}

/* =============================================================================
 * TRANSAÇÕES
 * ============================================================================= */

void reactive_transaction_begin(ReactiveRuntime* rt) {
    rt->in_transaction = true;
    rt->transaction_count = 0;
}

void reactive_transaction_commit(ReactiveRuntime* rt) {
    if (!rt->in_transaction) return;
    
    rt->in_transaction = false;
    
    /* Invalida todos os nós modificados */
    for (uint32_t i = 0; i < rt->transaction_count; i++) {
        reactive_invalidate(rt, rt->transaction_changes[i]);
    }
    
    /* Propaga uma vez */
    reactive_propagate(rt);
    
    rt->transaction_count = 0;
}

void reactive_transaction_rollback(ReactiveRuntime* rt) {
    if (!rt->in_transaction) return;
    
    /* Restaura valores anteriores */
    for (uint32_t i = 0; i < rt->transaction_count; i++) {
        ReactiveNode* node = rt->transaction_changes[i];
        node->value = node->prev_value;
    }
    
    rt->in_transaction = false;
    rt->transaction_count = 0;
}

/* =============================================================================
 * WHEN BLOCKS
 * ============================================================================= */

ReactiveNode* reactive_when(ReactiveRuntime* rt, const char* name,
                             ComputeFn condition, ComputeFn body,
                             bool once) {
    ReactiveNode* node = create_node(rt, name, NODE_EFFECT);
    node->trigger.triggered = false;
    node->trigger.once = once;
    node->compute = condition;
    node->user_data = (void*)body;
    node->state = STATE_DIRTY;
    return node;
}

void reactive_check_triggers(ReactiveRuntime* rt) {
    for (uint32_t i = 0; i < rt->node_count; i++) {
        ReactiveNode* node = rt->nodes[i];
        
        if (node->type != NODE_EFFECT || node->disposed) continue;
        if (node->trigger.once && node->trigger.triggered) continue;
        if (node->user_data == NULL) continue;  /* Não é when block */
        
        /* Avalia condição */
        AsteronValue cond = node->compute(rt, node);
        
        if (ASTERON_IS_TRUTHY(cond)) {
            /* Dispara body */
            ComputeFn body = (ComputeFn)node->user_data;
            body(rt, node);
            
            node->trigger.triggered = true;
            rt->stats.effects_triggered++;
        } else {
            /* Reset para when blocks que podem re-disparar */
            if (!node->trigger.once) {
                node->trigger.triggered = false;
            }
        }
    }
}

/* =============================================================================
 * CLEANUP
 * ============================================================================= */

void reactive_dispose(ReactiveRuntime* rt, ReactiveNode* node) {
    if (!node) return;
    
    node->disposed = true;
    
    /* Remove de dependentes */
    reactive_clear_deps(node);
    
    /* Remove dos dependentes de outros */
    for (uint32_t i = 0; i < node->dependent_count; i++) {
        reactive_remove_dep(node->dependents[i], node);
    }
    
    if (node->cleanup) {
        node->cleanup(node);
    }
}

void reactive_gc(ReactiveRuntime* rt) {
    /* Remove nós disposed */
    uint32_t write = 0;
    for (uint32_t read = 0; read < rt->node_count; read++) {
        if (!rt->nodes[read]->disposed) {
            rt->nodes[write++] = rt->nodes[read];
        } else {
            free(rt->nodes[read]->deps);
            free(rt->nodes[read]->dependents);
            free((void*)rt->nodes[read]->name);
            free(rt->nodes[read]);
        }
    }
    rt->node_count = write;
}

/* =============================================================================
 * DEBUG
 * ============================================================================= */

void reactive_print_graph(ReactiveRuntime* rt) {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              REACTIVE STATE GRAPH                            ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Nodes: %-5u   Contexts: %-3u   Generation: %-6u           ║\n",
           rt->node_count, rt->context_count, rt->dirty_queue.generation);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    
    const char* type_names[] = {
        "STATE", "DERIVED", "EFFECT", "CONTEXT", "COMPUTED", "RESOURCE"
    };
    const char* state_names[] = {
        "CLEAN", "DIRTY", "PENDING", "COMPUTING", "ERROR"
    };
    
    for (uint32_t i = 0; i < rt->node_count; i++) {
        ReactiveNode* node = rt->nodes[i];
        if (node->disposed) continue;
        
        printf("║ [%3u] %-20s %-8s %-10s v%-4lu       ║\n",
               node->id,
               node->name ? node->name : "<anon>",
               type_names[node->type],
               state_names[node->state],
               atomic_load(&node->version));
        
        if (node->dep_count > 0) {
            printf("║       └── deps: ");
            for (uint32_t j = 0; j < node->dep_count && j < 5; j++) {
                printf("%s ", node->deps[j]->name ? node->deps[j]->name : "?");
            }
            if (node->dep_count > 5) printf("...");
            printf("\n");
        }
    }
    
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Stats: updates=%-8lu props=%-8lu effects=%-8lu     ║\n",
           rt->stats.total_updates, rt->stats.total_propagations,
           rt->stats.effects_triggered);
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
}

void reactive_get_stats(ReactiveRuntime* rt, uint64_t* updates,
                         uint64_t* propagations, uint64_t* effects) {
    if (updates) *updates = rt->stats.total_updates;
    if (propagations) *propagations = rt->stats.total_propagations;
    if (effects) *effects = rt->stats.effects_triggered;
}

char* reactive_export_dot(ReactiveRuntime* rt) {
    /* Buffer simples */
    char* buf = (char*)malloc(64 * 1024);
    int pos = 0;
    
    pos += sprintf(buf + pos, "digraph ReactiveGraph {\n");
    pos += sprintf(buf + pos, "  rankdir=LR;\n");
    pos += sprintf(buf + pos, "  node [shape=box];\n\n");
    
    /* Cores por tipo */
    const char* colors[] = {
        "#4CAF50", /* STATE - verde */
        "#2196F3", /* DERIVED - azul */
        "#FF9800", /* EFFECT - laranja */
        "#9C27B0", /* CONTEXT - roxo */
        "#00BCD4", /* COMPUTED - cyan */
        "#607D8B"  /* RESOURCE - cinza */
    };
    
    for (uint32_t i = 0; i < rt->node_count; i++) {
        ReactiveNode* node = rt->nodes[i];
        if (node->disposed) continue;
        
        pos += sprintf(buf + pos, "  n%u [label=\"%s\", style=filled, fillcolor=\"%s\"];\n",
                       node->id, node->name ? node->name : "?", colors[node->type]);
        
        for (uint32_t j = 0; j < node->dep_count; j++) {
            pos += sprintf(buf + pos, "  n%u -> n%u;\n",
                           node->deps[j]->id, node->id);
        }
    }
    
    pos += sprintf(buf + pos, "}\n");
    
    return buf;
}

