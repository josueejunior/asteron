#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../graph/graph.h"
#include "../vm/vm.h"
#include "../utils/utils.h"
#include <stddef.h>
#include <pthread.h>

// Contexto isolado por task (evita compartilhamento de memória mutável)
typedef struct TaskContext {
    VM* vm;                        // VM isolada para esta task
    Stack* stack;                  // Stack isolada
    Environment* env;              // Environment isolado
    MemoryArena* arena;            // Arena de memória isolada
    char** capabilities;           // Lista de variáveis permitidas (isolamento)
    size_t cap_count;
} TaskContext;

// --- Sistema JIT (Just-In-Time) ---

typedef void (*JitFunction)(TaskContext* ctx);

typedef struct {
    JitFunction native_fn;         // Ponteiro para o código nativo gerado
    void* code_buffer;             // Buffer de memória executável
    size_t buffer_size;            // Tamanho do código gerado
    int is_valid;                  // Flag de validade
} JitCode;

// Task (unidade de trabalho)
typedef struct Task {
    size_t id;                    // ID único da task
    GraphNode* node;               // Nó do grafo correspondente (READ-ONLY após criação)
    size_t start_pc;               // PC inicial no bytecode
    size_t end_pc;                 // PC final no bytecode
    int dependencies_count;        // Número de dependências
    size_t* dependencies;         // IDs das tasks que devem executar antes
    int ready;                     // 1 se todas dependências foram satisfeitas
    int completed;                 // 1 se task foi completada
    pthread_mutex_t mutex;         // Mutex para sincronização
    TaskContext* context;          // Contexto isolado da task (NULL = usa contexto compartilhado)
    
    // Suporte JIT e FDO
    JitCode jit;                   // Cache JIT
    int exec_count;                // Contador de calor (Hot Path)
    ValueType observed_type;       // Profiling: tipo observado
    int type_stability_count;      // Ciclos com o mesmo tipo
    ValueType fixed_type;          // Tipo "congelado" para Tier 2
    int trip_count;                // FDO: Quantas vezes o loop interno girou
    int tier;                      // 0: Baseline, 1: JIT, 2: Optimized JIT
    int is_dirty;                  // Flag: 1 se a task precisa ser re-otimizada
    
    // Novas métricas para Camada 1
    int branch_taken_count;        // Quantas vezes saltos condicionais foram TRUE
    int branch_not_taken_count;    // Quantas vezes saltos condicionais foram FALSE
} Task;

// Worker thread
typedef struct Worker {
    pthread_t thread;
    int id;
    int running;
    struct Scheduler* scheduler;
} Worker;

// Scheduler (gerencia execução paralela)
// ============================================================
// REGRA DE OURO: Scheduler NÃO é dono de memória compartilhada
// ============================================================
// - cfg, dep_graph: READ-ONLY (propriedade do main)
// - bytecode: READ-ONLY (propriedade da VM)
// - vm: READ-ONLY (propriedade do main)
// - Scheduler apenas LÊ e COORDENA, nunca libera memória compartilhada
// ============================================================
typedef struct Scheduler {
    Task* tasks;                   // Array de tasks (propriedade do scheduler)
    size_t task_count;
    size_t task_capacity;
    
    Worker* workers;               // Pool de workers (propriedade do scheduler)
    size_t worker_count;
    
    // MEMÓRIA READ-ONLY (não liberar aqui)
    Graph* cfg;                    // Grafo de fluxo de controle (READ-ONLY)
    Graph* dep_graph;              // Grafo de dependências (READ-ONLY)
    BytecodeProgram* bytecode;     // Bytecode a executar (READ-ONLY)
    VM* vm;                        // VM compartilhada (READ-ONLY, usar locks)
    
    // Fila de tasks prontas
    size_t* ready_queue;           // Propriedade do scheduler
    size_t ready_count;
    size_t ready_capacity;
    pthread_mutex_t queue_mutex;   // Mutex para fila
    pthread_cond_t queue_cond;     // Condição para notificar workers
    
    int shutdown;                  // Flag para encerrar workers
    size_t completed_tasks;        // Contador de tasks completadas
    pthread_mutex_t completion_mutex;
    
    int conflict_detected;         // Flag: 1 se um conflito de dependência foi detectado
    int var_access_owner[100];     // Rastreia qual task é dona do acesso à variável
} Scheduler;

// Funções do scheduler
Scheduler* scheduler_create(Graph* cfg, Graph* dep_graph, BytecodeProgram* bytecode, VM* vm, size_t worker_count);
void scheduler_destroy(Scheduler* scheduler);
int scheduler_execute(Scheduler* scheduler);
void scheduler_print_tasks(Scheduler* scheduler);

// Funções de Contexto de Task
TaskContext* task_context_create(BytecodeProgram* program);
void task_context_destroy(TaskContext* context);

#endif // SCHEDULER_H

