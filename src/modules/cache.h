/**
 * =============================================================================
 * ASTERON MODULE CACHE v1.0
 * =============================================================================
 * 
 * Sistema de cache para módulos compilados.
 * Evita recompilação desnecessária e acelera carregamento.
 * 
 * Características:
 * - Cache em disco (.astm files)
 * - Cache em memória (LRU)
 * - Invalidação automática por mtime
 * - Suporte a dependências transitivas
 * 
 * =============================================================================
 */

#ifndef ASTERON_CACHE_H
#define ASTERON_CACHE_H

#include "astm.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * CONFIGURAÇÃO DO CACHE
 * ============================================================================= */

#define CACHE_MAX_MEMORY_ENTRIES    64      /* Máximo de módulos em memória */
#define CACHE_MAX_DISK_SIZE_MB      256     /* Tamanho máximo do cache em disco */
#define CACHE_DIR                   ".asteron_cache"

/* =============================================================================
 * VERSÃO DE MÓDULO
 * ============================================================================= */

typedef struct {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    uint16_t build;
} ModuleVersion;

#define MODULE_VERSION(maj, min, pat) \
    ((ModuleVersion){ .major = (maj), .minor = (min), .patch = (pat), .build = 0 })

#define MODULE_VERSION_COMPATIBLE(required, provided) \
    ((provided).major == (required).major && \
     ((provided).minor > (required).minor || \
      ((provided).minor == (required).minor && (provided).patch >= (required).patch)))

/* =============================================================================
 * ENTRADA DO CACHE
 * ============================================================================= */

typedef struct CacheEntry {
    char* key;                  /* Chave (path normalizado) */
    CompiledModule* module;     /* Módulo compilado */
    uint64_t last_access;       /* Timestamp do último acesso */
    uint64_t source_mtime;      /* Mtime do fonte quando compilado */
    int hit_count;              /* Número de hits */
    int is_dirty;               /* Precisa ser gravado em disco */
    struct CacheEntry* prev;    /* LRU: anterior */
    struct CacheEntry* next;    /* LRU: próximo */
} CacheEntry;

/* =============================================================================
 * CACHE DE MÓDULOS
 * ============================================================================= */

typedef struct {
    CacheEntry** buckets;       /* Hash table */
    size_t bucket_count;
    size_t entry_count;
    size_t max_entries;
    
    /* LRU list */
    CacheEntry* lru_head;       /* Mais recente */
    CacheEntry* lru_tail;       /* Menos recente */
    
    /* Estatísticas */
    size_t hits;
    size_t misses;
    size_t evictions;
    
    /* Diretório de cache */
    char* cache_dir;
    
    /* Controle de concorrência */
    int is_locked;
} ModuleCache;

/* =============================================================================
 * API DO CACHE
 * ============================================================================= */

/**
 * Cria cache de módulos
 */
ModuleCache* cache_create(const char* cache_dir, size_t max_entries);

/**
 * Destrói cache
 */
void cache_destroy(ModuleCache* cache);

/**
 * Obtém módulo do cache (ou compila se necessário)
 * @param cache Cache de módulos
 * @param source_path Caminho do arquivo fonte (.ast)
 * @return Módulo compilado (não liberar!)
 */
CompiledModule* cache_get(ModuleCache* cache, const char* source_path);

/**
 * Adiciona módulo ao cache
 */
void cache_put(ModuleCache* cache, const char* key, CompiledModule* module);

/**
 * Invalida entrada do cache
 */
void cache_invalidate(ModuleCache* cache, const char* key);

/**
 * Limpa todo o cache
 */
void cache_clear(ModuleCache* cache);

/**
 * Persiste cache em disco
 */
void cache_flush(ModuleCache* cache);

/**
 * Carrega cache do disco
 */
void cache_load(ModuleCache* cache);

/**
 * Verifica se entrada está válida (não expirada)
 */
int cache_is_valid(ModuleCache* cache, const char* key);

/**
 * Imprime estatísticas do cache
 */
void cache_print_stats(ModuleCache* cache);

/**
 * Obtém cache global
 */
ModuleCache* cache_get_global(void);

/**
 * Inicializa cache global
 */
void cache_init_global(const char* cache_dir);

/**
 * Finaliza cache global
 */
void cache_shutdown_global(void);

/* =============================================================================
 * API DE VERSIONAMENTO
 * ============================================================================= */

/**
 * Compara versões
 * @return <0 se a < b, 0 se a == b, >0 se a > b
 */
int version_compare(ModuleVersion a, ModuleVersion b);

/**
 * Verifica compatibilidade de versões
 * @return 1 se compatível, 0 se não
 */
int version_compatible(ModuleVersion required, ModuleVersion provided);

/**
 * Converte versão para string
 * @param version Versão
 * @param buffer Buffer de saída
 * @param size Tamanho do buffer
 */
void version_to_string(ModuleVersion version, char* buffer, size_t size);

/**
 * Parse versão de string
 * @param str String no formato "major.minor.patch"
 * @return Versão parseada
 */
ModuleVersion version_parse(const char* str);

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_CACHE_H */

