/**
 * =============================================================================
 * ASTERON OWNERSHIP SYSTEM v1.0
 * =============================================================================
 * 
 * Sistema de ownership formal inspirado em Rust, mas com semântica própria.
 * 
 * REGRAS FUNDAMENTAIS:
 * 
 * 1. OWNERSHIP ÚNICO
 *    - Cada valor tem exatamente um "dono"
 *    - Quando o dono sai do escopo, o valor é liberado
 * 
 * 2. BORROWING
 *    - Referências imutáveis (&T): múltiplas permitidas
 *    - Referência mutável (&mut T): exclusiva
 *    - Não pode ter &mut enquanto existir &
 * 
 * 3. LIFETIMES
 *    - Toda referência tem um lifetime implícito ou explícito
 *    - Referência não pode viver mais que o dado referenciado
 * 
 * 4. MOVE vs COPY
 *    - Tipos Copy: são copiados automaticamente (primitivos)
 *    - Tipos Move: ownership é transferido (heap-allocated)
 * 
 * =============================================================================
 */

#ifndef ASTERON_OWNERSHIP_H
#define ASTERON_OWNERSHIP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * TIPOS DE OWNERSHIP
 * ============================================================================= */

typedef enum {
    OWNER_OWNED,        /* Ownership completo */
    OWNER_BORROWED,     /* Empréstimo imutável (&T) */
    OWNER_BORROWED_MUT, /* Empréstimo mutável (&mut T) */
    OWNER_MOVED,        /* Ownership foi transferido */
    OWNER_DROPPED       /* Valor foi liberado */
} OwnershipState;

typedef enum {
    COPY_TYPE_NONE,     /* Não é Copy (move semantics) */
    COPY_TYPE_TRIVIAL,  /* Copy trivial (memcpy) */
    COPY_TYPE_CLONE     /* Precisa de Clone explícito */
} CopyType;

/* =============================================================================
 * LIFETIME
 * ============================================================================= */

typedef uint32_t LifetimeId;

#define LIFETIME_STATIC     0xFFFFFFFF  /* 'static - vive para sempre */
#define LIFETIME_TEMPORARY  0x00000000  /* Temporário (expressão) */

typedef struct {
    LifetimeId id;
    const char* name;       /* Nome opcional ('a, 'b, etc) */
    uint32_t scope_depth;   /* Profundidade do escopo */
    uint32_t start_line;    /* Linha de início */
    uint32_t end_line;      /* Linha de fim (estimada) */
} Lifetime;

/* =============================================================================
 * INFORMAÇÃO DE OWNERSHIP
 * ============================================================================= */

typedef struct {
    OwnershipState state;
    LifetimeId lifetime;
    
    /* Para borrowed */
    uint32_t borrow_count;      /* Número de borrows ativos */
    bool is_mut_borrowed;       /* Tem borrow mutável? */
    
    /* Para rastreamento */
    uint32_t owner_id;          /* ID do dono original */
    uint32_t moved_to;          /* Para onde foi movido (se MOVED) */
    
    /* Localização no código */
    uint32_t def_line;          /* Linha de definição */
    uint32_t last_use_line;     /* Última linha de uso */
} OwnershipInfo;

/* =============================================================================
 * TIPO COM INFORMAÇÃO DE MEMÓRIA
 * ============================================================================= */

typedef enum {
    MEM_STACK,          /* Alocado na stack */
    MEM_HEAP,           /* Alocado no heap (Box<T>) */
    MEM_STATIC,         /* Dados estáticos */
    MEM_REGISTER        /* Em registrador (otimização) */
} MemoryLocation;

typedef struct {
    size_t size;            /* Tamanho em bytes */
    size_t alignment;       /* Alinhamento requerido */
    MemoryLocation location;/* Onde está alocado */
    CopyType copy_type;     /* Como copiar */
    bool is_zero_sized;     /* Zero-sized type (ZST) */
    bool needs_drop;        /* Precisa de destrutor */
} MemoryLayout;

/* =============================================================================
 * REGIÃO DE BORROW
 * ============================================================================= */

typedef struct BorrowRegion {
    uint32_t var_id;            /* Variável emprestada */
    OwnershipState borrow_type; /* BORROWED ou BORROWED_MUT */
    LifetimeId lifetime;        /* Lifetime do borrow */
    uint32_t start_line;
    uint32_t end_line;
    struct BorrowRegion* next;
} BorrowRegion;

/* =============================================================================
 * BORROW CHECKER
 * ============================================================================= */

typedef struct {
    /* Tabela de variáveis e seus estados */
    struct {
        char* name;
        uint32_t id;
        OwnershipInfo ownership;
        MemoryLayout layout;
    }* variables;
    size_t var_count;
    size_t var_capacity;
    
    /* Lifetimes ativos */
    Lifetime* lifetimes;
    size_t lifetime_count;
    size_t lifetime_capacity;
    LifetimeId next_lifetime_id;
    
    /* Regiões de borrow ativas */
    BorrowRegion* active_borrows;
    
    /* Escopo atual */
    uint32_t current_scope;
    uint32_t current_line;
    
    /* Erros encontrados */
    struct {
        uint32_t line;
        char* message;
    }* errors;
    size_t error_count;
    size_t error_capacity;
} BorrowChecker;

/* =============================================================================
 * ERROS DE OWNERSHIP
 * ============================================================================= */

typedef enum {
    OWN_ERR_NONE = 0,
    OWN_ERR_USE_AFTER_MOVE,         /* Uso após move */
    OWN_ERR_DOUBLE_FREE,            /* Liberação dupla */
    OWN_ERR_BORROW_WHILE_MUT,       /* Borrow enquanto mut borrow ativo */
    OWN_ERR_MUT_BORROW_WHILE_BORROW,/* Mut borrow enquanto borrow ativo */
    OWN_ERR_OUTLIVES,               /* Referência vive mais que dado */
    OWN_ERR_DANGLING_REF,           /* Referência pendente */
    OWN_ERR_MOVE_WHILE_BORROWED,    /* Move enquanto borrowed */
    OWN_ERR_MODIFY_WHILE_BORROWED   /* Modificação enquanto borrowed */
} OwnershipError;

/* =============================================================================
 * API DO BORROW CHECKER
 * ============================================================================= */

/**
 * Cria um novo borrow checker
 */
BorrowChecker* borrow_checker_create(void);

/**
 * Destrói borrow checker
 */
void borrow_checker_destroy(BorrowChecker* bc);

/**
 * Registra nova variável
 */
uint32_t borrow_checker_add_var(BorrowChecker* bc, const char* name,
                                 MemoryLayout layout);

/**
 * Registra um borrow imutável
 * @return OWN_ERR_NONE se válido
 */
OwnershipError borrow_checker_borrow(BorrowChecker* bc, uint32_t var_id,
                                      LifetimeId lifetime);

/**
 * Registra um borrow mutável
 * @return OWN_ERR_NONE se válido
 */
OwnershipError borrow_checker_borrow_mut(BorrowChecker* bc, uint32_t var_id,
                                          LifetimeId lifetime);

/**
 * Registra um move
 * @return OWN_ERR_NONE se válido
 */
OwnershipError borrow_checker_move(BorrowChecker* bc, uint32_t from_var,
                                    uint32_t to_var);

/**
 * Registra um drop (fim do escopo)
 */
OwnershipError borrow_checker_drop(BorrowChecker* bc, uint32_t var_id);

/**
 * Libera um borrow
 */
void borrow_checker_unborrow(BorrowChecker* bc, uint32_t var_id);

/**
 * Entra em novo escopo
 */
void borrow_checker_enter_scope(BorrowChecker* bc);

/**
 * Sai do escopo atual (dropa variáveis locais)
 */
void borrow_checker_exit_scope(BorrowChecker* bc);

/**
 * Cria novo lifetime
 */
LifetimeId borrow_checker_new_lifetime(BorrowChecker* bc, const char* name);

/**
 * Verifica se um lifetime é válido em outro
 */
bool borrow_checker_lifetime_valid(BorrowChecker* bc, LifetimeId inner,
                                    LifetimeId outer);

/**
 * Verifica programa inteiro após parsing
 */
bool borrow_checker_verify(BorrowChecker* bc);

/**
 * Obtém mensagem de erro
 */
const char* ownership_error_message(OwnershipError err);

/**
 * Imprime estado do borrow checker (debug)
 */
void borrow_checker_print_state(BorrowChecker* bc);

/* =============================================================================
 * TIPOS DE SISTEMA (LOW-LEVEL)
 * ============================================================================= */

/* Tipos primitivos com tamanho explícito */
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float    f32;
typedef double   f64;
typedef size_t   usize;
typedef ptrdiff_t isize;

/* Ponteiro raw (unsafe) */
typedef void* RawPtr;
typedef const void* RawPtrConst;

/* =============================================================================
 * BOX<T> - Smart Pointer com ownership
 * ============================================================================= */

typedef struct {
    void* ptr;              /* Ponteiro para dados no heap */
    size_t size;            /* Tamanho alocado */
    void (*drop)(void*);    /* Função destrutor (opcional) */
} Box;

/**
 * Aloca Box<T>
 */
Box box_new(size_t size);

/**
 * Libera Box<T>
 */
void box_drop(Box* box);

/**
 * Obtém referência ao conteúdo
 */
void* box_deref(Box* box);

/**
 * Obtém referência mutável ao conteúdo
 */
void* box_deref_mut(Box* box);

/* =============================================================================
 * SLICE - Visão de array
 * ============================================================================= */

typedef struct {
    void* ptr;          /* Ponteiro para início */
    size_t len;         /* Número de elementos */
    size_t elem_size;   /* Tamanho de cada elemento */
} Slice;

/**
 * Cria slice de array
 */
Slice slice_from_array(void* arr, size_t len, size_t elem_size);

/**
 * Obtém elemento do slice
 */
void* slice_get(Slice* s, size_t index);

/**
 * Obtém sub-slice
 */
Slice slice_sub(Slice* s, size_t start, size_t end);

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_OWNERSHIP_H */

