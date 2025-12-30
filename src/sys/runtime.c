/**
 * =============================================================================
 * ASTERON MINIMAL RUNTIME - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "runtime.h"
#include <stdarg.h>
#include <string.h>

/* =============================================================================
 * RUNTIME GLOBAL
 * ============================================================================= */

static Runtime g_runtime = {0};

Runtime* rt_get(void) {
    return &g_runtime;
}

void rt_init(RuntimeConfig config) {
    if (g_runtime.initialized) return;
    
    g_runtime.config = config;
    
    /* Aloca heap */
    void* heap_base = alloc_pages(config.heap_initial);
    if (heap_base == 0) {
        eprint_str("FATAL: Failed to allocate heap\n");
        sys_exit(1);
    }
    
    bump_init(&g_runtime.heap, heap_base, config.heap_initial);
    
    /* Cria arena temporária */
    g_runtime.temp_arena = rt_arena_new(64 * 1024); /* 64KB blocks */
    
    g_runtime.initialized = true;
}

void rt_shutdown(void) {
    if (!g_runtime.initialized) return;
    
    if (g_runtime.temp_arena) {
        rt_arena_free(g_runtime.temp_arena);
    }
    
    if (g_runtime.heap.base) {
        free_pages(g_runtime.heap.base, g_runtime.heap.size);
    }
    
    g_runtime.initialized = false;
}

void* rt_alloc(usize size) {
    return bump_alloc(&g_runtime.heap, size, 8);
}

void* rt_alloc_aligned(usize size, usize align) {
    return bump_alloc(&g_runtime.heap, size, align);
}

void* rt_temp_alloc(usize size) {
    return rt_arena_alloc(g_runtime.temp_arena, size, 8);
}

void rt_temp_reset(void) {
    rt_arena_reset(g_runtime.temp_arena);
}

/* =============================================================================
 * BUMP ALLOCATOR
 * ============================================================================= */

void bump_init(BumpAllocator* bump, void* base, usize size) {
    bump->base = (u8*)base;
    bump->size = size;
    bump->offset = 0;
    bump->high_water = 0;
}

static inline usize align_up(usize n, usize align) {
    return (n + align - 1) & ~(align - 1);
}

void* bump_alloc(BumpAllocator* bump, usize size, usize align) {
    usize aligned_offset = align_up(bump->offset, align);
    
    if (aligned_offset + size > bump->size) {
        if (g_runtime.config.panic_on_oom) {
            panic("out of memory");
        }
        return 0;
    }
    
    void* ptr = bump->base + aligned_offset;
    bump->offset = aligned_offset + size;
    
    if (bump->offset > bump->high_water) {
        bump->high_water = bump->offset;
    }
    
    /* Zero memory */
    memset(ptr, 0, size);
    
    return ptr;
}

void bump_reset(BumpAllocator* bump) {
    bump->offset = 0;
}

/* =============================================================================
 * ARENA ALLOCATOR
 * ============================================================================= */

Arena* rt_arena_new(usize block_size) {
    Arena* arena = (Arena*)alloc_pages(sizeof(Arena) + block_size);
    if (arena == 0) return 0;
    
    arena->block_size = block_size;
    arena->total_allocated = block_size;
    arena->total_used = 0;
    
    /* Primeiro bloco está logo após o header */
    ArenaBlock* first = (ArenaBlock*)(arena + 1);
    first->data = (u8*)(first + 1);
    first->size = block_size - sizeof(ArenaBlock);
    first->used = 0;
    first->next = 0;
    
    arena->head = first;
    arena->current = first;
    
    return arena;
}

void* rt_arena_alloc(Arena* arena, usize size, usize align) {
    if (arena == 0) return 0;
    
    ArenaBlock* block = arena->current;
    
    /* Tenta alocar no bloco atual */
    usize aligned = align_up(block->used, align);
    
    if (aligned + size <= block->size) {
        void* ptr = block->data + aligned;
        block->used = aligned + size;
        arena->total_used += size;
        memset(ptr, 0, size);
        return ptr;
    }
    
    /* Precisa de novo bloco */
    usize new_size = arena->block_size;
    if (size + sizeof(ArenaBlock) > new_size) {
        new_size = size + sizeof(ArenaBlock);
    }
    
    void* new_mem = alloc_pages(new_size);
    if (new_mem == 0) {
        if (g_runtime.config.panic_on_oom) {
            panic("arena: out of memory");
        }
        return 0;
    }
    
    ArenaBlock* new_block = (ArenaBlock*)new_mem;
    new_block->data = (u8*)(new_block + 1);
    new_block->size = new_size - sizeof(ArenaBlock);
    new_block->used = size;
    new_block->next = 0;
    
    block->next = new_block;
    arena->current = new_block;
    arena->total_allocated += new_size;
    arena->total_used += size;
    
    memset(new_block->data, 0, size);
    return new_block->data;
}

void rt_arena_reset(Arena* arena) {
    if (arena == 0) return;
    
    /* Mantém primeiro bloco, libera os outros */
    ArenaBlock* block = arena->head->next;
    while (block) {
        ArenaBlock* next = block->next;
        free_pages(block, block->size + sizeof(ArenaBlock));
        block = next;
    }
    
    arena->head->next = 0;
    arena->head->used = 0;
    arena->current = arena->head;
    arena->total_used = 0;
}

void rt_arena_free(Arena* arena) {
    if (arena == 0) return;
    
    /* Libera blocos extras */
    ArenaBlock* block = arena->head->next;
    while (block) {
        ArenaBlock* next = block->next;
        free_pages(block, block->size + sizeof(ArenaBlock));
        block = next;
    }
    
    /* Libera arena + primeiro bloco */
    free_pages(arena, sizeof(Arena) + arena->block_size);
}

/* =============================================================================
 * STRINGS
 * ============================================================================= */

String string_new(void) {
    return (String){ .ptr = 0, .len = 0, .cap = 0 };
}

String string_from(const char* s) {
    if (s == 0) return string_new();
    
    usize len = 0;
    while (s[len]) len++;
    
    String str;
    str.cap = len + 1;
    str.ptr = (char*)rt_alloc(str.cap);
    str.len = len;
    
    memcpy(str.ptr, s, len);
    str.ptr[len] = 0;
    
    return str;
}

String string_with_capacity(usize cap) {
    String str;
    str.cap = cap;
    str.ptr = (char*)rt_alloc(cap);
    str.len = 0;
    if (str.ptr) str.ptr[0] = 0;
    return str;
}

void string_drop(String* s) {
    /* No-op com bump allocator - memória é liberada em batch */
    s->ptr = 0;
    s->len = 0;
    s->cap = 0;
}

Str string_as_str(const String* s) {
    return (Str){ .ptr = s->ptr, .len = s->len };
}

Str str_from(const char* s) {
    usize len = 0;
    if (s) while (s[len]) len++;
    return (Str){ .ptr = s, .len = len };
}

bool str_eq(Str a, Str b) {
    if (a.len != b.len) return false;
    for (usize i = 0; i < a.len; i++) {
        if (a.ptr[i] != b.ptr[i]) return false;
    }
    return true;
}

void string_push_str(String* s, Str other) {
    usize new_len = s->len + other.len;
    
    if (new_len + 1 > s->cap) {
        /* Precisa realocar */
        usize new_cap = s->cap * 2;
        if (new_cap < new_len + 1) new_cap = new_len + 1;
        
        char* new_ptr = (char*)rt_alloc(new_cap);
        if (s->ptr) {
            memcpy(new_ptr, s->ptr, s->len);
        }
        s->ptr = new_ptr;
        s->cap = new_cap;
    }
    
    memcpy(s->ptr + s->len, other.ptr, other.len);
    s->len = new_len;
    s->ptr[s->len] = 0;
}

void string_push(String* s, char c) {
    if (s->len + 2 > s->cap) {
        usize new_cap = s->cap ? s->cap * 2 : 16;
        char* new_ptr = (char*)rt_alloc(new_cap);
        if (s->ptr) {
            memcpy(new_ptr, s->ptr, s->len);
        }
        s->ptr = new_ptr;
        s->cap = new_cap;
    }
    
    s->ptr[s->len++] = c;
    s->ptr[s->len] = 0;
}

/* =============================================================================
 * VEC
 * ============================================================================= */

Vec vec_new(usize elem_size) {
    return (Vec){ .ptr = 0, .len = 0, .cap = 0, .elem_size = elem_size };
}

Vec vec_with_capacity(usize elem_size, usize cap) {
    Vec v;
    v.elem_size = elem_size;
    v.cap = cap;
    v.len = 0;
    v.ptr = rt_alloc(elem_size * cap);
    return v;
}

void vec_push(Vec* v, const void* elem) {
    if (v->len >= v->cap) {
        usize new_cap = v->cap ? v->cap * 2 : 8;
        void* new_ptr = rt_alloc(v->elem_size * new_cap);
        if (v->ptr) {
            memcpy(new_ptr, v->ptr, v->elem_size * v->len);
        }
        v->ptr = new_ptr;
        v->cap = new_cap;
    }
    
    memcpy((u8*)v->ptr + v->len * v->elem_size, elem, v->elem_size);
    v->len++;
}

bool vec_pop(Vec* v, void* out) {
    if (v->len == 0) return false;
    
    v->len--;
    if (out) {
        memcpy(out, (u8*)v->ptr + v->len * v->elem_size, v->elem_size);
    }
    return true;
}

void* vec_get(Vec* v, usize index) {
    if (index >= v->len) return 0;
    return (u8*)v->ptr + index * v->elem_size;
}

void vec_drop(Vec* v) {
    v->ptr = 0;
    v->len = 0;
    v->cap = 0;
}

void vec_clear(Vec* v) {
    v->len = 0;
}

/* =============================================================================
 * PANIC
 * ============================================================================= */

_Noreturn void panic(const char* msg) {
    eprint_str("\n!!! PANIC !!!\n");
    eprint_str(msg);
    eprint_str("\n");
    
    sys_exit(101);
    __builtin_unreachable();
}

_Noreturn void panic_fmt(const char* fmt, ...) {
    /* Simplificado - só imprime o formato */
    (void)fmt;
    eprint_str("\n!!! PANIC !!!\n");
    eprint_str(fmt);
    eprint_str("\n");
    
    sys_exit(101);
    __builtin_unreachable();
}

