/**
 * =============================================================================
 * ASTERON REGION-BASED MEMORY + ZERO-COPY - IMPLEMENTAÇÃO
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "region_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// =============================================================================
// REGION MANAGER GLOBAL
// =============================================================================

static RegionManager g_region_manager = {0};

// =============================================================================
// FUNÇÕES AUXILIARES
// =============================================================================

static uint64_t get_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static size_t align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// =============================================================================
// REGION CREATION & DESTRUCTION
// =============================================================================

void region_memory_init(void) {
    memset(&g_region_manager, 0, sizeof(RegionManager));
    g_region_manager.next_region_id = 1;
}

void region_memory_cleanup(void) {
    // Destrói todas as regiões ativas
    MemoryRegion* region = g_region_manager.regions;
    while (region != NULL) {
        MemoryRegion* next = region->next;
        region_destroy(region);
        region = next;
    }
    
    memset(&g_region_manager, 0, sizeof(RegionManager));
}

MemoryRegion* region_create(RegionType type, size_t capacity, const char* name) {
    MemoryRegion* region = (MemoryRegion*)calloc(1, sizeof(MemoryRegion));
    if (region == NULL) return NULL;
    
    // Aloca memória da região
    region->memory = malloc(capacity);
    if (region->memory == NULL) {
        free(region);
        return NULL;
    }
    
    // Inicializa campos
    region->id = g_region_manager.next_region_id++;
    region->type = type;
    region->name = name ? strdup(name) : NULL;
    region->state = REGION_ACTIVE;
    region->lifetime = LIFETIME_STATIC; // Será ajustado pelo borrow checker
    region->size = 0;
    region->capacity = capacity;
    region->alignment = sizeof(void*);
    region->created_at = get_timestamp();
    
    // Inicializa arrays de rastreamento
    region->tracked_values.capacity = 16;
    region->tracked_values.values = malloc(sizeof(Value) * region->tracked_values.capacity);
    region->tracked_values.count = 0;
    
    region->owned_vars.capacity = 16;
    region->owned_vars.var_ids = malloc(sizeof(uint32_t) * region->owned_vars.capacity);
    region->owned_vars.count = 0;
    
    region->dep_capacity = 8;
    region->dependencies = malloc(sizeof(MemoryRegion*) * region->dep_capacity);
    region->dep_count = 0;
    
    // Adiciona à lista global
    region->next = g_region_manager.regions;
    if (g_region_manager.regions) {
        g_region_manager.regions->prev = region;
    }
    g_region_manager.regions = region;
    
    g_region_manager.active_regions++;
    g_region_manager.total_regions++;
    g_region_manager.total_allocated += capacity;
    
    return region;
}

void region_destroy(MemoryRegion* region) {
    if (region == NULL || region->state == REGION_DESTROYED) return;
    
    // Callback de destruição
    if (region->on_destroy) {
        region->on_destroy(region);
    }
    
    // Libera todos os valores rastreados (sem reference counting individual)
    for (size_t i = 0; i < region->tracked_values.count; i++) {
        Value v = region->tracked_values.values[i];
        if (v.type == VAL_OBJ && v.as.obj != NULL) {
            // Libera objeto diretamente (sem decrementar ref_count)
            // Isso é seguro porque a região inteira está sendo destruída
            if (v.as.obj->type == OBJ_STRING) {
                ObjString* str = (ObjString*)v.as.obj;
                if (str->chars) {
                    // Se os chars estão na região, não precisa liberar
                    // Se são externos, libera
                    if (!region->memory || 
                        (void*)str->chars < (void*)region->memory ||
                        (void*)str->chars >= (void*)((char*)region->memory + region->capacity)) {
                        free((void*)str->chars);
                    }
                }
            }
            free(v.as.obj);
        }
    }
    
    // Libera memória da região
    free(region->memory);
    
    // Libera arrays de rastreamento
    free(region->tracked_values.values);
    free(region->owned_vars.var_ids);
    free(region->dependencies);
    
    // Remove da lista global
    if (region->prev) {
        region->prev->next = region->next;
    } else {
        g_region_manager.regions = region->next;
    }
    if (region->next) {
        region->next->prev = region->prev;
    }
    
    // Atualiza estatísticas
    g_region_manager.active_regions--;
    g_region_manager.total_allocated -= region->capacity;
    region->destroyed_at = get_timestamp();
    region->state = REGION_DESTROYED;
    
    // Libera nome
    if (region->name) {
        free((void*)region->name);
    }
    
    free(region);
}

// =============================================================================
// ALLOCATION
// =============================================================================

void region_set_current(MemoryRegion* region) {
    g_region_manager.current_region = region;
}

MemoryRegion* region_get_current(void) {
    return g_region_manager.current_region;
}

void* region_alloc(MemoryRegion* region, size_t size) {
    return region_alloc_aligned(region, size, region->alignment);
}

void* region_alloc_aligned(MemoryRegion* region, size_t size, size_t alignment) {
    if (region == NULL || region->state != REGION_ACTIVE) return NULL;
    
    // Alinha tamanho
    size_t aligned_size = align_size(size, alignment);
    
    // Alinha offset atual
    size_t aligned_offset = align_size(region->size, alignment);
    
    // Verifica capacidade
    if (aligned_offset + aligned_size > region->capacity) {
        return NULL; // Out of memory
    }
    
    // Aloca
    void* ptr = (char*)region->memory + aligned_offset;
    region->size = aligned_offset + aligned_size;
    
    // Atualiza estatísticas
    region->allocations++;
    if (region->size > region->peak_size) {
        region->peak_size = region->size;
    }
    
    // Zera memória
    memset(ptr, 0, aligned_size);
    
    return ptr;
}

// =============================================================================
// VALUE TRACKING
// =============================================================================

void region_track_value(MemoryRegion* region, Value value) {
    if (region == NULL) return;
    
    // Expande array se necessário
    if (region->tracked_values.count >= region->tracked_values.capacity) {
        region->tracked_values.capacity *= 2;
        region->tracked_values.values = realloc(region->tracked_values.values,
            sizeof(Value) * region->tracked_values.capacity);
    }
    
    // Adiciona valor
    region->tracked_values.values[region->tracked_values.count++] = value;
    
    // Retém referência (para evitar destruição prematura)
    value_retain(value);
}

int region_move_var(MemoryRegion* region, uint32_t var_id, BorrowChecker* bc) {
    if (region == NULL || bc == NULL) return 1;
    
    // Expande array se necessário
    if (region->owned_vars.count >= region->owned_vars.capacity) {
        region->owned_vars.capacity *= 2;
        region->owned_vars.var_ids = realloc(region->owned_vars.var_ids,
            sizeof(uint32_t) * region->owned_vars.capacity);
    }
    
    // Adiciona variável
    region->owned_vars.var_ids[region->owned_vars.count++] = var_id;
    
    // Marca variável como movida para região (no borrow checker)
    // Isso previne liberação individual
    // TODO: Integrar com borrow_checker_move
    
    return 0;
}

// =============================================================================
// REGION STATE
// =============================================================================

void region_freeze(MemoryRegion* region) {
    if (region == NULL) return;
    region->state = REGION_FROZEN;
}

void region_unfreeze(MemoryRegion* region) {
    if (region == NULL || region->state != REGION_FROZEN) return;
    region->state = REGION_ACTIVE;
}

void region_get_stats(MemoryRegion* region, size_t* used, size_t* peak, 
                      size_t* allocations) {
    if (region == NULL) return;
    if (used) *used = region->size;
    if (peak) *peak = region->peak_size;
    if (allocations) *allocations = region->allocations;
}

// =============================================================================
// ZERO-COPY BUFFERS
// =============================================================================

ZeroCopyBuffer* zerocopy_create_external(void* data, size_t size, 
                                         void (*free_fn)(void*)) {
    ZeroCopyBuffer* buf = (ZeroCopyBuffer*)malloc(sizeof(ZeroCopyBuffer));
    if (buf == NULL) return NULL;
    
    buf->data = data;
    buf->size = size;
    buf->is_region_owned = false;
    buf->region = NULL;
    buf->is_external = true;
    buf->free_fn = free_fn;
    
    return buf;
}

ZeroCopyBuffer* zerocopy_create_from_region(MemoryRegion* region, 
                                             void* data, size_t size) {
    ZeroCopyBuffer* buf = (ZeroCopyBuffer*)malloc(sizeof(ZeroCopyBuffer));
    if (buf == NULL) return NULL;
    
    buf->data = data;
    buf->size = size;
    buf->is_region_owned = true;
    buf->region = region;
    buf->is_external = false;
    buf->free_fn = NULL;
    
    return buf;
}

ZeroCopyBuffer* zerocopy_create_for_read(MemoryRegion* region, size_t size) {
    if (region == NULL) return NULL;
    
    // Aloca espaço na região para leitura direta
    void* data = region_alloc(region, size);
    if (data == NULL) return NULL;
    
    return zerocopy_create_from_region(region, data, size);
}

void zerocopy_destroy(ZeroCopyBuffer* buffer) {
    if (buffer == NULL) return;
    
    // Se é externo e tem função de liberação, chama
    if (buffer->is_external && buffer->free_fn) {
        buffer->free_fn(buffer->data);
    }
    
    // Se é de região, não precisa liberar (região cuida)
    
    free(buffer);
}

void* zerocopy_get_data(ZeroCopyBuffer* buffer) {
    return buffer ? buffer->data : NULL;
}

size_t zerocopy_get_size(ZeroCopyBuffer* buffer) {
    return buffer ? buffer->size : 0;
}

// =============================================================================
// INTEGRAÇÃO COM MÓDULOS NATIVOS
// =============================================================================

MemoryRegion* region_prepare_for_io(size_t expected_size, const char* name) {
    // Cria região temporária para I/O
    // Capacidade: expected_size + overhead
    size_t capacity = expected_size + (expected_size / 4); // 25% overhead
    if (capacity < 4096) capacity = 4096; // Mínimo 4KB
    
    MemoryRegion* region = region_create(REGION_TEMPORARY, capacity, name);
    if (region == NULL) return NULL;
    
    region_set_current(region);
    return region;
}

Value region_finish_io(MemoryRegion* region, ZeroCopyBuffer* buffer) {
    if (region == NULL || buffer == NULL) return value_nil();
    
    // Cria ObjString apontando diretamente para dados da região (zero-copy)
    ObjString* str = (ObjString*)region_alloc(region, sizeof(ObjString) + buffer->size + 1);
    if (str == NULL) {
        zerocopy_destroy(buffer);
        return value_nil();
    }
    
    // Inicializa objeto
    str->obj.type = OBJ_STRING;
    str->obj.ref_count = 1;
    str->length = buffer->size;
    str->chars = (char*)str + sizeof(ObjString);
    
    // Copia dados (ou usa diretamente se já estão na região)
    if (buffer->is_region_owned && buffer->data == str->chars) {
        // Já está no lugar certo, não precisa copiar
    } else {
        memcpy(str->chars, buffer->data, buffer->size);
        str->chars[buffer->size] = '\0';
    }
    
    // Cria Value
    Value value;
    value.type = VAL_OBJ;
    value.as.obj = (Obj*)str;
    
    // Rastreia na região
    region_track_value(region, value);
    
    // Libera buffer (mas dados já estão na região)
    zerocopy_destroy(buffer);
    
    return value;
}

// =============================================================================
// UTILITÁRIOS
// =============================================================================

void region_print_stats(void) {
    printf("\n=== Region Memory Statistics ===\n");
    printf("Total regions: %zu\n", g_region_manager.total_regions);
    printf("Active regions: %zu\n", g_region_manager.active_regions);
    printf("Total allocated: %zu bytes\n", g_region_manager.total_allocated);
    printf("\nRegions:\n");
    
    MemoryRegion* region = g_region_manager.regions;
    while (region != NULL) {
        printf("  [%llu] %s (type=%d, used=%zu/%zu, peak=%zu, allocs=%zu)\n",
               (unsigned long long)region->id,
               region->name ? region->name : "(unnamed)",
               region->type,
               region->size,
               region->capacity,
               region->peak_size,
               region->allocations);
        region = region->next;
    }
    printf("================================\n\n");
}

MemoryRegion* region_get_by_id(uint64_t id) {
    MemoryRegion* region = g_region_manager.regions;
    while (region != NULL) {
        if (region->id == id) return region;
        region = region->next;
    }
    return NULL;
}

MemoryRegion* region_get_by_name(const char* name) {
    if (name == NULL) return NULL;
    
    MemoryRegion* region = g_region_manager.regions;
    while (region != NULL) {
        if (region->name && strcmp(region->name, name) == 0) {
            return region;
        }
        region = region->next;
    }
    return NULL;
}

void region_destroy_all(RegionType type) {
    MemoryRegion* region = g_region_manager.regions;
    MemoryRegion* next;
    
    while (region != NULL) {
        next = region->next;
        if (region->type == type) {
            region_destroy(region);
        }
        region = next;
    }
}

