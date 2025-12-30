/**
 * =============================================================================
 * ASTERON TASK MODULE v1.0.0
 * =============================================================================
 * 
 * Módulo oficial para concorrência e async/await.
 * 
 * FUNÇÕES EXPORTADAS:
 * 
 * Tasks:
 *   task.spawn(fn)          -> handle (Task)
 *   task.spawn_blocking(fn) -> handle
 *   task.join(task)         -> value | error
 *   task.join_all(tasks)    -> array
 *   task.cancel(task)       -> bool
 *   task.is_done(task)      -> bool
 * 
 * Channels:
 *   task.channel(capacity)  -> { tx, rx }
 *   task.send(tx, value)    -> bool | blocks
 *   task.recv(rx)           -> value | blocks
 *   task.try_send(tx, val)  -> bool
 *   task.try_recv(rx)       -> value | nil
 *   task.close(ch)          -> nil
 * 
 * Sincronização:
 *   task.mutex()            -> handle
 *   task.lock(mutex)        -> nil
 *   task.unlock(mutex)      -> nil
 *   task.with_lock(m, fn)   -> value
 * 
 * Utilitários:
 *   task.yield()            -> nil
 *   task.current()          -> handle | nil
 *   task.parallel(fns)      -> array
 *   task.race(tasks)        -> value (primeiro)
 * 
 * CONSTANTES:
 *   task.NUM_CPUS           -> número de CPUs
 * 
 * =============================================================================
 */

#ifndef ASTERON_TASK_MODULE_H
#define ASTERON_TASK_MODULE_H

#include "../module.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * API DO MÓDULO
 * ============================================================================= */

extern NativeModuleDesc task_module;
void task_module_register(void);

/* =============================================================================
 * ESTRUTURAS INTERNAS
 * ============================================================================= */

typedef enum {
    TASK_PENDING,
    TASK_RUNNING,
    TASK_DONE,
    TASK_CANCELLED,
    TASK_ERROR
} TaskState;

typedef struct Task {
    AsteronObjHeader header;
    TaskState state;
    AsteronValue result;
    void* thread;           /* pthread_t ou similar */
    void* fn;               /* Função a executar */
    int id;
} Task;

typedef struct Channel {
    AsteronObjHeader header;
    AsteronValue* buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    void* mutex;            /* pthread_mutex_t */
    void* not_empty;        /* pthread_cond_t */
    void* not_full;         /* pthread_cond_t */
    bool closed;
} Channel;

typedef struct Mutex {
    AsteronObjHeader header;
    void* mutex;            /* pthread_mutex_t */
    bool locked;
} Mutex;

/* =============================================================================
 * FUNÇÕES NATIVAS
 * ============================================================================= */

/* Tasks */
AsteronValue task_spawn(int argc, AsteronValue* args);
AsteronValue task_spawn_blocking(int argc, AsteronValue* args);
AsteronValue task_join(int argc, AsteronValue* args);
AsteronValue task_join_all(int argc, AsteronValue* args);
AsteronValue task_cancel(int argc, AsteronValue* args);
AsteronValue task_is_done(int argc, AsteronValue* args);

/* Channels */
AsteronValue task_channel(int argc, AsteronValue* args);
AsteronValue task_send(int argc, AsteronValue* args);
AsteronValue task_recv(int argc, AsteronValue* args);
AsteronValue task_try_send(int argc, AsteronValue* args);
AsteronValue task_try_recv(int argc, AsteronValue* args);
AsteronValue task_close_channel(int argc, AsteronValue* args);

/* Sincronização */
AsteronValue task_mutex_new(int argc, AsteronValue* args);
AsteronValue task_lock(int argc, AsteronValue* args);
AsteronValue task_unlock(int argc, AsteronValue* args);
AsteronValue task_try_lock(int argc, AsteronValue* args);

/* Utilitários */
AsteronValue task_yield(int argc, AsteronValue* args);
AsteronValue task_current(int argc, AsteronValue* args);
AsteronValue task_num_cpus(int argc, AsteronValue* args);
AsteronValue task_sleep(int argc, AsteronValue* args);

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_TASK_MODULE_H */

