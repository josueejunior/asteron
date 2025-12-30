/**
 * =============================================================================
 * ASTERON REGION-BASED MEMORY + ZERO-COPY v2.0
 * =============================================================================
 * 
 * Sistema unificado de memória que combina:
 * 
 * 1. REGION-BASED MEMORY
 *    - Arenas/Regions para tarefas específicas (ex: requisição HTTP)
 *    - Desalocação em massa quando a tarefa termina
 *    - Reduz overhead do Reference Counting
 * 
 * 2. ZERO-COPY INTEGRATION
 *    - Módulos nativos (Net, FS) leem diretamente para memória da VM
 *    - Sem buffers intermediários
 *    - Máxima performance para I/O
 * 
 * 3. OWNERSHIP INTEGRATION
 *    - Integra com sistema de Ownership existente
 *    - Regions têm lifetime explícito
 *    - Valores podem ser "moved" para regions
 * 
 * =============================================================================
 */

#ifndef REGION_MEMORY_H
#define REGION_MEMORY_H

#include "../interpreter/interpreter.h"
#include "ownership.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// MEMORY REGION
// =============================================================================

// Tipo de região
typedef enum {
    REGION_TASK,        // Região para uma task específica
    REGION_REQUEST,     // Região para uma requisição HTTP
    REGION_LOOP,        // Região para iterações de loop
    REGION_FUNCTION,    // Região para execução de função
    REGION_TEMPORARY    // Região temporária (curta duração)
} RegionType;

// Estado da região
typedef enum {
    REGION_ACTIVE,     // Região ativa (em uso)
    REGION_FROZEN,     // Região congelada (read-only)
    REGION_DESTROYED   // Região destruída
} RegionState;

// Estrutura de uma região de memória
typedef struct MemoryRegion {
    // Identificação
    uint64_t id;                   // ID único da região
    RegionType type;               // Tipo da região
    const char* name;              // Nome opcional (para debug)
    
    // Estado
    RegionState state;              // Estado atual
    LifetimeId lifetime;            // Lifetime da região
    
    // Memória
    void* memory;                   // Bloco de memória alocado
    size_t size;                    // Tamanho usado
    size_t capacity;                // Capacidade total
    size_t alignment;               // Alinhamento (padrão: sizeof(void*))
    
    // Rastreamento de objetos
    struct {
        Value* values;              // Valores alocados nesta região
        size_t count;                // Número de valores
        size_t capacity;             // Capacidade do array
    } tracked_values;
    
    // Ownership tracking
    struct {
        uint32_t* var_ids;           // IDs de variáveis nesta região
        size_t count;
        size_t capacity;
    } owned_vars;
    
    // Referências a outras regiões
    struct MemoryRegion** dependencies; // Regiões que esta depende
    size_t dep_count;
    size_t dep_capacity;
    
    // Estatísticas
    size_t allocations;             // Número de alocações
    size_t peak_size;                // Tamanho máximo usado
    uint64_t created_at;            // Timestamp de criação
    uint64_t destroyed_at;           // Timestamp de destruição
    
    // Callbacks
    void (*on_destroy)(struct MemoryRegion*); // Callback de destruição
    
    // Lista encadeada
    struct MemoryRegion* next;
    struct MemoryRegion* prev;
} MemoryRegion;

// =============================================================================
// REGION MANAGER
// =============================================================================

// Gerenciador global de regiões
typedef struct {
    MemoryRegion* regions;          // Lista de regiões ativas
    MemoryRegion* current_region;   // Região atual (para alocações)
    uint64_t next_region_id;        // Próximo ID de região
    size_t total_regions;           // Total de regiões criadas
    size_t active_regions;          // Regiões ativas
    size_t total_allocated;          // Total de memória alocada
} RegionManager;

// =============================================================================
// ZERO-COPY BUFFER
// =============================================================================

// Buffer zero-copy que aponta diretamente para memória da VM
typedef struct {
    void* data;                     // Dados (pode ser de região ou externo)
    size_t size;                    // Tamanho em bytes
    bool is_region_owned;           // true se pertence a uma região
    MemoryRegion* region;           // Região que possui (se is_region_owned)
    bool is_external;               // true se é memória externa (zero-copy)
    void (*free_fn)(void*);         // Função para liberar (se externo)
} ZeroCopyBuffer;

// =============================================================================
// API PÚBLICA
// =============================================================================

// Inicializa sistema de regiões
void region_memory_init(void);

// Limpa sistema de regiões
void region_memory_cleanup(void);

// Cria nova região de memória
MemoryRegion* region_create(RegionType type, size_t capacity, const char* name);

// Destrói região (libera toda memória de uma vez)
void region_destroy(MemoryRegion* region);

// Define região atual (para alocações)
void region_set_current(MemoryRegion* region);

// Obtém região atual
MemoryRegion* region_get_current(void);

// Aloca memória na região atual
void* region_alloc(MemoryRegion* region, size_t size);

// Aloca memória alinhada na região atual
void* region_alloc_aligned(MemoryRegion* region, size_t size, size_t alignment);

// Rastreia um Value na região (para desalocação automática)
void region_track_value(MemoryRegion* region, Value value);

// Move variável para região (ownership transfer)
int region_move_var(MemoryRegion* region, uint32_t var_id, BorrowChecker* bc);

// Congela região (torna read-only)
void region_freeze(MemoryRegion* region);

// Descongela região (permite escrita novamente)
void region_unfreeze(MemoryRegion* region);

// Obtém estatísticas da região
void region_get_stats(MemoryRegion* region, size_t* used, size_t* peak, 
                      size_t* allocations);

// =============================================================================
// ZERO-COPY API
// =============================================================================

// Cria buffer zero-copy a partir de dados externos
ZeroCopyBuffer* zerocopy_create_external(void* data, size_t size, 
                                         void (*free_fn)(void*));

// Cria buffer zero-copy a partir de região
ZeroCopyBuffer* zerocopy_create_from_region(MemoryRegion* region, 
                                             void* data, size_t size);

// Cria buffer zero-copy para leitura direta (para módulos nativos)
ZeroCopyBuffer* zerocopy_create_for_read(MemoryRegion* region, size_t size);

// Libera buffer zero-copy
void zerocopy_destroy(ZeroCopyBuffer* buffer);

// Obtém ponteiro para dados (para leitura/escrita direta)
void* zerocopy_get_data(ZeroCopyBuffer* buffer);

// Obtém tamanho do buffer
size_t zerocopy_get_size(ZeroCopyBuffer* buffer);

// =============================================================================
// INTEGRAÇÃO COM MÓDULOS NATIVOS
// =============================================================================

// Prepara região para receber dados de módulo nativo (zero-copy)
MemoryRegion* region_prepare_for_io(size_t expected_size, const char* name);

// Finaliza I/O e retorna Value (sem cópia)
Value region_finish_io(MemoryRegion* region, ZeroCopyBuffer* buffer);

// =============================================================================
// UTILITÁRIOS
// =============================================================================

// Imprime estatísticas de todas as regiões
void region_print_stats(void);

// Obtém região por ID
MemoryRegion* region_get_by_id(uint64_t id);

// Obtém região por nome
MemoryRegion* region_get_by_name(const char* name);

// Destrói todas as regiões de um tipo
void region_destroy_all(RegionType type);

#ifdef __cplusplus
}
#endif

#endif // REGION_MEMORY_H

