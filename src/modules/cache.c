/**
 * =============================================================================
 * ASTERON MODULE CACHE - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

/* =============================================================================
 * CACHE GLOBAL
 * ============================================================================= */

static ModuleCache* g_global_cache = NULL;

/* =============================================================================
 * FUNÇÕES AUXILIARES
 * ============================================================================= */

static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static uint64_t get_current_time(void) {
    return (uint64_t)time(NULL);
}

static uint64_t get_file_mtime(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (uint64_t)st.st_mtime;
}

static char* normalize_path(const char* path) {
    /* Simplificado: apenas duplica o path */
    return strdup(path);
}

static void ensure_dir_exists(const char* dir) {
    struct stat st;
    if (stat(dir, &st) != 0) {
        mkdir(dir, 0755);
    }
}

static char* get_cache_path(ModuleCache* cache, const char* key) {
    if (cache == NULL || cache->cache_dir == NULL || key == NULL) {
        return NULL;
    }
    
    /* Cria hash do key para nome único */
    uint32_t hash = hash_string(key);
    
    size_t len = strlen(cache->cache_dir) + 32;
    char* path = malloc(len);
    snprintf(path, len, "%s/%08x.astm", cache->cache_dir, hash);
    
    return path;
}

/* =============================================================================
 * LRU HELPERS
 * ============================================================================= */

static void lru_remove(ModuleCache* cache, CacheEntry* entry) {
    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        cache->lru_head = entry->next;
    }
    
    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        cache->lru_tail = entry->prev;
    }
    
    entry->prev = NULL;
    entry->next = NULL;
}

static void lru_insert_front(ModuleCache* cache, CacheEntry* entry) {
    entry->prev = NULL;
    entry->next = cache->lru_head;
    
    if (cache->lru_head) {
        cache->lru_head->prev = entry;
    }
    cache->lru_head = entry;
    
    if (cache->lru_tail == NULL) {
        cache->lru_tail = entry;
    }
}

static void lru_touch(ModuleCache* cache, CacheEntry* entry) {
    lru_remove(cache, entry);
    lru_insert_front(cache, entry);
    entry->last_access = get_current_time();
}

static void evict_lru(ModuleCache* cache) {
    if (cache->lru_tail == NULL) return;
    
    CacheEntry* victim = cache->lru_tail;
    
    printf("[Cache] Evicting: %s\n", victim->key);
    
    /* Remove da LRU list */
    lru_remove(cache, victim);
    
    /* Remove da hash table */
    uint32_t hash = hash_string(victim->key);
    size_t bucket = hash % cache->bucket_count;
    
    CacheEntry** pp = &cache->buckets[bucket];
    while (*pp != NULL) {
        if (*pp == victim) {
            *pp = victim->next;
            break;
        }
        pp = &(*pp)->next;
    }
    
    /* Grava em disco se dirty */
    if (victim->is_dirty && victim->module) {
        char* cache_path = get_cache_path(cache, victim->key);
        if (cache_path) {
            astm_save(victim->module, cache_path);
            free(cache_path);
        }
    }
    
    /* Libera recursos */
    free(victim->key);
    if (victim->module) {
        astm_free(victim->module);
    }
    free(victim);
    
    cache->entry_count--;
    cache->evictions++;
}

/* =============================================================================
 * API DO CACHE
 * ============================================================================= */

#define CACHE_BUCKETS 32

ModuleCache* cache_create(const char* cache_dir, size_t max_entries) {
    ModuleCache* cache = (ModuleCache*)calloc(1, sizeof(ModuleCache));
    if (cache == NULL) return NULL;
    
    cache->bucket_count = CACHE_BUCKETS;
    cache->buckets = (CacheEntry**)calloc(CACHE_BUCKETS, sizeof(CacheEntry*));
    if (cache->buckets == NULL) {
        free(cache);
        return NULL;
    }
    
    cache->max_entries = max_entries > 0 ? max_entries : CACHE_MAX_MEMORY_ENTRIES;
    cache->entry_count = 0;
    cache->lru_head = NULL;
    cache->lru_tail = NULL;
    
    cache->hits = 0;
    cache->misses = 0;
    cache->evictions = 0;
    
    if (cache_dir) {
        cache->cache_dir = strdup(cache_dir);
        ensure_dir_exists(cache->cache_dir);
    } else {
        cache->cache_dir = strdup(CACHE_DIR);
        ensure_dir_exists(cache->cache_dir);
    }
    
    cache->is_locked = 0;
    
    printf("[Cache] Criado cache com %zu slots em '%s'\n", 
           cache->max_entries, cache->cache_dir);
    
    return cache;
}

void cache_destroy(ModuleCache* cache) {
    if (cache == NULL) return;
    
    /* Flush antes de destruir */
    cache_flush(cache);
    
    /* Libera todas as entradas */
    for (size_t i = 0; i < cache->bucket_count; i++) {
        CacheEntry* entry = cache->buckets[i];
        while (entry != NULL) {
            CacheEntry* next = entry->next;
            free(entry->key);
            if (entry->module) {
                astm_free(entry->module);
            }
            free(entry);
            entry = next;
        }
    }
    
    free(cache->buckets);
    free(cache->cache_dir);
    free(cache);
}

static CacheEntry* cache_lookup(ModuleCache* cache, const char* key) {
    uint32_t hash = hash_string(key);
    size_t bucket = hash % cache->bucket_count;
    
    CacheEntry* entry = cache->buckets[bucket];
    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

CompiledModule* cache_get(ModuleCache* cache, const char* source_path) {
    if (cache == NULL || source_path == NULL) return NULL;
    
    char* key = normalize_path(source_path);
    if (key == NULL) return NULL;
    
    /* Verifica cache em memória */
    CacheEntry* entry = cache_lookup(cache, key);
    
    if (entry != NULL) {
        /* Verifica se ainda válido */
        uint64_t current_mtime = get_file_mtime(source_path);
        
        if (current_mtime > 0 && current_mtime > entry->source_mtime) {
            /* Fonte modificado, invalidar */
            printf("[Cache] Invalidando (fonte modificado): %s\n", key);
            cache_invalidate(cache, key);
            entry = NULL;
        } else {
            /* Cache hit */
            cache->hits++;
            lru_touch(cache, entry);
            entry->hit_count++;
            free(key);
            return entry->module;
        }
    }
    
    cache->misses++;
    
    /* Tenta carregar do disco */
    char* cache_path = get_cache_path(cache, key);
    if (cache_path && astm_is_valid(cache_path, source_path)) {
        CompiledModule* module = astm_load(cache_path);
        if (module != NULL) {
            printf("[Cache] Carregado do disco: %s\n", key);
            cache_put(cache, key, module);
            free(cache_path);
            free(key);
            return module;
        }
    }
    free(cache_path);
    
    /* Compila do fonte */
    printf("[Cache] Compilando: %s\n", key);
    
    CompiledModule* module = astm_compile(source_path, NULL);
    if (module != NULL) {
        cache_put(cache, key, module);
        
        /* Marca como dirty para gravar em disco depois */
        CacheEntry* new_entry = cache_lookup(cache, key);
        if (new_entry) {
            new_entry->is_dirty = 1;
        }
    }
    
    free(key);
    return module;
}

void cache_put(ModuleCache* cache, const char* key, CompiledModule* module) {
    if (cache == NULL || key == NULL || module == NULL) return;
    
    /* Verifica se já existe */
    CacheEntry* existing = cache_lookup(cache, key);
    if (existing != NULL) {
        /* Atualiza entrada existente */
        if (existing->module != module) {
            astm_free(existing->module);
            existing->module = module;
            module->ref_count++;
        }
        existing->source_mtime = get_file_mtime(key);
        existing->is_dirty = 1;
        lru_touch(cache, existing);
        return;
    }
    
    /* Evicta se necessário */
    while (cache->entry_count >= cache->max_entries) {
        evict_lru(cache);
    }
    
    /* Cria nova entrada */
    CacheEntry* entry = (CacheEntry*)calloc(1, sizeof(CacheEntry));
    if (entry == NULL) return;
    
    entry->key = strdup(key);
    entry->module = module;
    module->ref_count++;
    entry->source_mtime = get_file_mtime(key);
    entry->last_access = get_current_time();
    entry->hit_count = 0;
    entry->is_dirty = 0;
    
    /* Insere na hash table */
    uint32_t hash = hash_string(key);
    size_t bucket = hash % cache->bucket_count;
    entry->next = cache->buckets[bucket];
    cache->buckets[bucket] = entry;
    
    /* Insere na LRU list */
    lru_insert_front(cache, entry);
    
    cache->entry_count++;
}

void cache_invalidate(ModuleCache* cache, const char* key) {
    if (cache == NULL || key == NULL) return;
    
    CacheEntry* entry = cache_lookup(cache, key);
    if (entry == NULL) return;
    
    /* Remove da LRU */
    lru_remove(cache, entry);
    
    /* Remove da hash table */
    uint32_t hash = hash_string(key);
    size_t bucket = hash % cache->bucket_count;
    
    CacheEntry** pp = &cache->buckets[bucket];
    while (*pp != NULL) {
        if (*pp == entry) {
            *pp = entry->next;
            break;
        }
        pp = &(*pp)->next;
    }
    
    /* Libera */
    free(entry->key);
    if (entry->module) {
        astm_free(entry->module);
    }
    free(entry);
    
    cache->entry_count--;
}

void cache_clear(ModuleCache* cache) {
    if (cache == NULL) return;
    
    for (size_t i = 0; i < cache->bucket_count; i++) {
        CacheEntry* entry = cache->buckets[i];
        while (entry != NULL) {
            CacheEntry* next = entry->next;
            free(entry->key);
            if (entry->module) {
                astm_free(entry->module);
            }
            free(entry);
            entry = next;
        }
        cache->buckets[i] = NULL;
    }
    
    cache->entry_count = 0;
    cache->lru_head = NULL;
    cache->lru_tail = NULL;
    
    printf("[Cache] Cache limpo\n");
}

void cache_flush(ModuleCache* cache) {
    if (cache == NULL) return;
    
    int flushed = 0;
    
    for (size_t i = 0; i < cache->bucket_count; i++) {
        CacheEntry* entry = cache->buckets[i];
        while (entry != NULL) {
            if (entry->is_dirty && entry->module) {
                char* cache_path = get_cache_path(cache, entry->key);
                if (cache_path) {
                    astm_save(entry->module, cache_path);
                    free(cache_path);
                    entry->is_dirty = 0;
                    flushed++;
                }
            }
            entry = entry->next;
        }
    }
    
    if (flushed > 0) {
        printf("[Cache] Flush: %d módulos gravados em disco\n", flushed);
    }
}

void cache_load(ModuleCache* cache) {
    if (cache == NULL || cache->cache_dir == NULL) return;
    
    /* TODO: Carregar índice do cache do disco */
    printf("[Cache] Carregando cache de '%s'\n", cache->cache_dir);
}

int cache_is_valid(ModuleCache* cache, const char* key) {
    if (cache == NULL || key == NULL) return 0;
    
    CacheEntry* entry = cache_lookup(cache, key);
    if (entry == NULL) return 0;
    
    uint64_t current_mtime = get_file_mtime(key);
    if (current_mtime == 0) return 1; /* Fonte não existe, cache válido */
    
    return current_mtime <= entry->source_mtime;
}

void cache_print_stats(ModuleCache* cache) {
    if (cache == NULL) return;
    
    double hit_rate = 0;
    if (cache->hits + cache->misses > 0) {
        hit_rate = (double)cache->hits / (cache->hits + cache->misses) * 100;
    }
    
    printf("\n=== Estatísticas do Cache ===\n");
    printf("  Entradas: %zu / %zu\n", cache->entry_count, cache->max_entries);
    printf("  Hits: %zu\n", cache->hits);
    printf("  Misses: %zu\n", cache->misses);
    printf("  Hit Rate: %.1f%%\n", hit_rate);
    printf("  Evictions: %zu\n", cache->evictions);
    printf("  Diretório: %s\n", cache->cache_dir);
    printf("=============================\n\n");
}

/* =============================================================================
 * CACHE GLOBAL
 * ============================================================================= */

ModuleCache* cache_get_global(void) {
    if (g_global_cache == NULL) {
        cache_init_global(NULL);
    }
    return g_global_cache;
}

void cache_init_global(const char* cache_dir) {
    if (g_global_cache != NULL) return;
    g_global_cache = cache_create(cache_dir, CACHE_MAX_MEMORY_ENTRIES);
}

void cache_shutdown_global(void) {
    if (g_global_cache == NULL) return;
    cache_destroy(g_global_cache);
    g_global_cache = NULL;
}

/* =============================================================================
 * VERSIONAMENTO
 * ============================================================================= */

int version_compare(ModuleVersion a, ModuleVersion b) {
    if (a.major != b.major) return a.major - b.major;
    if (a.minor != b.minor) return a.minor - b.minor;
    if (a.patch != b.patch) return a.patch - b.patch;
    return a.build - b.build;
}

int version_compatible(ModuleVersion required, ModuleVersion provided) {
    /* Mesmo major, minor >= required, qualquer patch */
    if (provided.major != required.major) return 0;
    if (provided.minor < required.minor) return 0;
    if (provided.minor == required.minor && provided.patch < required.patch) return 0;
    return 1;
}

void version_to_string(ModuleVersion version, char* buffer, size_t size) {
    if (buffer == NULL || size == 0) return;
    
    if (version.build > 0) {
        snprintf(buffer, size, "%d.%d.%d+%d",
                 version.major, version.minor, version.patch, version.build);
    } else {
        snprintf(buffer, size, "%d.%d.%d",
                 version.major, version.minor, version.patch);
    }
}

ModuleVersion version_parse(const char* str) {
    ModuleVersion v = {0, 0, 0, 0};
    if (str == NULL) return v;
    
    sscanf(str, "%hu.%hu.%hu+%hu", &v.major, &v.minor, &v.patch, &v.build);
    return v;
}

