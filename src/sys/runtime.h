/**
 * =============================================================================
 * ASTERON MINIMAL RUNTIME v1.0
 * =============================================================================
 * 
 * Runtime mínimo para Asteron como linguagem de sistema.
 * - Sem garbage collector
 * - Ownership-based memory management
 * - Zero-cost abstractions
 * 
 * Este runtime pode ser usado para:
 * - Standalone executables
 * - Daemons e serviços
 * - Ferramentas de linha de comando
 * - Embedded systems
 * 
 * =============================================================================
 */

#ifndef ASTERON_RUNTIME_H
#define ASTERON_RUNTIME_H

#include "syscall.h"
#include "../core/memory/ownership.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * CONFIGURAÇÃO DO RUNTIME
 * ============================================================================= */

typedef struct {
    usize stack_size;           /* Tamanho da stack (default: 8MB) */
    usize heap_initial;         /* Heap inicial (default: 1MB) */
    usize heap_max;             /* Heap máximo (default: 256MB) */
    bool panic_on_oom;          /* Panic se out of memory */
    bool enable_backtrace;      /* Habilitar backtrace em panics */
    i32 argc;                   /* Argumentos de linha de comando */
    char** argv;
    char** envp;
} RuntimeConfig;

#define RUNTIME_DEFAULT_CONFIG (RuntimeConfig){ \
    .stack_size = 8 * 1024 * 1024, \
    .heap_initial = 1 * 1024 * 1024, \
    .heap_max = 256 * 1024 * 1024, \
    .panic_on_oom = true, \
    .enable_backtrace = true, \
    .argc = 0, \
    .argv = 0, \
    .envp = 0 \
}

/* =============================================================================
 * ALOCADOR DE MEMÓRIA (Arena-based)
 * ============================================================================= */

typedef struct ArenaBlock {
    u8* data;                   /* Dados do bloco */
    usize size;                 /* Tamanho do bloco */
    usize used;                 /* Bytes usados */
    struct ArenaBlock* next;    /* Próximo bloco */
} ArenaBlock;

typedef struct {
    ArenaBlock* head;           /* Primeiro bloco */
    ArenaBlock* current;        /* Bloco atual */
    usize block_size;           /* Tamanho padrão de bloco */
    usize total_allocated;      /* Total alocado */
    usize total_used;           /* Total usado */
} Arena;

/**
 * Cria nova arena
 */
Arena* rt_arena_new(usize block_size);

/**
 * Aloca na arena (nunca falha se panic_on_oom)
 */
void* rt_arena_alloc(Arena* arena, usize size, usize align);

/**
 * Reseta arena (libera tudo)
 */
void rt_arena_reset(Arena* arena);

/**
 * Destrói arena
 */
void rt_arena_free(Arena* arena);

/* =============================================================================
 * ALOCADOR GLOBAL (BUMP ALLOCATOR)
 * ============================================================================= */

typedef struct {
    u8* base;                   /* Base do heap */
    usize size;                 /* Tamanho total */
    usize offset;               /* Próximo byte livre */
    usize high_water;           /* Marca d'água (máximo usado) */
} BumpAllocator;

/**
 * Inicializa alocador global
 */
void bump_init(BumpAllocator* bump, void* base, usize size);

/**
 * Aloca memória
 */
void* bump_alloc(BumpAllocator* bump, usize size, usize align);

/**
 * Reseta alocador
 */
void bump_reset(BumpAllocator* bump);

/* =============================================================================
 * STRINGS (OWNED & BORROWED)
 * ============================================================================= */

/* String owned (allocated) */
typedef struct {
    char* ptr;
    usize len;
    usize cap;
} String;

/* String slice (borrowed) */
typedef struct {
    const char* ptr;
    usize len;
} Str;

/**
 * Cria String vazia
 */
String string_new(void);

/**
 * Cria String de literal C
 */
String string_from(const char* s);

/**
 * Cria String com capacidade
 */
String string_with_capacity(usize cap);

/**
 * Libera String
 */
void string_drop(String* s);

/**
 * Obtém Str (borrow) de String
 */
Str string_as_str(const String* s);

/**
 * Cria Str de literal C
 */
Str str_from(const char* s);

/**
 * Compara Str
 */
bool str_eq(Str a, Str b);

/**
 * Concatena strings
 */
void string_push_str(String* s, Str other);

/**
 * Adiciona char
 */
void string_push(String* s, char c);

/* =============================================================================
 * VETOR DINÂMICO
 * ============================================================================= */

typedef struct {
    void* ptr;
    usize len;
    usize cap;
    usize elem_size;
} Vec;

/**
 * Cria vetor vazio
 */
Vec vec_new(usize elem_size);

/**
 * Cria vetor com capacidade
 */
Vec vec_with_capacity(usize elem_size, usize cap);

/**
 * Adiciona elemento
 */
void vec_push(Vec* v, const void* elem);

/**
 * Remove último elemento
 */
bool vec_pop(Vec* v, void* out);

/**
 * Obtém elemento
 */
void* vec_get(Vec* v, usize index);

/**
 * Libera vetor
 */
void vec_drop(Vec* v);

/**
 * Limpa vetor (mantém capacidade)
 */
void vec_clear(Vec* v);

/* =============================================================================
 * RESULT<T, E> - Tratamento de Erros
 * ============================================================================= */

typedef enum {
    RESULT_OK,
    RESULT_ERR
} ResultTag;

/* Macro para criar tipos Result */
#define RESULT_TYPE(T, E) struct { ResultTag tag; union { T ok; E err; } v; }

typedef RESULT_TYPE(void*, const char*) ResultPtr;
typedef RESULT_TYPE(i32, const char*) ResultI32;
typedef RESULT_TYPE(Fd, const char*) ResultFd;

#define OK(val) { .tag = RESULT_OK, .v.ok = (val) }
#define ERR(e) { .tag = RESULT_ERR, .v.err = (e) }
#define IS_OK(r) ((r).tag == RESULT_OK)
#define IS_ERR(r) ((r).tag == RESULT_ERR)
#define UNWRAP(r) ((r).v.ok)
#define UNWRAP_ERR(r) ((r).v.err)

/* =============================================================================
 * OPTION<T> - Valores opcionais
 * ============================================================================= */

typedef enum {
    OPTION_NONE,
    OPTION_SOME
} OptionTag;

#define OPTION_TYPE(T) struct { OptionTag tag; T value; }

typedef OPTION_TYPE(void*) OptionPtr;
typedef OPTION_TYPE(i32) OptionI32;

#define SOME(val) { .tag = OPTION_SOME, .value = (val) }
#define NONE { .tag = OPTION_NONE }
#define IS_SOME(o) ((o).tag == OPTION_SOME)
#define IS_NONE(o) ((o).tag == OPTION_NONE)

/* =============================================================================
 * PANIC E ASSERT
 * ============================================================================= */

/**
 * Panic com mensagem
 */
_Noreturn void panic(const char* msg);

/**
 * Panic com formato
 */
_Noreturn void panic_fmt(const char* fmt, ...);

/**
 * Assert com mensagem
 */
#define assert_msg(cond, msg) \
    do { if (!(cond)) panic("assertion failed: " msg); } while(0)

/**
 * Unreachable
 */
#define unreachable() panic("entered unreachable code")

/**
 * Todo (não implementado)
 */
#define todo() panic("not yet implemented")

/* =============================================================================
 * RUNTIME GLOBAL
 * ============================================================================= */

typedef struct {
    RuntimeConfig config;
    BumpAllocator heap;
    Arena* temp_arena;          /* Arena para alocações temporárias */
    BorrowChecker* bc;          /* Borrow checker (debug mode) */
    bool initialized;
} Runtime;

/**
 * Obtém runtime global
 */
Runtime* rt_get(void);

/**
 * Inicializa runtime
 */
void rt_init(RuntimeConfig config);

/**
 * Finaliza runtime
 */
void rt_shutdown(void);

/**
 * Aloca no heap global
 */
void* rt_alloc(usize size);

/**
 * Aloca alinhado no heap global
 */
void* rt_alloc_aligned(usize size, usize align);

/**
 * Aloca temporário (liberado no fim do frame)
 */
void* rt_temp_alloc(usize size);

/**
 * Reseta alocações temporárias
 */
void rt_temp_reset(void);

/* =============================================================================
 * ENTRY POINT
 * ============================================================================= */

/**
 * Main function signature
 */
typedef i32 (*MainFn)(i32 argc, char** argv);

/**
 * Macro para definir entry point
 */
#define ASTERON_MAIN(main_fn) \
    int main(int argc, char** argv) { \
        RuntimeConfig cfg = RUNTIME_DEFAULT_CONFIG; \
        cfg.argc = argc; \
        cfg.argv = argv; \
        rt_init(cfg); \
        i32 result = main_fn(argc, argv); \
        rt_shutdown(); \
        return result; \
    }

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_RUNTIME_H */

