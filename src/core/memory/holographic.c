/**
 * =============================================================================
 * ASTERON HOLOGRAPHIC MEMORY (DVM) - IMPLEMENTAÇÃO
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "holographic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

// =============================================================================
// UTILITÁRIOS
// =============================================================================

static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static uint64_t hash_string(const char* str) {
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// =============================================================================
// GLOBAL ADDRESS
// =============================================================================

GlobalAddress global_address_create(const char* node_id, uint64_t local_addr) {
    GlobalAddress addr;
    addr.node_id = node_id ? strdup(node_id) : NULL;
    addr.local_addr = local_addr;
    
    // Gera ID global único (hash de node_id + local_addr)
    if (node_id) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%s:%llu", node_id, (unsigned long long)local_addr);
        addr.global_id = hash_string(buffer);
    } else {
        addr.global_id = local_addr;
    }
    
    return addr;
}

bool global_address_equal(GlobalAddress* a, GlobalAddress* b) {
    if (a == NULL || b == NULL) return false;
    if (a->global_id != b->global_id) return false;
    if (a->local_addr != b->local_addr) return false;
    if (a->node_id == NULL || b->node_id == NULL) {
        return a->node_id == b->node_id;
    }
    return strcmp(a->node_id, b->node_id) == 0;
}

char* global_address_serialize(GlobalAddress* addr) {
    if (addr == NULL) return NULL;
    
    char* buffer = (char*)malloc(256);
    if (buffer == NULL) return NULL;
    
    snprintf(buffer, 256, "%s:%llu:%llu",
             addr->node_id ? addr->node_id : "",
             (unsigned long long)addr->local_addr,
             (unsigned long long)addr->global_id);
    
    return buffer;
}

GlobalAddress global_address_deserialize(const char* data) {
    GlobalAddress addr = {0};
    if (data == NULL) return addr;
    
    char node_id[128];
    unsigned long long local_addr, global_id;
    
    if (sscanf(data, "%127[^:]:%llu:%llu", node_id, &local_addr, &global_id) == 3) {
        addr.node_id = strdup(node_id);
        addr.local_addr = (uint64_t)local_addr;
        addr.global_id = (uint64_t)global_id;
    }
    
    return addr;
}

// =============================================================================
// DVM (DISTRIBUTED VIRTUAL MACHINE)
// =============================================================================

DistributedVM* dvm_init(const char* local_node_id) {
    if (local_node_id == NULL) return NULL;
    
    DistributedVM* dvm = (DistributedVM*)calloc(1, sizeof(DistributedVM));
    if (dvm == NULL) return NULL;
    
    dvm->local_node_id = strdup(local_node_id);
    
    // Inicializa arrays
    dvm->ownership_capacity = 64;
    dvm->ownership_table = (DistributedOwnershipInfo*)calloc(
        dvm->ownership_capacity, sizeof(DistributedOwnershipInfo));
    
    dvm->request_capacity = 32;
    dvm->pending_requests = (BorrowRequest*)calloc(
        dvm->request_capacity, sizeof(BorrowRequest));
    
    dvm->cache_capacity = 128;
    dvm->remote_cache = (RemoteCacheEntry*)calloc(
        dvm->cache_capacity, sizeof(RemoteCacheEntry));
    
    printf("[DVM] Inicializado (nó: %s)\n", local_node_id);
    return dvm;
}

void dvm_destroy(DistributedVM* dvm) {
    if (dvm == NULL) return;
    
    // Libera ownership table
    for (size_t i = 0; i < dvm->ownership_count; i++) {
        free(dvm->ownership_table[i].address.node_id);
        free(dvm->ownership_table[i].owner_node_id);
        free(dvm->ownership_table[i].borrower_node_id);
    }
    free(dvm->ownership_table);
    
    // Libera requests
    for (size_t i = 0; i < dvm->request_count; i++) {
        free(dvm->pending_requests[i].requester_node_id);
    }
    free(dvm->pending_requests);
    
    // Libera cache
    for (size_t i = 0; i < dvm->cache_count; i++) {
        free(dvm->remote_cache[i].address.node_id);
        if (dvm->remote_cache[i].data) {
            free(dvm->remote_cache[i].data);
        }
    }
    free(dvm->remote_cache);
    
    free(dvm->local_node_id);
    free(dvm);
}

GlobalAddress dvm_register_object(DistributedVM* dvm, void* data, size_t size) {
    (void)size; // Não usado por enquanto
    if (dvm == NULL || data == NULL) {
        GlobalAddress empty = {0};
        return empty;
    }
    
    // Gera endereço local único (simplificado - em produção, usar pool de endereços)
    static uint64_t next_local_addr = 1;
    uint64_t local_addr = next_local_addr++;
    
    GlobalAddress addr = global_address_create(dvm->local_node_id, local_addr);
    
    // Expande array se necessário
    if (dvm->ownership_count >= dvm->ownership_capacity) {
        dvm->ownership_capacity *= 2;
        dvm->ownership_table = (DistributedOwnershipInfo*)realloc(
            dvm->ownership_table,
            sizeof(DistributedOwnershipInfo) * dvm->ownership_capacity);
    }
    
    // Registra ownership
    DistributedOwnershipInfo* info = &dvm->ownership_table[dvm->ownership_count++];
    info->address = addr;
    info->state = OWNER_LOCAL;
    info->owner_node_id = strdup(dvm->local_node_id);
    info->borrower_node_id = NULL;
    info->is_mutable = true;
    info->version = 1;
    info->last_modified_ns = get_timestamp_ns();
    
    printf("[DVM] Objeto registrado: %s:%llu\n",
           addr.node_id, (unsigned long long)addr.local_addr);
    
    return addr;
}

int dvm_borrow_remote(DistributedVM* dvm, GlobalAddress* address,
                      RemoteBorrowType type, char* target_node_id) {
    if (dvm == NULL || address == NULL || target_node_id == NULL) return 1;
    
    // Verifica se é nó local
    if (address->node_id && strcmp(address->node_id, dvm->local_node_id) == 0) {
        // É local - não precisa de borrowing remoto
        return 0;
    }
    
    // Cria requisição de borrowing
    if (dvm->request_count >= dvm->request_capacity) {
        dvm->request_capacity *= 2;
        dvm->pending_requests = (BorrowRequest*)realloc(
            dvm->pending_requests,
            sizeof(BorrowRequest) * dvm->request_capacity);
    }
    
    BorrowRequest* req = &dvm->pending_requests[dvm->request_count++];
    req->address = *address;
    req->type = type;
    req->requester_node_id = strdup(dvm->local_node_id);
    req->request_id = dvm->borrow_requests_sent++;
    req->timestamp_ns = get_timestamp_ns();
    req->granted = false;
    
    // Envia requisição (via callback de rede)
    if (dvm->send_borrow_request) {
        dvm->send_borrow_request(dvm, req);
    }
    
    printf("[DVM] Requisição de borrowing enviada: %s -> %s (tipo: %d)\n",
           dvm->local_node_id, target_node_id, type);
    
    return 0;
}

int dvm_return_borrowed(DistributedVM* dvm, GlobalAddress* address) {
    if (dvm == NULL || address == NULL) return 1;
    
    // Encontra ownership info
    for (size_t i = 0; i < dvm->ownership_count; i++) {
        DistributedOwnershipInfo* info = &dvm->ownership_table[i];
        if (global_address_equal(&info->address, address)) {
            if (info->state == OWNER_BORROWED_FROM) {
                // Retorna para nó remoto
                info->state = OWNER_LOCAL; // Volta para local (ou remove)
                
                // Envia notificação de retorno (via callback)
                if (dvm->send_borrow_response) {
                    BorrowRequest req = {0};
                    req.address = *address;
                    req.granted = false; // Retornando
                    dvm->send_borrow_response(dvm, &req, false);
                }
                
                printf("[DVM] Objeto retornado: %s:%llu\n",
                       address->node_id, (unsigned long long)address->local_addr);
                return 0;
            }
        }
    }
    
    return 1;
}

int dvm_move_to_remote(DistributedVM* dvm, GlobalAddress* address,
                       char* target_node_id) {
    if (dvm == NULL || address == NULL || target_node_id == NULL) return 1;
    
    // Encontra ownership info
    for (size_t i = 0; i < dvm->ownership_count; i++) {
        DistributedOwnershipInfo* info = &dvm->ownership_table[i];
        if (global_address_equal(&info->address, address)) {
            if (info->state == OWNER_LOCAL) {
                // Move ownership
                free(info->owner_node_id);
                info->owner_node_id = strdup(target_node_id);
                info->state = OWNER_REMOTE;
                info->is_mutable = false; // Não pode mais modificar localmente
                
                dvm->ownership_transfers++;
                
                printf("[DVM] Ownership movido: %s -> %s\n",
                       dvm->local_node_id, target_node_id);
                return 0;
            }
        }
    }
    
    return 1;
}

DistributedOwnershipInfo* dvm_get_ownership(DistributedVM* dvm,
                                             GlobalAddress* address) {
    if (dvm == NULL || address == NULL) return NULL;
    
    for (size_t i = 0; i < dvm->ownership_count; i++) {
        if (global_address_equal(&dvm->ownership_table[i].address, address)) {
            return &dvm->ownership_table[i];
        }
    }
    return NULL;
}

bool dvm_can_modify(DistributedVM* dvm, GlobalAddress* address) {
    if (dvm == NULL || address == NULL) return false;
    
    DistributedOwnershipInfo* info = dvm_get_ownership(dvm, address);
    if (info == NULL) return false;
    
    // Pode modificar se:
    // - É owner local
    // - Tem borrow mutável
    // - Não está emprestado para outro nó
    return (info->state == OWNER_LOCAL || 
            info->state == OWNER_BORROWED_FROM) &&
           info->is_mutable &&
           info->borrower_node_id == NULL;
}

// =============================================================================
// PERSISTENT MEMORY (PMEM)
// =============================================================================

PersistentMemoryManager* pmem_init(PersistentMemoryConfig* config,
                                    SnapshotManager* snapshot_mgr,
                                    ReactiveRuntime* reactive_rt) {
    if (config == NULL || snapshot_mgr == NULL) return NULL;
    
    PersistentMemoryManager* pmem = (PersistentMemoryManager*)calloc(
        1, sizeof(PersistentMemoryManager));
    if (pmem == NULL) return NULL;
    
    pmem->config = *config;
    pmem->snapshot_mgr = snapshot_mgr;
    pmem->reactive_rt = reactive_rt;
    
    // Inicializa arrays
    pmem->persistent_var_capacity = 64;
    pmem->persistent_vars = (PersistentVar*)calloc(
        pmem->persistent_var_capacity, sizeof(PersistentVar));
    
    pmem->snapshot_capacity = 32;
    pmem->persistent_snapshots = (uint64_t*)calloc(
        pmem->snapshot_capacity, sizeof(uint64_t));
    
    // Flags padrão
    pmem->auto_save_enabled = true;
    pmem->auto_save_interval_ns = 60000000000ULL; // 60 segundos
    
    printf("[PMEM] Inicializado (tipo: %d, device: %s)\n",
           config->type, config->device_path ? config->device_path : "N/A");
    
    return pmem;
}

void pmem_destroy(PersistentMemoryManager* pmem) {
    if (pmem == NULL) return;
    
    // Desmapeia se necessário
    if (pmem->config.is_mapped) {
        pmem_unmap(pmem);
    }
    
    // Libera arrays
    free(pmem->persistent_vars);
    free(pmem->persistent_snapshots);
    
    free(pmem);
}

int pmem_map(PersistentMemoryManager* pmem) {
    if (pmem == NULL || pmem->config.device_path == NULL) return 1;
    
    // Abre dispositivo
    int fd = open(pmem->config.device_path, O_RDWR);
    if (fd < 0) {
        perror("open pmem device");
        return 1;
    }
    
    // Mapeia em memória
    void* addr = mmap(NULL, pmem->config.size,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    
    close(fd);
    
    if (addr == MAP_FAILED) {
        perror("mmap pmem");
        return 1;
    }
    
    pmem->config.mapped_address = addr;
    pmem->config.is_mapped = true;
    
    printf("[PMEM] Mapeado: %p (size: %zu)\n", addr, pmem->config.size);
    return 0;
}

void pmem_unmap(PersistentMemoryManager* pmem) {
    if (pmem == NULL || !pmem->config.is_mapped) return;
    
    if (pmem->config.mapped_address) {
        munmap(pmem->config.mapped_address, pmem->config.size);
        pmem->config.mapped_address = NULL;
    }
    
    pmem->config.is_mapped = false;
}

int pmem_register_reactive(PersistentMemoryManager* pmem,
                            ReactiveNode* node,
                            const char* name) {
    if (pmem == NULL || node == NULL || name == NULL) return 1;
    
    // Expande array se necessário
    if (pmem->persistent_var_count >= pmem->persistent_var_capacity) {
        pmem->persistent_var_capacity *= 2;
        pmem->persistent_vars = (PersistentVar*)realloc(
            pmem->persistent_vars,
            sizeof(PersistentVar) * pmem->persistent_var_capacity);
    }
    
    // Registra variável
    PersistentVar* pvar = 
        &pmem->persistent_vars[pmem->persistent_var_count++];
    pvar->node = node;
    pvar->address = global_address_create("pmem", (uint64_t)node);
    pvar->last_saved_ns = 0;
    pvar->is_dirty = true;
    
    printf("[PMEM] Variável reativa registrada: %s\n", name);
    return 0;
}

int pmem_save_reactive(PersistentMemoryManager* pmem, ReactiveNode* node) {
    if (pmem == NULL || node == NULL || !pmem->config.is_mapped) return 1;
    
    // Encontra variável
    for (size_t i = 0; i < pmem->persistent_var_count; i++) {
        if (pmem->persistent_vars[i].node == node) {
            // Em produção, serializaria valor e gravaria em PMEM
            // Por enquanto, apenas marca como salvo
            pmem->persistent_vars[i].last_saved_ns = get_timestamp_ns();
            pmem->persistent_vars[i].is_dirty = false;
            
            pmem->saves_performed++;
            
            printf("[PMEM] Variável reativa salva: %s\n", node->name);
            return 0;
        }
    }
    
    return 1;
}

int pmem_restore_reactive(PersistentMemoryManager* pmem,
                          const char* name,
                          ReactiveNode** node) {
    if (pmem == NULL || name == NULL || node == NULL || !pmem->config.is_mapped) {
        return 1;
    }
    
    // Em produção, leria de PMEM e restauraria
    // Por enquanto, apenas placeholder
    
    pmem->restores_performed++;
    
    printf("[PMEM] Variável reativa restaurada: %s\n", name);
    return 0;
}

int pmem_save_snapshot(PersistentMemoryManager* pmem, uint64_t snapshot_id) {
    if (pmem == NULL || pmem->snapshot_mgr == NULL || !pmem->config.is_mapped) {
        return 1;
    }
    
    // Encontra snapshot
    CompleteSnapshot* snap = pmem->snapshot_mgr->snapshots;
    while (snap != NULL) {
        if (snap->snapshot_id == snapshot_id) {
            break;
        }
        snap = snap->next;
    }
    
    if (snap == NULL) return 1;
    
    // Em produção, serializaria snapshot e gravaria em PMEM
    // Por enquanto, apenas adiciona à lista
    
    if (pmem->snapshot_count >= pmem->snapshot_capacity) {
        pmem->snapshot_capacity *= 2;
        pmem->persistent_snapshots = (uint64_t*)realloc(
            pmem->persistent_snapshots,
            sizeof(uint64_t) * pmem->snapshot_capacity);
    }
    
    pmem->persistent_snapshots[pmem->snapshot_count++] = snapshot_id;
    
    pmem->saves_performed++;
    
    printf("[PMEM] Snapshot salvo em PMEM: %llu\n", (unsigned long long)snapshot_id);
    return 0;
}

int pmem_restore_snapshot(PersistentMemoryManager* pmem, uint64_t snapshot_id) {
    if (pmem == NULL || pmem->snapshot_mgr == NULL || !pmem->config.is_mapped) {
        return 1;
    }
    
    // Em produção, leria snapshot de PMEM e restauraria
    // Por enquanto, usa snapshot manager normal
    
    int result = snapshot_restore(pmem->snapshot_mgr, snapshot_id);
    
    if (result) {
        pmem->restores_performed++;
        printf("[PMEM] Snapshot restaurado de PMEM: %llu\n", 
               (unsigned long long)snapshot_id);
    }
    
    return result ? 0 : 1;
}

void pmem_process_auto_save(PersistentMemoryManager* pmem) {
    if (pmem == NULL || !pmem->auto_save_enabled) return;
    
    uint64_t now_ns = get_timestamp_ns();
    
    // Salva variáveis reativas sujas
    for (size_t i = 0; i < pmem->persistent_var_count; i++) {
        PersistentVar* pvar = &pmem->persistent_vars[i];
        
        if (pvar->is_dirty || 
            (now_ns - pvar->last_saved_ns) > pmem->auto_save_interval_ns) {
            pmem_save_reactive(pmem, pvar->node);
        }
    }
}

// =============================================================================
// UTILITÁRIOS
// =============================================================================

void dvm_get_stats(DistributedVM* dvm,
                    uint64_t* borrow_requests_sent,
                    uint64_t* borrow_requests_received,
                    uint64_t* objects_borrowed,
                    uint64_t* ownership_transfers) {
    if (dvm == NULL) return;
    
    if (borrow_requests_sent) *borrow_requests_sent = dvm->borrow_requests_sent;
    if (borrow_requests_received) *borrow_requests_received = dvm->borrow_requests_received;
    if (objects_borrowed) *objects_borrowed = dvm->objects_borrowed;
    if (ownership_transfers) *ownership_transfers = dvm->ownership_transfers;
}

void pmem_get_stats(PersistentMemoryManager* pmem,
                     uint64_t* saves,
                     uint64_t* restores,
                     size_t* bytes_written,
                     size_t* bytes_read) {
    if (pmem == NULL) return;
    
    if (saves) *saves = pmem->saves_performed;
    if (restores) *restores = pmem->restores_performed;
    if (bytes_written) *bytes_written = pmem->bytes_written;
    if (bytes_read) *bytes_read = pmem->bytes_read;
}

void dvm_print_info(DistributedVM* dvm) {
    if (dvm == NULL) return;
    
    printf("\n=== DVM Information ===\n");
    printf("Local Node: %s\n", dvm->local_node_id);
    printf("Ownership Entries: %zu\n", dvm->ownership_count);
    printf("Pending Requests: %zu\n", dvm->request_count);
    printf("Remote Cache: %zu\n", dvm->cache_count);
    printf("Borrow Requests Sent: %llu\n", 
           (unsigned long long)dvm->borrow_requests_sent);
    printf("Ownership Transfers: %llu\n",
           (unsigned long long)dvm->ownership_transfers);
    printf("========================\n\n");
}

void pmem_print_info(PersistentMemoryManager* pmem) {
    if (pmem == NULL) return;
    
    printf("\n=== PMEM Information ===\n");
    printf("Type: %d\n", pmem->config.type);
    printf("Device: %s\n", pmem->config.device_path ? pmem->config.device_path : "N/A");
    printf("Size: %zu bytes\n", pmem->config.size);
    printf("Mapped: %s\n", pmem->config.is_mapped ? "Yes" : "No");
    printf("Persistent Vars: %zu\n", pmem->persistent_var_count);
    printf("Persistent Snapshots: %zu\n", pmem->snapshot_count);
    printf("Saves: %llu\n", (unsigned long long)pmem->saves_performed);
    printf("Restores: %llu\n", (unsigned long long)pmem->restores_performed);
    printf("========================\n\n");
}

