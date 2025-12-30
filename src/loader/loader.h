/**
 * =============================================================================
 * ASTERON LOADER v1.0
 * =============================================================================
 * 
 * Loader simples (hardcoded) para inicializar o runtime Asteron.
 * 
 * Este é um loader minimalista que:
 * 1. Inicializa todos os subsistemas do core
 * 2. Configura a tabela de funções nativas padrão
 * 3. Prepara o runtime para execução
 * 
 * =============================================================================
 */

#ifndef ASTERON_LOADER_H
#define ASTERON_LOADER_H

#include "../core/abi.h"
#include "../core/version.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * ESTRUTURA DO RUNTIME (Implementação interna)
 * ============================================================================= */

/**
 * Tabela de funções nativas registradas
 */
typedef struct {
    AsteronNativeDesc* functions;
    size_t count;
    size_t capacity;
} NativeTable;

/**
 * Runtime Asteron (implementação concreta)
 */
struct AsteronRuntime {
    /* Versão */
    int version_major;
    int version_minor;
    int version_patch;
    
    /* Subsistemas (ponteiros opacos internamente) */
    void* lexer;
    void* parser;
    void* typechecker;
    void* vm;
    void* jit_context;
    
    /* Tabela de funções nativas */
    NativeTable natives;
    
    /* Estado */
    int is_initialized;
    int is_running;
    
    /* Erro */
    char* last_error;
    AsteronResult last_result;
    
    /* Memória */
    void* memory_arena;
    AsteronObjHeader* objects;  /* Lista encadeada de objetos para GC */
};

/**
 * Módulo Asteron (implementação concreta)
 */
struct AsteronModule {
    char* name;
    void* ast;              /* ASTNode* */
    void* bytecode;         /* BytecodeProgram* */
    int is_compiled;
};

/* =============================================================================
 * FUNÇÕES DO LOADER
 * ============================================================================= */

/**
 * Inicializa o loader (chamado uma vez no início do programa)
 * @return ASTERON_OK em sucesso
 */
ASTERON_API AsteronResult asteron_loader_init(void);

/**
 * Finaliza o loader (chamado no fim do programa)
 */
ASTERON_API void asteron_loader_shutdown(void);

/**
 * Verifica se o loader foi inicializado
 * @return 1 se inicializado, 0 caso contrário
 */
ASTERON_API int asteron_loader_is_initialized(void);

/**
 * Imprime informações do loader (debug)
 */
ASTERON_API void asteron_loader_print_info(void);

/* =============================================================================
 * FUNÇÕES NATIVAS PADRÃO (Hardcoded)
 * ============================================================================= */

/**
 * Registra todas as funções nativas padrão no runtime
 * @param rt Handle do runtime
 * @return ASTERON_OK em sucesso
 */
ASTERON_API AsteronResult asteron_register_stdlib(AsteronRuntime* rt);

/* Funções nativas disponíveis */
ASTERON_API AsteronValue native_print(int argc, AsteronValue* args);
ASTERON_API AsteronValue native_println(int argc, AsteronValue* args);
ASTERON_API AsteronValue native_type(int argc, AsteronValue* args);
ASTERON_API AsteronValue native_len(int argc, AsteronValue* args);
ASTERON_API AsteronValue native_str(int argc, AsteronValue* args);
ASTERON_API AsteronValue native_num(int argc, AsteronValue* args);
ASTERON_API AsteronValue native_clock(int argc, AsteronValue* args);
ASTERON_API AsteronValue native_char_at(int argc, AsteronValue* args);
ASTERON_API AsteronValue native_substr(int argc, AsteronValue* args);
ASTERON_API AsteronValue native_index_of(int argc, AsteronValue* args);

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_LOADER_H */

