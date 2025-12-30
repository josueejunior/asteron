#define _POSIX_C_SOURCE 200809L
#include "scheduler.h"
#include "../utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "../core/jit/jit.h"

// ==================== Contexto de Task ====================

TaskContext* task_context_create(BytecodeProgram* program) {
    TaskContext* context = (TaskContext*)malloc(sizeof(TaskContext));
    if (context == NULL) return NULL;
    
    // Cada task tem sua própria VM
    context->vm = vm_create(program);
    if (context->vm == NULL) {
        free(context);
        return NULL;
    }
    
    // Vincula stack e env da VM ao contexto (isolados)
    // A VM já cria sua própria stack e env no vm_create()
    context->stack = context->vm->stack;
    context->env = context->vm->env;
    
    // Cria arena isolada para a task
    context->arena = arena_create(1024 * 10); // 10KB para dados temporários da task
    
    return context;
}

void task_context_destroy(TaskContext* context) {
    if (context == NULL) return;
    
    // VM_destroy já destrói stack e env isolados da VM
    if (context->vm != NULL) vm_destroy(context->vm);
    
    // Destrói arena isolada
    if (context->arena != NULL) arena_destroy(context->arena);
    
    free(context);
}

// ==================== Scheduler ====================

// Função interna: constrói plano de execução baseado no grafo
static void build_execution_plan(Scheduler* scheduler) {
    if (scheduler == NULL || scheduler->cfg == NULL) return;
    
    Graph* cfg = scheduler->cfg;
    Graph* dep_graph = scheduler->dep_graph;
    
    // Cria tasks para cada nó do CFG que pode ser paralelizado
    scheduler->task_count = 0;
    
    for (size_t i = 0; i < cfg->node_count; i++) {
        GraphNode* node = cfg->nodes[i];
        if (node == NULL || node->name == NULL) continue;
        
        // Cria task apenas para statements que podem ser paralelizados
        if (node->type == NODE_STATEMENT && node->can_parallelize) {
            if (scheduler->task_count >= scheduler->task_capacity) {
                scheduler->task_capacity *= 2;
                Task* new_tasks = (Task*)realloc(
                    scheduler->tasks, sizeof(Task) * scheduler->task_capacity);
                if (new_tasks == NULL) return;
                scheduler->tasks = new_tasks;
            }
            
            Task* task = &scheduler->tasks[scheduler->task_count];
            task->id = scheduler->task_count;
            task->node = node;  // READ-ONLY - nunca modificar ou liberar
            
            // Mapeia o range de PC a partir da AST armazenada no nó do grafo
            ASTNode* ast_node = (ASTNode*)node->data;
            if (ast_node != NULL) {
                task->start_pc = ast_node->start_pc;
                task->end_pc = ast_node->end_pc;
            } else {
                task->start_pc = 0;
                task->end_pc = 0;
            }
            
            task->dependencies_count = 0;
            task->dependencies = NULL;
            task->ready = 0;
            task->completed = 0;
            task->exec_count = 0;
            task->type_stability_count = 0;
            task->fixed_type = VAL_NIL;
            task->trip_count = 0;
            task->branch_taken_count = 0;
            task->branch_not_taken_count = 0;
            task->tier = 0;
            task->is_dirty = 0;
            task->jit.is_valid = 0;
            task->jit.native_fn = NULL;
            
            // Cria contexto isolado da task
            task->context = task_context_create(scheduler->bytecode);
            
            pthread_mutex_init(&task->mutex, NULL);
            
            // Analisa dependências baseado no grafo de dependências
            if (dep_graph != NULL) {
                // Encontra variáveis usadas por este nó
                // (simplificado - em implementação completa, mapearia PC ranges)
                for (size_t j = 0; j < dep_graph->node_count; j++) {
                    GraphNode* dep_node = dep_graph->nodes[j];
                    if (dep_node == NULL) continue;
                    
                    // Verifica se há aresta de dependência
                    for (size_t k = 0; k < dep_node->edge_count; k++) {
                        GraphEdge* edge = dep_node->edges[k];
                        if (edge != NULL && edge->to != NULL) {
                            // Se este nó depende de outro, adiciona dependência
                            // (lógica simplificada)
                        }
                    }
                }
            }
            
            scheduler->task_count++;
        }
    }
    
    // Marca tasks sem dependências como prontas
    for (size_t i = 0; i < scheduler->task_count; i++) {
        if (scheduler->tasks[i].dependencies_count == 0) {
            scheduler->tasks[i].ready = 1;
        }
    }
}

// Função interna: marca task como pronta e adiciona à fila
static void mark_task_ready(Scheduler* scheduler, size_t task_id) {
    if (scheduler == NULL || task_id >= scheduler->task_count) return;
    
    pthread_mutex_lock(&scheduler->queue_mutex);
    
    // Adiciona à fila de prontas
    if (scheduler->ready_count >= scheduler->ready_capacity) {
        scheduler->ready_capacity *= 2;
        size_t* new_queue = (size_t*)realloc(
            scheduler->ready_queue, sizeof(size_t) * scheduler->ready_capacity);
        if (new_queue == NULL) {
            pthread_mutex_unlock(&scheduler->queue_mutex);
            return;
        }
        scheduler->ready_queue = new_queue;
    }
    
    scheduler->ready_queue[scheduler->ready_count++] = task_id;
    scheduler->tasks[task_id].ready = 1;
    
    // Notifica workers que há trabalho disponível
    pthread_cond_broadcast(&scheduler->queue_cond);
    pthread_mutex_unlock(&scheduler->queue_mutex);
}

// Função interna: thread worker que executa tasks
static void* worker_thread(void* arg) {
    Worker* worker = (Worker*)arg;
    Scheduler* scheduler = worker->scheduler;
    
    while (worker->running) {
        pthread_mutex_lock(&scheduler->queue_mutex);
        
        // Espera por tasks prontas
        while (scheduler->ready_count == 0 && !scheduler->shutdown) {
            pthread_cond_wait(&scheduler->queue_cond, &scheduler->queue_mutex);
        }
        
        if (scheduler->shutdown) {
            pthread_mutex_unlock(&scheduler->queue_mutex);
            break;
        }
        
        // Pega uma task da fila
        if (scheduler->ready_count > 0) {
            size_t task_id = scheduler->ready_queue[--scheduler->ready_count];
            pthread_mutex_unlock(&scheduler->queue_mutex);
            
            // Executa task
            Task* task = &scheduler->tasks[task_id];
            TaskContext* ctx = task->context; // Usa contexto isolado
            
            pthread_mutex_lock(&task->mutex);
            if (!task->completed) {
                task->exec_count++;

                // Lógica de Calor JIT
                if (task->exec_count >= JIT_HOT_THRESHOLD && !task->jit.is_valid) {
                    jit_compile_task(task);
                }

                if (task->jit.is_valid) {
                    printf("  [Worker %d] Executando task %zu via JIT NATIVO [Especialização: %s]\n", 
                           worker->id, task->id, (task->observed_type == VAL_NUMBER) ? "Numeric" : "Generic");
                    
                    // Salva snapshot para caso de falha (Rollback Point)
                    VMSnapshot rollback_point = vm_take_snapshot(ctx->vm);
                    
                    // Tenta executar código nativo
                    task->jit.native_fn(ctx);
                    
                    // Verifica se houve DEOPT (is_running foi resetado pelo handler)
                    if (!ctx->vm->is_running) {
                        printf("  [Worker %d] 🔄 Rollback ativado! Restaurando estado e voltando para VM...\n", worker->id);
                        vm_restore_snapshot(ctx->vm, rollback_point);
                        ctx->vm->is_running = 1;
                        task->is_dirty = 1; // Marca para re-compilação JIT (ou descarta)
                        
                        // Executa via VM para garantir conclusão segura
                        ctx->vm->pc = task->start_pc;
                        while (ctx->vm->pc < task->end_pc) {
                            size_t o_pc = ctx->vm->pc;
                            vm_step(ctx->vm, &task->observed_type, &task->type_stability_count);
                            if (ctx->vm->pc == o_pc) ctx->vm->pc++;
                        }
                    }
                    vm_snapshot_destroy(&rollback_point);
                } else {
                    printf("  [Worker %d] Executando task %zu via VM BYTECODE (PC: %zu-%zu) [Type: %d]\n", 
                           worker->id, task->id, task->start_pc, task->end_pc, task->observed_type);
                    
                    // Executa bytecode real usando o motor passo-a-passo
                    // Passamos o endereço do profiler para que a VM o atualize
                    ctx->vm->pc = task->start_pc;
                    while (ctx->vm->pc < task->end_pc) {
                        size_t old_pc = ctx->vm->pc;
                        vm_step(ctx->vm, &task->observed_type, &task->type_stability_count);
                        
                        // FDO: Se o PC pulou para trás dentro da task, incrementa trip count
                        if (ctx->vm->pc < old_pc) {
                            task->trip_count++;
                        }

                        // Proteção contra loop infinito
                        if (ctx->vm->pc == old_pc) {
                            ctx->vm->pc++;
                        }
                    }
                }
                
                // --- DETECÇÃO DE CONFLITO ADAPTATIVA ---
                // Verifica se há conflito de acesso a variáveis globais
                for (size_t pc = task->start_pc; pc < task->end_pc; pc++) {
                    Instruction* instr = &scheduler->bytecode->instructions[pc];
                    if (instr->op == OP_STORE_VAR || instr->op == OP_LOAD_VAR) {
                        size_t var_idx = instr->operand.var_index;
                        if (var_idx < 100) {
                            if (scheduler->var_access_owner[var_idx] != -1 && 
                                scheduler->var_access_owner[var_idx] != (int)task->id) {
                                // CONFLITO DETECTADO! Outra task está usando a mesma variável
                                scheduler->conflict_detected = 1;
                                printf("  [Adaptive] 🚨 CONFLITO: Tasks %d e %zu acessando var_%zu\n", 
                                       scheduler->var_access_owner[var_idx], task->id, var_idx);
                            }
                            scheduler->var_access_owner[var_idx] = (int)task->id;
                        }
                    }
                }
                
                task->completed = 1;
            }
            pthread_mutex_unlock(&task->mutex);
            
            // Marca como completada e verifica dependências
            pthread_mutex_lock(&scheduler->completion_mutex);
            scheduler->completed_tasks++;
            pthread_mutex_unlock(&scheduler->completion_mutex);
            
            // Verifica se outras tasks podem ser marcadas como prontas
            // (quando suas dependências foram satisfeitas)
            for (size_t i = 0; i < scheduler->task_count; i++) {
                Task* other_task = &scheduler->tasks[i];
                if (other_task->completed || other_task->ready) continue;
                
                // Verifica se todas dependências foram completadas
                int all_deps_done = 1;
                for (int j = 0; j < other_task->dependencies_count; j++) {
                    size_t dep_id = other_task->dependencies[j];
                    if (dep_id >= scheduler->task_count || 
                        !scheduler->tasks[dep_id].completed) {
                        all_deps_done = 0;
                        break;
                    }
                }
                
                if (all_deps_done) {
                    mark_task_ready(scheduler, i);
                }
            }
        } else {
            pthread_mutex_unlock(&scheduler->queue_mutex);
        }
    }
    
    return NULL;
}

Scheduler* scheduler_create(Graph* cfg, Graph* dep_graph, BytecodeProgram* bytecode, VM* vm, size_t worker_count) {
    if (cfg == NULL || bytecode == NULL || vm == NULL) return NULL;
    
    Scheduler* scheduler = (Scheduler*)malloc(sizeof(Scheduler));
    if (scheduler == NULL) return NULL;
    
    scheduler->cfg = cfg;
    scheduler->dep_graph = dep_graph;
    scheduler->bytecode = bytecode;
    scheduler->vm = vm;
    scheduler->worker_count = worker_count > 0 ? worker_count : 4;  // Default: 4 workers
    
    scheduler->task_count = 0;
    scheduler->task_capacity = 32;
    scheduler->tasks = (Task*)malloc(sizeof(Task) * scheduler->task_capacity);
    if (scheduler->tasks == NULL) {
        free(scheduler);
        return NULL;
    }
    
    scheduler->ready_count = 0;
    scheduler->ready_capacity = 32;
    scheduler->ready_queue = (size_t*)malloc(sizeof(size_t) * scheduler->ready_capacity);
    if (scheduler->ready_queue == NULL) {
        free(scheduler->tasks);
        free(scheduler);
        return NULL;
    }
    
    scheduler->workers = (Worker*)malloc(sizeof(Worker) * scheduler->worker_count);
    if (scheduler->workers == NULL) {
        free(scheduler->ready_queue);
        free(scheduler->tasks);
        free(scheduler);
        return NULL;
    }
    
    scheduler->shutdown = 0;
    scheduler->completed_tasks = 0;
    scheduler->conflict_detected = 0;
    for (int i = 0; i < 100; i++) scheduler->var_access_owner[i] = -1;
    
    pthread_mutex_init(&scheduler->queue_mutex, NULL);
    pthread_mutex_init(&scheduler->completion_mutex, NULL);
    pthread_cond_init(&scheduler->queue_cond, NULL);
    
    // Cria workers
    for (size_t i = 0; i < scheduler->worker_count; i++) {
        scheduler->workers[i].id = i;
        scheduler->workers[i].running = 1;
        scheduler->workers[i].scheduler = scheduler;
        
        if (pthread_create(&scheduler->workers[i].thread, NULL, worker_thread, 
                          &scheduler->workers[i]) != 0) {
            // Se falhar, limpa workers já criados
            scheduler->shutdown = 1;
            for (size_t j = 0; j < i; j++) {
                pthread_join(scheduler->workers[j].thread, NULL);
            }
            pthread_cond_destroy(&scheduler->queue_cond);
            pthread_mutex_destroy(&scheduler->completion_mutex);
            pthread_mutex_destroy(&scheduler->queue_mutex);
            free(scheduler->workers);
            free(scheduler->ready_queue);
            free(scheduler->tasks);
            free(scheduler);
            return NULL;
        }
    }
    
    // Constrói plano de execução
    build_execution_plan(scheduler);
    
    return scheduler;
}

void scheduler_destroy(Scheduler* scheduler) {
    if (scheduler == NULL) return;
    
    // Encerra workers
    scheduler->shutdown = 1;
    pthread_cond_broadcast(&scheduler->queue_cond);
    
    for (size_t i = 0; i < scheduler->worker_count; i++) {
        pthread_join(scheduler->workers[i].thread, NULL);
    }
    
    // Destrói tasks
    // IMPORTANTE: Não libera node, bytecode, vm, ou qualquer memória compartilhada
    // Apenas libera estruturas próprias da task (dependencies array, mutex, context)
    if (scheduler->tasks != NULL) {
        for (size_t i = 0; i < scheduler->task_count; i++) {
            // Libera array de dependências (propriedade da task)
            if (scheduler->tasks[i].dependencies != NULL) {
                free(scheduler->tasks[i].dependencies);
                scheduler->tasks[i].dependencies = NULL;
            }
            // Destrói mutex da task
            pthread_mutex_destroy(&scheduler->tasks[i].mutex);
            
            // Libera contexto isolado da task
            if (scheduler->tasks[i].context != NULL) {
                task_context_destroy(scheduler->tasks[i].context);
                scheduler->tasks[i].context = NULL;
            }
            
            // NÃO libera task->node (read-only, propriedade do grafo)
        }
        free(scheduler->tasks);
        scheduler->tasks = NULL;
    }
    
    // Destrói mutexes e condições
    pthread_cond_destroy(&scheduler->queue_cond);
    pthread_mutex_destroy(&scheduler->completion_mutex);
    pthread_mutex_destroy(&scheduler->queue_mutex);
    
    free(scheduler->ready_queue);
    free(scheduler->workers);
    free(scheduler);
}

int scheduler_execute(Scheduler* scheduler) {
    if (scheduler == NULL) return 1;
    
    printf("\n=== Execução Paralela (Scheduler) ===\n");
    printf("Tasks criadas: %zu\n", scheduler->task_count);
    printf("Workers: %zu\n", scheduler->worker_count);
    
    // Marca todas tasks sem dependências como prontas
    for (size_t i = 0; i < scheduler->task_count; i++) {
        if (scheduler->tasks[i].ready) {
            mark_task_ready(scheduler, i);
        }
    }
    
    // Espera todas tasks completarem
    while (scheduler->completed_tasks < scheduler->task_count) {
        // Espera um pouco antes de verificar novamente
        struct timespec ts = {0, 10000000};  // 10ms em nanosegundos
        nanosleep(&ts, NULL);
    }
    
    printf("✓ Todas tasks completadas\n");
    return 0;
}

void scheduler_print_tasks(Scheduler* scheduler) {
    if (scheduler == NULL) {
        printf("Scheduler: NULL\n");
        return;
    }
    
    printf("\n=== Tasks do Scheduler ===\n");
    printf("Total: %zu\n\n", scheduler->task_count);
    
    for (size_t i = 0; i < scheduler->task_count; i++) {
        Task* task = &scheduler->tasks[i];
        printf("Task %zu: %s\n", task->id, 
               task->node != NULL ? task->node->name : "unknown");
        printf("  Dependências: %d\n", task->dependencies_count);
        printf("  Status: %s\n", 
               task->completed ? "Completada" : 
               (task->ready ? "Pronta" : "Aguardando"));
    }
}

