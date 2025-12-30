/**
 * =============================================================================
 * ASTERON TASK MODULE - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "task_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #define NUM_CPUS_FUNC() ({ SYSTEM_INFO si; GetSystemInfo(&si); si.dwNumberOfProcessors; })
#else
    #include <pthread.h>
    #include <unistd.h>
    #include <sched.h>
    #define NUM_CPUS_FUNC() sysconf(_SC_NPROCESSORS_ONLN)
#endif

/* =============================================================================
 * HELPERS
 * ============================================================================= */

static AsteronValue make_error(AsteronResult code) {
    return ASTERON_ERROR_VAL(code);
}

static int g_next_task_id = 1;

/* =============================================================================
 * task.spawn(fn) -> handle
 * Nota: Implementação simplificada sem real threading por enquanto
 * ============================================================================= */

AsteronValue task_spawn(int argc, AsteronValue* args) {
    if (argc < 1) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    /* Cria task */
    Task* task = (Task*)malloc(sizeof(Task));
    task->header.obj_type = ASTERON_OBJ_TASK;
    task->header.flags = 0;
    task->header.ref_count = 1;
    task->header.next = NULL;
    task->state = TASK_PENDING;
    task->result = ASTERON_NIL();
    task->thread = NULL;
    task->fn = NULL;
    task->id = g_next_task_id++;
    
    /* TODO: Implementar threading real */
    /* Por enquanto, marca como "done" imediatamente */
    task->state = TASK_DONE;
    
    return ASTERON_HANDLE(task, HANDLE_THREAD);
}

/* =============================================================================
 * task.join(task) -> value | error
 * ============================================================================= */

AsteronValue task_join(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_HANDLE(args[0])) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    AsteronHandle* handle = ASTERON_AS_HANDLE(args[0]);
    Task* task = (Task*)handle;
    
    /* TODO: Aguardar thread real */
    
    if (task->state == TASK_ERROR) {
        return make_error(ASTERON_ERROR_RUNTIME);
    }
    
    return task->result;
}

/* =============================================================================
 * task.is_done(task) -> bool
 * ============================================================================= */

AsteronValue task_is_done(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_HANDLE(args[0])) {
        return ASTERON_BOOL(false);
    }
    
    AsteronHandle* handle = ASTERON_AS_HANDLE(args[0]);
    Task* task = (Task*)handle;
    
    return ASTERON_BOOL(task->state == TASK_DONE || 
                        task->state == TASK_CANCELLED ||
                        task->state == TASK_ERROR);
}

/* =============================================================================
 * task.cancel(task) -> bool
 * ============================================================================= */

AsteronValue task_cancel(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_HANDLE(args[0])) {
        return ASTERON_BOOL(false);
    }
    
    AsteronHandle* handle = ASTERON_AS_HANDLE(args[0]);
    Task* task = (Task*)handle;
    
    if (task->state == TASK_PENDING || task->state == TASK_RUNNING) {
        task->state = TASK_CANCELLED;
        return ASTERON_BOOL(true);
    }
    
    return ASTERON_BOOL(false);
}

/* =============================================================================
 * task.channel(capacity) -> { tx, rx }
 * ============================================================================= */

AsteronValue task_channel(int argc, AsteronValue* args) {
    size_t capacity = 16;  /* Default */
    
    if (argc >= 1 && ASTERON_IS_NUMBER(args[0])) {
        capacity = (size_t)ASTERON_AS_NUMBER(args[0]);
        if (capacity == 0) capacity = 1;
    }
    
    Channel* ch = (Channel*)malloc(sizeof(Channel));
    ch->header.obj_type = ASTERON_OBJ_CHANNEL;
    ch->header.flags = 0;
    ch->header.ref_count = 1;
    ch->header.next = NULL;
    ch->buffer = (AsteronValue*)calloc(capacity, sizeof(AsteronValue));
    ch->capacity = capacity;
    ch->head = 0;
    ch->tail = 0;
    ch->count = 0;
    ch->closed = false;
    
    #ifndef _WIN32
    ch->mutex = malloc(sizeof(pthread_mutex_t));
    ch->not_empty = malloc(sizeof(pthread_cond_t));
    ch->not_full = malloc(sizeof(pthread_cond_t));
    pthread_mutex_init((pthread_mutex_t*)ch->mutex, NULL);
    pthread_cond_init((pthread_cond_t*)ch->not_empty, NULL);
    pthread_cond_init((pthread_cond_t*)ch->not_full, NULL);
    #else
    ch->mutex = NULL;
    ch->not_empty = NULL;
    ch->not_full = NULL;
    #endif
    
    return ASTERON_HANDLE(ch, HANDLE_CUSTOM);
}

/* =============================================================================
 * task.send(ch, value) -> bool
 * ============================================================================= */

AsteronValue task_send(int argc, AsteronValue* args) {
    if (argc < 2 || !ASTERON_IS_HANDLE(args[0])) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    AsteronHandle* handle = ASTERON_AS_HANDLE(args[0]);
    Channel* ch = (Channel*)handle;
    
    if (ch->closed) {
        return ASTERON_BOOL(false);
    }
    
    #ifndef _WIN32
    pthread_mutex_lock((pthread_mutex_t*)ch->mutex);
    
    while (ch->count >= ch->capacity && !ch->closed) {
        pthread_cond_wait((pthread_cond_t*)ch->not_full, (pthread_mutex_t*)ch->mutex);
    }
    
    if (ch->closed) {
        pthread_mutex_unlock((pthread_mutex_t*)ch->mutex);
        return ASTERON_BOOL(false);
    }
    
    ch->buffer[ch->tail] = args[1];
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    
    pthread_cond_signal((pthread_cond_t*)ch->not_empty);
    pthread_mutex_unlock((pthread_mutex_t*)ch->mutex);
    #else
    /* Implementação simples sem locks para Windows */
    if (ch->count >= ch->capacity) {
        return ASTERON_BOOL(false);
    }
    ch->buffer[ch->tail] = args[1];
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    #endif
    
    return ASTERON_BOOL(true);
}

/* =============================================================================
 * task.recv(ch) -> value | error
 * ============================================================================= */

AsteronValue task_recv(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_HANDLE(args[0])) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    AsteronHandle* handle = ASTERON_AS_HANDLE(args[0]);
    Channel* ch = (Channel*)handle;
    
    #ifndef _WIN32
    pthread_mutex_lock((pthread_mutex_t*)ch->mutex);
    
    while (ch->count == 0 && !ch->closed) {
        pthread_cond_wait((pthread_cond_t*)ch->not_empty, (pthread_mutex_t*)ch->mutex);
    }
    
    if (ch->count == 0 && ch->closed) {
        pthread_mutex_unlock((pthread_mutex_t*)ch->mutex);
        return make_error(ASTERON_ERROR_EOF);
    }
    
    AsteronValue value = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    
    pthread_cond_signal((pthread_cond_t*)ch->not_full);
    pthread_mutex_unlock((pthread_mutex_t*)ch->mutex);
    
    return value;
    #else
    if (ch->count == 0) {
        return ch->closed ? make_error(ASTERON_ERROR_EOF) : ASTERON_NIL();
    }
    AsteronValue value = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    return value;
    #endif
}

/* =============================================================================
 * task.try_send/try_recv (non-blocking)
 * ============================================================================= */

AsteronValue task_try_send(int argc, AsteronValue* args) {
    if (argc < 2 || !ASTERON_IS_HANDLE(args[0])) {
        return ASTERON_BOOL(false);
    }
    
    AsteronHandle* handle = ASTERON_AS_HANDLE(args[0]);
    Channel* ch = (Channel*)handle;
    
    if (ch->closed || ch->count >= ch->capacity) {
        return ASTERON_BOOL(false);
    }
    
    ch->buffer[ch->tail] = args[1];
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    
    return ASTERON_BOOL(true);
}

AsteronValue task_try_recv(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_HANDLE(args[0])) {
        return ASTERON_NIL();
    }
    
    AsteronHandle* handle = ASTERON_AS_HANDLE(args[0]);
    Channel* ch = (Channel*)handle;
    
    if (ch->count == 0) {
        return ASTERON_NIL();
    }
    
    AsteronValue value = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    
    return value;
}

/* =============================================================================
 * task.close(ch) -> nil
 * ============================================================================= */

AsteronValue task_close_channel(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_HANDLE(args[0])) {
        return ASTERON_NIL();
    }
    
    AsteronHandle* handle = ASTERON_AS_HANDLE(args[0]);
    Channel* ch = (Channel*)handle;
    
    ch->closed = true;
    
    #ifndef _WIN32
    /* Acorda todos os waiters */
    pthread_cond_broadcast((pthread_cond_t*)ch->not_empty);
    pthread_cond_broadcast((pthread_cond_t*)ch->not_full);
    #endif
    
    return ASTERON_NIL();
}

/* =============================================================================
 * task.mutex() -> handle
 * ============================================================================= */

AsteronValue task_mutex_new(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    Mutex* m = (Mutex*)malloc(sizeof(Mutex));
    m->header.obj_type = ASTERON_OBJ_MODULE;  /* Reusa tipo */
    m->header.flags = 0;
    m->header.ref_count = 1;
    m->header.next = NULL;
    m->locked = false;
    
    #ifndef _WIN32
    m->mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init((pthread_mutex_t*)m->mutex, NULL);
    #else
    m->mutex = NULL;
    #endif
    
    return ASTERON_HANDLE(m, HANDLE_CUSTOM);
}

/* =============================================================================
 * task.lock/unlock/try_lock
 * ============================================================================= */

AsteronValue task_lock(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_HANDLE(args[0])) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    AsteronHandle* handle = ASTERON_AS_HANDLE(args[0]);
    Mutex* m = (Mutex*)handle;
    
    #ifndef _WIN32
    pthread_mutex_lock((pthread_mutex_t*)m->mutex);
    #endif
    m->locked = true;
    
    return ASTERON_NIL();
}

AsteronValue task_unlock(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_HANDLE(args[0])) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    AsteronHandle* handle = ASTERON_AS_HANDLE(args[0]);
    Mutex* m = (Mutex*)handle;
    
    m->locked = false;
    #ifndef _WIN32
    pthread_mutex_unlock((pthread_mutex_t*)m->mutex);
    #endif
    
    return ASTERON_NIL();
}

AsteronValue task_try_lock(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_HANDLE(args[0])) {
        return ASTERON_BOOL(false);
    }
    
    AsteronHandle* handle = ASTERON_AS_HANDLE(args[0]);
    Mutex* m = (Mutex*)handle;
    
    #ifndef _WIN32
    int result = pthread_mutex_trylock((pthread_mutex_t*)m->mutex);
    if (result == 0) {
        m->locked = true;
        return ASTERON_BOOL(true);
    }
    return ASTERON_BOOL(false);
    #else
    if (!m->locked) {
        m->locked = true;
        return ASTERON_BOOL(true);
    }
    return ASTERON_BOOL(false);
    #endif
}

/* =============================================================================
 * task.yield() -> nil
 * ============================================================================= */

AsteronValue task_yield(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #ifndef _WIN32
    sched_yield();
    #else
    SwitchToThread();
    #endif
    
    return ASTERON_NIL();
}

/* =============================================================================
 * task.num_cpus() -> number
 * ============================================================================= */

AsteronValue task_num_cpus(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return ASTERON_NUMBER((double)si.dwNumberOfProcessors);
    #else
    return ASTERON_NUMBER((double)sysconf(_SC_NPROCESSORS_ONLN));
    #endif
}

/* =============================================================================
 * task.sleep(ms) -> nil
 * ============================================================================= */

AsteronValue task_sleep(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return ASTERON_NIL();
    }
    
    double ms = ASTERON_AS_NUMBER(args[0]);
    
    #ifdef _WIN32
    Sleep((DWORD)ms);
    #else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)((ms - ts.tv_sec * 1000) * 1e6);
    nanosleep(&ts, NULL);
    #endif
    
    return ASTERON_NIL();
}

/* Funções não totalmente implementadas */
AsteronValue task_spawn_blocking(int argc, AsteronValue* args) { return task_spawn(argc, args); }
AsteronValue task_join_all(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }
AsteronValue task_current(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }

/* =============================================================================
 * DESCRITOR DO MÓDULO
 * ============================================================================= */

static ModuleExport task_exports[] = {
    /* Tasks */
    { "spawn",          task_spawn,          ASTERON_NIL(), 1, 1, "(fn) -> handle" },
    { "spawn_blocking", task_spawn_blocking, ASTERON_NIL(), 1, 1, "(fn) -> handle" },
    { "join",           task_join,           ASTERON_NIL(), 1, 1, "(task: handle) -> value" },
    { "cancel",         task_cancel,         ASTERON_NIL(), 1, 1, "(task: handle) -> bool" },
    { "is_done",        task_is_done,        ASTERON_NIL(), 1, 1, "(task: handle) -> bool" },
    
    /* Channels */
    { "channel",        task_channel,        ASTERON_NIL(), 0, 1, "(capacity?: number) -> handle" },
    { "send",           task_send,           ASTERON_NIL(), 2, 2, "(ch: handle, value) -> bool" },
    { "recv",           task_recv,           ASTERON_NIL(), 1, 1, "(ch: handle) -> value" },
    { "try_send",       task_try_send,       ASTERON_NIL(), 2, 2, "(ch: handle, value) -> bool" },
    { "try_recv",       task_try_recv,       ASTERON_NIL(), 1, 1, "(ch: handle) -> value | nil" },
    { "close",          task_close_channel,  ASTERON_NIL(), 1, 1, "(ch: handle) -> nil" },
    
    /* Sincronização */
    { "mutex",          task_mutex_new,      ASTERON_NIL(), 0, 0, "() -> handle" },
    { "lock",           task_lock,           ASTERON_NIL(), 1, 1, "(m: handle) -> nil" },
    { "unlock",         task_unlock,         ASTERON_NIL(), 1, 1, "(m: handle) -> nil" },
    { "try_lock",       task_try_lock,       ASTERON_NIL(), 1, 1, "(m: handle) -> bool" },
    
    /* Utilitários */
    { "yield",          task_yield,          ASTERON_NIL(), 0, 0, "() -> nil" },
    { "num_cpus",       task_num_cpus,       ASTERON_NIL(), 0, 0, "() -> number" },
    { "sleep",          task_sleep,          ASTERON_NIL(), 1, 1, "(ms: number) -> nil" },
};

NativeModuleDesc task_module = {
    .name = "task",
    .version = "1.0.0",
    .description = "Concorrência e async/await",
    .exports = task_exports,
    .export_count = sizeof(task_exports) / sizeof(task_exports[0]),
    .init = NULL,
    .cleanup = NULL
};

void task_module_register(void) {
    module_register_builtin(&task_module);
}

