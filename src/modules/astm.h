/**
 * =============================================================================
 * ASTERON MODULE FORMAT (.astm) v1.0
 * =============================================================================
 * 
 * Formato binário para módulos Asteron compilados.
 * 
 * Estrutura do arquivo:
 * +------------------+
 * | Header (32 bytes)|
 * +------------------+
 * | Symbol Table     |
 * +------------------+
 * | Bytecode         |
 * +------------------+
 * | Constants        |
 * +------------------+
 * | Metadata         |
 * +------------------+
 * 
 * =============================================================================
 */

#ifndef ASTERON_ASTM_H
#define ASTERON_ASTM_H

#include "../core/abi.h"
#include "../core/vm/vm.h"
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * MAGIC NUMBER E VERSÃO
 * ============================================================================= */

#define ASTM_MAGIC          0x4D545341  /* "ASTM" em little-endian */
#define ASTM_VERSION_MAJOR  1
#define ASTM_VERSION_MINOR  0

/* =============================================================================
 * FLAGS DO MÓDULO
 * ============================================================================= */

#define ASTM_FLAG_NONE          0x0000
#define ASTM_FLAG_DEBUG         0x0001  /* Contém info de debug */
#define ASTM_FLAG_OPTIMIZED     0x0002  /* Foi otimizado */
#define ASTM_FLAG_JIT_READY     0x0004  /* Pronto para JIT */
#define ASTM_FLAG_NATIVE        0x0008  /* Contém código nativo */
#define ASTM_FLAG_COMPRESSED    0x0010  /* Bytecode comprimido */

/* =============================================================================
 * HEADER DO ARQUIVO .astm
 * ============================================================================= */

typedef struct {
    uint32_t magic;             /* ASTM_MAGIC */
    uint16_t version_major;     /* Versão major do formato */
    uint16_t version_minor;     /* Versão minor do formato */
    uint32_t flags;             /* ASTM_FLAG_* */
    uint32_t checksum;          /* CRC32 do conteúdo */
    
    /* Offsets e tamanhos das seções */
    uint32_t symbol_offset;     /* Offset da tabela de símbolos */
    uint32_t symbol_count;      /* Número de símbolos */
    uint32_t bytecode_offset;   /* Offset do bytecode */
    uint32_t bytecode_size;     /* Tamanho do bytecode em bytes */
    uint32_t const_offset;      /* Offset das constantes */
    uint32_t const_count;       /* Número de constantes */
    uint32_t meta_offset;       /* Offset dos metadados */
    uint32_t meta_size;         /* Tamanho dos metadados */
    
    /* Reservado para expansão futura */
    uint32_t reserved[4];
} AstmHeader;

/* =============================================================================
 * TIPOS DE SÍMBOLOS
 * ============================================================================= */

typedef enum {
    SYMBOL_FUNCTION     = 0x01,
    SYMBOL_VARIABLE     = 0x02,
    SYMBOL_CONSTANT     = 0x03,
    SYMBOL_TYPE         = 0x04,
    SYMBOL_MODULE       = 0x05,
    SYMBOL_NATIVE       = 0x06
} SymbolType;

typedef enum {
    SYMBOL_VIS_PUBLIC   = 0x00,  /* Exportado */
    SYMBOL_VIS_PRIVATE  = 0x01,  /* Interno */
    SYMBOL_VIS_PROTECTED= 0x02   /* Submodulos */
} SymbolVisibility;

/* =============================================================================
 * ENTRADA NA TABELA DE SÍMBOLOS
 * ============================================================================= */

typedef struct {
    uint32_t name_offset;       /* Offset do nome na string table */
    uint16_t name_length;       /* Tamanho do nome */
    uint8_t  type;              /* SymbolType */
    uint8_t  visibility;        /* SymbolVisibility */
    uint32_t value_offset;      /* Offset do valor/bytecode */
    uint32_t value_size;        /* Tamanho do valor */
    uint16_t arity;             /* Aridade (para funções) */
    uint16_t flags;             /* Flags adicionais */
    uint32_t type_signature;    /* Hash da assinatura de tipo */
} AstmSymbol;

/* =============================================================================
 * TABELA DE SÍMBOLOS EM MEMÓRIA
 * ============================================================================= */

typedef struct SymbolEntry {
    char* name;                 /* Nome do símbolo */
    SymbolType type;            /* Tipo */
    SymbolVisibility visibility;/* Visibilidade */
    
    union {
        struct {
            size_t bytecode_start;  /* Início no bytecode */
            size_t bytecode_end;    /* Fim no bytecode */
            int arity;              /* Número de parâmetros */
            char** param_names;     /* Nomes dos parâmetros */
            int is_exported;        /* Exportado? */
        } function;
        
        struct {
            AsteronValue value;     /* Valor da constante */
        } constant;
        
        struct {
            size_t slot;            /* Slot na tabela de variáveis */
        } variable;
    } as;
    
    struct SymbolEntry* next;   /* Lista encadeada */
} SymbolEntry;

typedef struct {
    SymbolEntry** buckets;      /* Hash table */
    size_t bucket_count;        /* Número de buckets */
    size_t entry_count;         /* Número total de entradas */
    
    /* Exports */
    SymbolEntry** exports;      /* Array de símbolos exportados */
    size_t export_count;
    size_t export_capacity;
    
    /* Imports */
    struct {
        char* module_name;      /* Nome do módulo */
        char** symbols;         /* Símbolos importados */
        size_t symbol_count;
    }* imports;
    size_t import_count;
    size_t import_capacity;
} SymbolTable;

/* =============================================================================
 * MÓDULO COMPILADO EM MEMÓRIA
 * ============================================================================= */

typedef struct CompiledModule {
    char* name;                 /* Nome do módulo */
    char* path;                 /* Caminho do arquivo .astm */
    uint32_t version;           /* Versão do módulo */
    uint32_t flags;             /* Flags */
    
    SymbolTable* symbols;       /* Tabela de símbolos */
    BytecodeProgram* bytecode;  /* Bytecode compilado */
    
    /* Cache de JIT */
    void* jit_cache;            /* Código JIT compilado */
    int jit_valid;              /* JIT válido? */
    
    /* Dependências */
    struct CompiledModule** deps;   /* Módulos dependentes */
    size_t dep_count;
    
    /* Timestamps para cache */
    uint64_t source_mtime;      /* Mtime do arquivo fonte */
    uint64_t compile_time;      /* Quando foi compilado */
    
    /* Reference counting */
    int ref_count;
} CompiledModule;

/* =============================================================================
 * API DA TABELA DE SÍMBOLOS
 * ============================================================================= */

/**
 * Cria uma nova tabela de símbolos
 */
SymbolTable* symbol_table_create(void);

/**
 * Destrói tabela de símbolos
 */
void symbol_table_destroy(SymbolTable* table);

/**
 * Adiciona símbolo à tabela
 */
SymbolEntry* symbol_table_add(SymbolTable* table, const char* name, 
                               SymbolType type, SymbolVisibility vis);

/**
 * Busca símbolo pelo nome
 */
SymbolEntry* symbol_table_lookup(SymbolTable* table, const char* name);

/**
 * Marca símbolo como exportado
 */
void symbol_table_export(SymbolTable* table, const char* name);

/**
 * Registra import de outro módulo
 */
void symbol_table_add_import(SymbolTable* table, const char* module_name,
                              const char* symbol_name);

/**
 * Imprime tabela de símbolos (debug)
 */
void symbol_table_print(SymbolTable* table);

/* =============================================================================
 * API DO FORMATO .astm
 * ============================================================================= */

/**
 * Compila módulo fonte para .astm
 * @param source_path Caminho do arquivo .ast
 * @param output_path Caminho do arquivo .astm (ou NULL para auto)
 * @return CompiledModule ou NULL em erro
 */
CompiledModule* astm_compile(const char* source_path, const char* output_path);

/**
 * Carrega módulo .astm do disco
 * @param path Caminho do arquivo .astm
 * @return CompiledModule ou NULL em erro
 */
CompiledModule* astm_load(const char* path);

/**
 * Salva módulo compilado para .astm
 * @param module Módulo a salvar
 * @param path Caminho do arquivo .astm
 * @return 0 em sucesso
 */
int astm_save(CompiledModule* module, const char* path);

/**
 * Libera módulo compilado
 */
void astm_free(CompiledModule* module);

/**
 * Verifica se módulo está atualizado (cache válido)
 */
int astm_is_valid(const char* astm_path, const char* source_path);

/**
 * Obtém versão do formato
 */
void astm_get_version(int* major, int* minor);

/**
 * Imprime informações do módulo
 */
void astm_print_info(CompiledModule* module);

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_ASTM_H */

