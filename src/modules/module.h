/**
 * =============================================================================
 * ASTERON MODULE SYSTEM v1.0
 * =============================================================================
 * 
 * Sistema de módulos para Asteron.
 * Suporta módulos nativos (C) e módulos Asteron (.ast).
 * 
 * Uso:
 *   import math               // Importa módulo inteiro
 *   import math.sin           // Importa função específica  
 *   from math import sin, cos // Importa múltiplas funções
 * 
 * =============================================================================
 */

#ifndef ASTERON_MODULE_H
#define ASTERON_MODULE_H

#include "../core/abi.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * TIPOS DE MÓDULOS
 * ============================================================================= */

typedef enum {
    MODULE_NATIVE = 0,      /* Módulo escrito em C */
    MODULE_ASTERON = 1,     /* Módulo escrito em Asteron (.ast) */
    MODULE_BUILTIN = 2      /* Módulo built-in (sempre disponível) */
} ModuleType;

typedef enum {
    MODULE_OK = 0,
    MODULE_NOT_FOUND = 1,
    MODULE_LOAD_ERROR = 2,
    MODULE_INIT_ERROR = 3,
    MODULE_SYMBOL_NOT_FOUND = 4
} ModuleResult;

/* =============================================================================
 * ESTRUTURAS DE MÓDULO
 * ============================================================================= */

/**
 * Entrada de símbolo exportado pelo módulo
 */
typedef struct {
    const char* name;           /* Nome do símbolo */
    AsteronNativeFn fn;         /* Função (se for função) */
    AsteronValue value;         /* Valor (se for constante) */
    int is_function;            /* 1 se é função, 0 se é valor */
    int arity;                  /* Aridade da função (-1 = variádico) */
    const char* signature;      /* Assinatura legível */
} ModuleExport;

/**
 * Descritor de módulo nativo
 */
typedef struct {
    const char* name;           /* Nome do módulo */
    const char* version;        /* Versão do módulo */
    const char* description;    /* Descrição */
    ModuleExport* exports;      /* Lista de símbolos exportados */
    size_t export_count;        /* Número de exports */
    
    /* Callbacks de ciclo de vida */
    int (*init)(void);          /* Inicialização (opcional) */
    void (*cleanup)(void);      /* Limpeza (opcional) */
} NativeModuleDesc;

/**
 * Módulo carregado (instância)
 */
typedef struct ModuleInstance {
    char* name;                     /* Nome do módulo */
    ModuleType type;                /* Tipo do módulo */
    void* handle;                   /* Handle (para dlopen, etc) */
    NativeModuleDesc* native_desc;  /* Descritor (se nativo) */
    int is_loaded;                  /* 1 se carregado */
    int ref_count;                  /* Reference count */
    struct ModuleInstance* next;    /* Lista encadeada */
} ModuleInstance;

/**
 * Registry de módulos (global)
 */
typedef struct {
    ModuleInstance* modules;        /* Lista de módulos carregados */
    size_t module_count;            /* Número de módulos */
    
    /* Módulos built-in registrados */
    NativeModuleDesc** builtins;    /* Array de módulos built-in */
    size_t builtin_count;
    size_t builtin_capacity;
    
    /* Caminhos de busca */
    char** search_paths;            /* Diretórios de busca */
    size_t path_count;
    size_t path_capacity;
} ModuleRegistry;

/* =============================================================================
 * API DO SISTEMA DE MÓDULOS
 * ============================================================================= */

/**
 * Inicializa o sistema de módulos
 * @return MODULE_OK em sucesso
 */
ModuleResult module_system_init(void);

/**
 * Finaliza o sistema de módulos
 */
void module_system_shutdown(void);

/**
 * Obtém o registry global
 * @return Ponteiro para o registry
 */
ModuleRegistry* module_get_registry(void);

/**
 * Registra um módulo built-in
 * @param desc Descritor do módulo
 * @return MODULE_OK em sucesso
 */
ModuleResult module_register_builtin(NativeModuleDesc* desc);

/**
 * Carrega um módulo pelo nome
 * @param name Nome do módulo
 * @return Instância do módulo ou NULL
 */
ModuleInstance* module_load(const char* name);

/**
 * Descarrega um módulo
 * @param module Instância do módulo
 */
void module_unload(ModuleInstance* module);

/**
 * Busca um símbolo em um módulo
 * @param module Instância do módulo
 * @param symbol_name Nome do símbolo
 * @return Ponteiro para o export ou NULL
 */
ModuleExport* module_find_symbol(ModuleInstance* module, const char* symbol_name);

/**
 * Importa um símbolo para o runtime
 * @param rt Runtime Asteron
 * @param module_name Nome do módulo
 * @param symbol_name Nome do símbolo (NULL = importar tudo)
 * @return MODULE_OK em sucesso
 */
ModuleResult module_import(AsteronRuntime* rt, const char* module_name, const char* symbol_name);

/**
 * Adiciona um caminho de busca
 * @param path Caminho do diretório
 */
void module_add_search_path(const char* path);

/**
 * Lista todos os módulos carregados
 */
void module_list_loaded(void);

/**
 * Imprime informações de um módulo
 * @param module Instância do módulo
 */
void module_print_info(ModuleInstance* module);

/* =============================================================================
 * MACROS PARA DEFINIÇÃO DE MÓDULOS NATIVOS
 * ============================================================================= */

/* Undefine versões simples de abi.h se existirem */
#ifdef ASTERON_MODULE_BEGIN
#undef ASTERON_MODULE_BEGIN
#endif
#ifdef ASTERON_MODULE_END
#undef ASTERON_MODULE_END
#endif

/**
 * Macro para começar definição de módulo
 */
#define ASTERON_MODULE_BEGIN(mod_name, mod_version, mod_desc) \
    static ModuleExport _module_exports[] = {

/**
 * Macro para exportar função
 */
#define ASTERON_EXPORT_FUNC(name, fn, arity, sig) \
    { name, fn, {0}, 1, arity, sig },

/**
 * Macro para exportar constante numérica
 */
#define ASTERON_EXPORT_CONST(name, value) \
    { name, NULL, ASTERON_NUMBER(value), 0, 0, NULL },

/**
 * Macro para finalizar definição de módulo
 */
#define ASTERON_MODULE_END(mod_name, mod_version, mod_desc, init_fn, cleanup_fn) \
    }; \
    NativeModuleDesc mod_name##_module = { \
        .name = #mod_name, \
        .version = mod_version, \
        .description = mod_desc, \
        .exports = _module_exports, \
        .export_count = sizeof(_module_exports) / sizeof(_module_exports[0]), \
        .init = init_fn, \
        .cleanup = cleanup_fn \
    };

/**
 * Macro para declarar módulo externo
 */
#define ASTERON_MODULE_DECLARE(mod_name) \
    extern NativeModuleDesc mod_name##_module;

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_MODULE_H */

