/**
 * =============================================================================
 * ASTERON HOLOGRAPHIC MEMORY (DVM) v1.0
 * =============================================================================
 * 
 * Memória Holográfica: Estende Ownership & Borrowing para fora da máquina física.
 * 
 * CONCEITO:
 *   - A memória do Nó A e do Nó B é vista como um espaço de endereçamento único
 *   - Sistema de Ownership garante que, se o Nó A "emprestar" um objeto para o
 *     Nó B via rede, o Nó A perde o direito de escrita até que o Nó B o devolva
 * 
 * COMPONENTES:
 * 
 * 1. DVM (Distributed Virtual Machine)
 *    - Espaço de endereçamento global único
 *    - Ownership distribuído entre nós
 *    - Borrowing via rede com garantias de segurança
 * 
 * 2. PERSISTENT MEMORY (PMEM) SUPPORT
 *    - Variáveis reativas sobrevivem a reinicializações
 *    - Grava estado diretamente em memórias não voláteis (NVMe/Optane)
 *    - Usa snapshots para persistência
 * 
 * =============================================================================
 */

#ifndef HOLOGRAPHIC_MEMORY_H
#define HOLOGRAPHIC_MEMORY_H

#include "ownership.h"
#include "../vm/snapshot.h"
#include "../../reactive/reactive.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// GLOBAL ADDRESS SPACE
// =============================================================================

// Endereço global único (combina nó + endereço local)
typedef struct {
    char* node_id;          // ID do nó (ex: "node-1", "server-2")
    uint64_t local_addr;    // Endereço local no nó
    uint64_t global_id;     // ID global único (hash de node_id + local_addr)
} GlobalAddress;

// Estado de ownership distribuído
typedef enum {
    OWNER_LOCAL,            // Ownership local (nó atual)
    OWNER_REMOTE,           // Ownership em nó remoto
    OWNER_BORROWED_REMOTE,  // Emprestado para nó remoto
    OWNER_BORROWED_FROM,    // Emprestado de nó remoto
    OWNER_PERSISTENT        // Persistido em PMEM
} DistributedOwnershipState;

// Informação de ownership distribuído
typedef struct {
    GlobalAddress address;              // Endereço global
    DistributedOwnershipState state;     // Estado atual
    char* owner_node_id;                // Nó que possui ownership
    char* borrower_node_id;             // Nó que está emprestando (se aplicável)
    bool is_mutable;                    // Pode ser modificado?
    uint64_t version;                   // Versão do objeto (para detecção de conflitos)
    uint64_t last_modified_ns;          // Timestamp da última modificação
} DistributedOwnershipInfo;

// =============================================================================
// DVM (DISTRIBUTED VIRTUAL MACHINE)
// =============================================================================

// Forward declaration
typedef struct DistributedVM DistributedVM;

// Operação de borrowing remoto
typedef enum {
    BORROW_READ,            // Empréstimo de leitura (múltiplos permitidos)
    BORROW_WRITE,           // Empréstimo de escrita (exclusivo)
    BORROW_MOVE             // Move ownership para nó remoto
} RemoteBorrowType;

// Requisição de borrowing
typedef struct {
    GlobalAddress address;              // Endereço do objeto
    RemoteBorrowType type;              // Tipo de borrowing
    char* requester_node_id;            // Nó que está pedindo
    uint64_t request_id;                // ID único da requisição
    uint64_t timestamp_ns;              // Timestamp da requisição
    bool granted;                       // Requisição foi concedida?
} BorrowRequest;

// Cache entry para objetos remotos
typedef struct RemoteCacheEntry {
    GlobalAddress address;
    void* data;
    size_t size;
    uint64_t cached_at_ns;
    uint64_t ttl_ns;                // Time-to-live
} RemoteCacheEntry;

// DVM Runtime
typedef struct DistributedVM {
    char* local_node_id;                // ID do nó local
    
    // Tabela de ownership distribuído
    DistributedOwnershipInfo* ownership_table;
    size_t ownership_count;
    size_t ownership_capacity;
    
    // Requisições de borrowing pendentes
    BorrowRequest* pending_requests;
    size_t request_count;
    size_t request_capacity;
    
    // Cache de objetos remotos
    RemoteCacheEntry* remote_cache;
    size_t cache_count;
    size_t cache_capacity;
    
    // Callbacks de rede
    int (*send_borrow_request)(DistributedVM* dvm, BorrowRequest* req);
    int (*send_borrow_response)(DistributedVM* dvm, BorrowRequest* req, bool granted);
    int (*send_object_data)(DistributedVM* dvm, GlobalAddress* addr, void* data, size_t size);
    
    // Estatísticas
    uint64_t borrow_requests_sent;
    uint64_t borrow_requests_received;
    uint64_t objects_borrowed;
    uint64_t ownership_transfers;
} DistributedVM;

// =============================================================================
// PERSISTENT MEMORY (PMEM)
// =============================================================================

// Tipo de memória persistente
typedef enum {
    PMEM_NVME,              // NVMe SSD
    PMEM_OPTANE,            // Intel Optane
    PMEM_FILE,              // Arquivo mapeado
    PMEM_CUSTOM             // Implementação customizada
} PersistentMemoryType;

// Configuração de PMEM
typedef struct {
    PersistentMemoryType type;          // Tipo de PMEM
    const char* device_path;             // Caminho do dispositivo (ex: "/dev/pmem0")
    size_t size;                        // Tamanho em bytes
    size_t alignment;                   // Alinhamento requerido
    bool is_mapped;                     // Está mapeado em memória?
    void* mapped_address;               // Endereço mapeado (se is_mapped)
} PersistentMemoryConfig;

// Variável reativa persistente
typedef struct PersistentVar {
    ReactiveNode* node;              // Nó reativo
    GlobalAddress address;          // Endereço global
    uint64_t last_saved_ns;         // Última vez que foi salvo
    bool is_dirty;                  // Precisa ser salvo?
} PersistentVar;

// Gerenciador de PMEM
typedef struct {
    PersistentMemoryConfig config;      // Configuração
    SnapshotManager* snapshot_mgr;      // Gerenciador de snapshots
    ReactiveRuntime* reactive_rt;       // Runtime reativo (para variáveis reativas)
    
    // Variáveis reativas persistentes
    PersistentVar* persistent_vars;
    size_t persistent_var_count;
    size_t persistent_var_capacity;
    
    // Snapshots persistentes
    uint64_t* persistent_snapshots;      // IDs de snapshots persistentes
    size_t snapshot_count;
    size_t snapshot_capacity;
    
    // Flags
    bool auto_save_enabled;              // Salva automaticamente?
    uint64_t auto_save_interval_ns;     // Intervalo de auto-save
    
    // Estatísticas
    uint64_t saves_performed;
    uint64_t restores_performed;
    size_t bytes_written;
    size_t bytes_read;
} PersistentMemoryManager;

// =============================================================================
// API PÚBLICA - DVM
// =============================================================================

// Inicializa DVM
DistributedVM* dvm_init(const char* local_node_id);

// Destrói DVM
void dvm_destroy(DistributedVM* dvm);

// Registra objeto no espaço de endereçamento global
GlobalAddress dvm_register_object(DistributedVM* dvm, void* data, size_t size);

// Solicita borrowing de objeto remoto
int dvm_borrow_remote(DistributedVM* dvm, GlobalAddress* address, 
                      RemoteBorrowType type, char* target_node_id);

// Retorna objeto emprestado
int dvm_return_borrowed(DistributedVM* dvm, GlobalAddress* address);

// Move ownership para nó remoto
int dvm_move_to_remote(DistributedVM* dvm, GlobalAddress* address, 
                       char* target_node_id);

// Obtém informação de ownership
DistributedOwnershipInfo* dvm_get_ownership(DistributedVM* dvm, 
                                             GlobalAddress* address);

// Verifica se pode modificar objeto
bool dvm_can_modify(DistributedVM* dvm, GlobalAddress* address);

// =============================================================================
// API PÚBLICA - PERSISTENT MEMORY
// =============================================================================

// Inicializa gerenciador de PMEM
PersistentMemoryManager* pmem_init(PersistentMemoryConfig* config,
                                    SnapshotManager* snapshot_mgr,
                                    ReactiveRuntime* reactive_rt);

// Destrói gerenciador de PMEM
void pmem_destroy(PersistentMemoryManager* pmem);

// Mapeia memória persistente
int pmem_map(PersistentMemoryManager* pmem);

// Desmapeia memória persistente
void pmem_unmap(PersistentMemoryManager* pmem);

// Registra variável reativa como persistente
int pmem_register_reactive(PersistentMemoryManager* pmem, 
                            ReactiveNode* node,
                            const char* name);

// Salva estado de variável reativa em PMEM
int pmem_save_reactive(PersistentMemoryManager* pmem, ReactiveNode* node);

// Restaura variável reativa de PMEM
int pmem_restore_reactive(PersistentMemoryManager* pmem, 
                          const char* name,
                          ReactiveNode** node);

// Salva snapshot em PMEM
int pmem_save_snapshot(PersistentMemoryManager* pmem, uint64_t snapshot_id);

// Restaura snapshot de PMEM
int pmem_restore_snapshot(PersistentMemoryManager* pmem, uint64_t snapshot_id);

// Processa auto-save (chamado periodicamente)
void pmem_process_auto_save(PersistentMemoryManager* pmem);

// =============================================================================
// UTILITÁRIOS
// =============================================================================

// Cria endereço global
GlobalAddress global_address_create(const char* node_id, uint64_t local_addr);

// Compara endereços globais
bool global_address_equal(GlobalAddress* a, GlobalAddress* b);

// Serializa endereço global
char* global_address_serialize(GlobalAddress* addr);

// Deserializa endereço global
GlobalAddress global_address_deserialize(const char* data);

// Obtém estatísticas do DVM
void dvm_get_stats(DistributedVM* dvm,
                    uint64_t* borrow_requests_sent,
                    uint64_t* borrow_requests_received,
                    uint64_t* objects_borrowed,
                    uint64_t* ownership_transfers);

// Obtém estatísticas do PMEM
void pmem_get_stats(PersistentMemoryManager* pmem,
                     uint64_t* saves,
                     uint64_t* restores,
                     size_t* bytes_written,
                     size_t* bytes_read);

// Imprime informações do DVM
void dvm_print_info(DistributedVM* dvm);

// Imprime informações do PMEM
void pmem_print_info(PersistentMemoryManager* pmem);

#ifdef __cplusplus
}
#endif

#endif // HOLOGRAPHIC_MEMORY_H

