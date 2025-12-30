/**
 * =============================================================================
 * ASTERON CORE ABI v1.0.0 - FROZEN
 * =============================================================================
 * 
 * ██████╗ ██████╗  ██████╗ ███████╗███████╗███╗   ██╗
 * ██╔══██╗██╔══██╗██╔═══██╗╚══███╔╝██╔════╝████╗  ██║
 * ██████╔╝██████╔╝██║   ██║  ███╔╝ █████╗  ██╔██╗ ██║
 * ██╔═══╝ ██╔══██╗██║   ██║ ███╔╝  ██╔══╝  ██║╚██╗██║
 * ██║     ██║  ██║╚██████╔╝███████╗███████╗██║ ╚████║
 * ╚═╝     ╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚══════╝╚═╝  ╚═══╝
 * 
 * Esta é a Interface Binária de Aplicação (ABI) CONGELADA do Asteron.
 * 
 * =============================================================================
 * REGRAS DE ESTABILIDADE (NUNCA VIOLAR):
 * =============================================================================
 * 
 * 1. Esta interface é CONGELADA - NÃO MODIFICAR campos existentes
 * 2. Novos campos só podem ser ADICIONADOS no final das structs
 * 3. Campos existentes NUNCA são removidos ou reordenados
 * 4. Tamanhos de tipos são FIXOS para compatibilidade binária
 * 5. Funções marcadas com ASTERON_API são parte da ABI pública
 * 6. Enums mantêm valores numéricos FIXOS
 * 
 * =============================================================================
 * CONVENÇÃO DE CHAMADA:
 * =============================================================================
 * 
 * - Funções nativas: (int arg_count, AsteronValue* args) -> AsteronValue
 * - Erros: Retornar AsteronValue com type = ASTERON_VAL_ERROR
 * - Memory: Valores OWNED devem ser liberados, BORROWED não
 * 
 * =============================================================================
 */

#ifndef ASTERON_ABI_H
#define ASTERON_ABI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * VERSÃO DA ABI (FROZEN)
 * ============================================================================= */

#define ASTERON_ABI_VERSION_MAJOR   1
#define ASTERON_ABI_VERSION_MINOR   0
#define ASTERON_ABI_VERSION_PATCH   0
#define ASTERON_ABI_STATUS          "FROZEN"

/* String de versão */
#define ASTERON_ABI_VERSION_STRING  "1.0.0"

/* Check de compatibilidade */
#define ASTERON_ABI_COMPATIBLE(major, minor) \
    ((major) == ASTERON_ABI_VERSION_MAJOR && (minor) <= ASTERON_ABI_VERSION_MINOR)

/* =============================================================================
 * MACROS DE EXPORTAÇÃO
 * ============================================================================= */

#ifdef _WIN32
    #ifdef ASTERON_BUILD_DLL
        #define ASTERON_API __declspec(dllexport)
    #else
        #define ASTERON_API __declspec(dllimport)
    #endif
    #define ASTERON_INTERNAL
#else
    #define ASTERON_API __attribute__((visibility("default")))
    #define ASTERON_INTERNAL __attribute__((visibility("hidden")))
#endif

/* Marca funções como deprecated */
#define ASTERON_DEPRECATED(msg) __attribute__((deprecated(msg)))

/* Marca funções como cold (raramente chamadas) */
#define ASTERON_COLD __attribute__((cold))

/* Marca funções como hot (frequentemente chamadas) */
#define ASTERON_HOT __attribute__((hot))

/* Inline hint */
#define ASTERON_INLINE static inline __attribute__((always_inline))

/* =============================================================================
 * TIPOS FUNDAMENTAIS (FROZEN - NÃO MODIFICAR)
 * ============================================================================= */

/**
 * Tipos de valores da linguagem Asteron
 * 
 * FROZEN: Valores numéricos são FIXOS e parte da ABI
 */
typedef enum {
    ASTERON_VAL_NIL         = 0,    /* Valor nulo */
    ASTERON_VAL_BOOL        = 1,    /* Booleano */
    ASTERON_VAL_NUMBER      = 2,    /* Número (double IEEE 754) */
    ASTERON_VAL_STRING      = 3,    /* String (UTF-8) */
    ASTERON_VAL_FUNCTION    = 4,    /* Função Asteron */
    ASTERON_VAL_NATIVE      = 5,    /* Função nativa C */
    ASTERON_VAL_OBJECT      = 6,    /* Objeto genérico */
    ASTERON_VAL_ERROR       = 7,    /* Erro de runtime */
    ASTERON_VAL_ARRAY       = 8,    /* Array */
    ASTERON_VAL_MAP         = 9,    /* Map/Dict */
    ASTERON_VAL_BYTES       = 10,   /* Buffer de bytes */
    ASTERON_VAL_HANDLE      = 11,   /* Handle para recurso externo */
    ASTERON_VAL_FUTURE      = 12,   /* Valor futuro (async) */
    /* === RESERVADO para expansão futura === */
    ASTERON_VAL_RESERVED_13 = 13,
    ASTERON_VAL_RESERVED_14 = 14,
    ASTERON_VAL_RESERVED_15 = 15,
    /* ====================================== */
    ASTERON_VAL_MAX         = 255   /* Máximo (1 byte) */
} AsteronValueType;

/**
 * Valor Asteron - Unidade fundamental de dados
 * 
 * FROZEN: Tamanho = 16 bytes em 64-bit (garantido pela ABI)
 * 
 * Layout:
 *   [0]     type      - Tipo do valor (AsteronValueType)
 *   [1]     flags     - Flags de controle
 *   [2-3]   reserved  - Padding/reservado
 *   [4-7]   extra     - Dados extras (subtipo, índice, etc)
 *   [8-15]  as        - Dados do valor
 */
typedef struct AsteronValue {
    uint8_t  type;          /* AsteronValueType */
    uint8_t  flags;         /* Flags de controle */
    uint16_t reserved;      /* Reservado/padding */
    uint32_t extra;         /* Dados extras */
    union {
        double   number;    /* ASTERON_VAL_NUMBER */
        int64_t  boolean;   /* ASTERON_VAL_BOOL (0 ou 1) */
        int64_t  integer;   /* Para inteiros (futuro) */
        void*    ptr;       /* Ponteiro para objetos */
        uint64_t bits;      /* Acesso raw aos bits */
    } as;
} AsteronValue;

/* Verificação de tamanho em compile-time */
_Static_assert(sizeof(AsteronValue) == 16, "AsteronValue must be 16 bytes");

/* =============================================================================
 * FLAGS DE VALOR (FROZEN)
 * ============================================================================= */

#define ASTERON_FLAG_NONE       0x00    /* Sem flags */
#define ASTERON_FLAG_READONLY   0x01    /* Valor somente leitura */
#define ASTERON_FLAG_BORROWED   0x02    /* Emprestado (não liberar) */
#define ASTERON_FLAG_OWNED      0x04    /* Proprietário (liberar) */
#define ASTERON_FLAG_STATIC     0x08    /* Dado estático */
#define ASTERON_FLAG_TEMP       0x10    /* Valor temporário */
#define ASTERON_FLAG_MARKED     0x20    /* Marcado pelo GC */

/* =============================================================================
 * TIPOS DE OBJETOS (FROZEN)
 * ============================================================================= */

typedef enum {
    ASTERON_OBJ_STRING      = 0,    /* String */
    ASTERON_OBJ_FUNCTION    = 1,    /* Função */
    ASTERON_OBJ_NATIVE      = 2,    /* Função nativa */
    ASTERON_OBJ_ARRAY       = 3,    /* Array */
    ASTERON_OBJ_MAP         = 4,    /* Map/Dict */
    ASTERON_OBJ_MODULE      = 5,    /* Módulo */
    ASTERON_OBJ_BYTES       = 6,    /* Buffer de bytes */
    ASTERON_OBJ_FILE        = 7,    /* Handle de arquivo */
    ASTERON_OBJ_SOCKET      = 8,    /* Handle de socket */
    ASTERON_OBJ_TASK        = 9,    /* Task assíncrona */
    ASTERON_OBJ_CHANNEL     = 10,   /* Canal de comunicação */
    ASTERON_OBJ_ERROR       = 11,   /* Objeto de erro */
    ASTERON_OBJ_ITER        = 12,   /* Iterador */
    ASTERON_OBJ_RANGE       = 13,   /* Range */
    ASTERON_OBJ_REGEX       = 14,   /* Expressão regular */
    ASTERON_OBJ_TIMER       = 15,   /* Timer/Instant */
    ASTERON_OBJ_MAX         = 255
} AsteronObjType;

/**
 * Cabeçalho de objeto heap-allocated
 * 
 * FROZEN: Este struct é prefixo de TODOS os objetos no heap
 */
typedef struct AsteronObjHeader {
    uint8_t  obj_type;      /* AsteronObjType */
    uint8_t  flags;         /* Flags do objeto */
    uint16_t reserved;      /* Reservado */
    uint32_t ref_count;     /* Reference count */
    struct AsteronObjHeader* next;  /* Lista para GC */
} AsteronObjHeader;

_Static_assert(sizeof(AsteronObjHeader) == 16, "AsteronObjHeader must be 16 bytes");

/* =============================================================================
 * STRINGS (FROZEN)
 * ============================================================================= */

typedef struct AsteronString {
    AsteronObjHeader header;
    size_t   length;        /* Bytes (sem \0) */
    uint32_t hash;          /* Hash pré-calculado */
    uint32_t capacity;      /* Capacidade alocada */
    char     chars[];       /* Flexible array (C99) */
} AsteronString;

/* =============================================================================
 * ARRAYS (FROZEN)
 * ============================================================================= */

typedef struct AsteronArray {
    AsteronObjHeader header;
    AsteronValue* items;    /* Elementos */
    size_t length;          /* Número de elementos */
    size_t capacity;        /* Capacidade */
} AsteronArray;

/* =============================================================================
 * BYTES/BUFFER (FROZEN)
 * ============================================================================= */

typedef struct AsteronBytes {
    AsteronObjHeader header;
    uint8_t* data;          /* Dados */
    size_t   length;        /* Bytes */
    size_t   capacity;      /* Capacidade */
    bool     readonly;      /* Somente leitura */
} AsteronBytes;

/* =============================================================================
 * HANDLES (FROZEN)
 * ============================================================================= */

typedef enum {
    HANDLE_FILE     = 0,    /* Arquivo */
    HANDLE_SOCKET   = 1,    /* Socket */
    HANDLE_PIPE     = 2,    /* Pipe */
    HANDLE_PROCESS  = 3,    /* Processo */
    HANDLE_THREAD   = 4,    /* Thread */
    HANDLE_TIMER    = 5,    /* Timer */
    HANDLE_DIR      = 6,    /* Diretório */
    HANDLE_CUSTOM   = 255   /* Customizado */
} HandleType;

typedef struct AsteronHandle {
    AsteronObjHeader header;
    HandleType   handle_type;
    int          fd;            /* File descriptor (se aplicável) */
    void*        data;          /* Dados extras */
    void       (*close)(void*); /* Função para fechar */
} AsteronHandle;

/* =============================================================================
 * CÓDIGOS DE ERRO (FROZEN)
 * ============================================================================= */

typedef enum {
    ASTERON_OK              = 0,    /* Sucesso */
    ASTERON_ERROR_GENERIC   = 1,    /* Erro genérico */
    ASTERON_ERROR_COMPILE   = 2,    /* Erro de compilação */
    ASTERON_ERROR_RUNTIME   = 3,    /* Erro de runtime */
    ASTERON_ERROR_TYPE      = 4,    /* Erro de tipo */
    ASTERON_ERROR_MEMORY    = 5,    /* Erro de memória */
    ASTERON_ERROR_IO        = 6,    /* Erro de I/O */
    ASTERON_ERROR_INVALID   = 7,    /* Argumento inválido */
    ASTERON_ERROR_NOT_FOUND = 8,    /* Não encontrado */
    ASTERON_ERROR_EXISTS    = 9,    /* Já existe */
    ASTERON_ERROR_PERM      = 10,   /* Permissão negada */
    ASTERON_ERROR_BUSY      = 11,   /* Recurso ocupado */
    ASTERON_ERROR_TIMEOUT   = 12,   /* Timeout */
    ASTERON_ERROR_NET       = 13,   /* Erro de rede */
    ASTERON_ERROR_EOF       = 14,   /* Fim de arquivo */
    ASTERON_ERROR_OVERFLOW  = 15,   /* Overflow */
    ASTERON_ERROR_UNDERFLOW = 16,   /* Underflow */
    ASTERON_ERROR_ASSERT    = 17,   /* Assertion falhou */
} AsteronResult;

/* =============================================================================
 * INTERFACE DE FUNÇÕES NATIVAS (FROZEN)
 * ============================================================================= */

/**
 * Assinatura de função nativa
 */
typedef AsteronValue (*AsteronNativeFn)(int arg_count, AsteronValue* args);

/**
 * Descritor de função nativa
 */
typedef struct AsteronNativeDesc {
    const char*     name;       /* Nome */
    AsteronNativeFn fn;         /* Ponteiro */
    int             min_args;   /* Mínimo de args */
    int             max_args;   /* Máximo (-1 = variádico) */
    const char*     signature;  /* Ex: "(number, string) -> bool" */
    const char*     doc;        /* Documentação */
} AsteronNativeDesc;

/* =============================================================================
 * DESCRITOR DE MÓDULO (FROZEN)
 * ============================================================================= */

typedef struct AsteronModuleDesc {
    const char*       name;         /* Nome do módulo */
    const char*       version;      /* Versão semver */
    const char*       description;  /* Descrição */
    const char*       author;       /* Autor */
    
    /* Funções exportadas */
    AsteronNativeDesc* functions;
    size_t            function_count;
    
    /* Constantes exportadas */
    struct {
        const char*   name;
        AsteronValue  value;
    }* constants;
    size_t            constant_count;
    
    /* Callbacks de lifecycle */
    AsteronResult   (*init)(void);      /* Inicialização */
    void            (*cleanup)(void);   /* Limpeza */
    
    /* Dependências */
    const char**      deps;
    size_t            dep_count;
} AsteronModuleDesc;

/* =============================================================================
 * HANDLES OPACOS
 * ============================================================================= */

typedef struct AsteronRuntime AsteronRuntime;
typedef struct AsteronModule AsteronModule;
typedef struct AsteronVM AsteronVM;

/* =============================================================================
 * MACROS DE CRIAÇÃO DE VALORES (FROZEN)
 * ============================================================================= */

#define ASTERON_NIL() \
    ((AsteronValue){ .type = ASTERON_VAL_NIL, .flags = 0, .extra = 0, .as.ptr = NULL })

#define ASTERON_BOOL(b) \
    ((AsteronValue){ .type = ASTERON_VAL_BOOL, .flags = 0, .extra = 0, .as.boolean = (b) ? 1 : 0 })

#define ASTERON_NUMBER(n) \
    ((AsteronValue){ .type = ASTERON_VAL_NUMBER, .flags = 0, .extra = 0, .as.number = (double)(n) })

#define ASTERON_INT(n) \
    ((AsteronValue){ .type = ASTERON_VAL_NUMBER, .flags = 0, .extra = 1, .as.integer = (int64_t)(n) })

#define ASTERON_PTR(p, t) \
    ((AsteronValue){ .type = (t), .flags = ASTERON_FLAG_OWNED, .extra = 0, .as.ptr = (void*)(p) })

#define ASTERON_HANDLE(h, ht) \
    ((AsteronValue){ .type = ASTERON_VAL_HANDLE, .flags = ASTERON_FLAG_OWNED, .extra = (ht), .as.ptr = (void*)(h) })

#define ASTERON_ERROR_VAL(code) \
    ((AsteronValue){ .type = ASTERON_VAL_ERROR, .flags = 0, .extra = (code), .as.ptr = NULL })

/* =============================================================================
 * MACROS DE VERIFICAÇÃO DE TIPO (FROZEN)
 * ============================================================================= */

#define ASTERON_IS_NIL(v)       ((v).type == ASTERON_VAL_NIL)
#define ASTERON_IS_BOOL(v)      ((v).type == ASTERON_VAL_BOOL)
#define ASTERON_IS_NUMBER(v)    ((v).type == ASTERON_VAL_NUMBER)
#define ASTERON_IS_STRING(v)    ((v).type == ASTERON_VAL_STRING)
#define ASTERON_IS_FUNCTION(v)  ((v).type == ASTERON_VAL_FUNCTION)
#define ASTERON_IS_NATIVE(v)    ((v).type == ASTERON_VAL_NATIVE)
#define ASTERON_IS_OBJECT(v)    ((v).type == ASTERON_VAL_OBJECT)
#define ASTERON_IS_ERROR(v)     ((v).type == ASTERON_VAL_ERROR)
#define ASTERON_IS_ARRAY(v)     ((v).type == ASTERON_VAL_ARRAY)
#define ASTERON_IS_MAP(v)       ((v).type == ASTERON_VAL_MAP)
#define ASTERON_IS_BYTES(v)     ((v).type == ASTERON_VAL_BYTES)
#define ASTERON_IS_HANDLE(v)    ((v).type == ASTERON_VAL_HANDLE)
#define ASTERON_IS_FUTURE(v)    ((v).type == ASTERON_VAL_FUTURE)

#define ASTERON_IS_TRUTHY(v)    (!ASTERON_IS_NIL(v) && \
                                 (!ASTERON_IS_BOOL(v) || (v).as.boolean))

/* =============================================================================
 * MACROS DE EXTRAÇÃO DE VALOR (FROZEN)
 * ============================================================================= */

#define ASTERON_AS_BOOL(v)      ((v).as.boolean != 0)
#define ASTERON_AS_NUMBER(v)    ((v).as.number)
#define ASTERON_AS_INT(v)       ((v).as.integer)
#define ASTERON_AS_PTR(v)       ((v).as.ptr)
#define ASTERON_AS_STRING(v)    ((AsteronString*)(v).as.ptr)
#define ASTERON_AS_ARRAY(v)     ((AsteronArray*)(v).as.ptr)
#define ASTERON_AS_BYTES(v)     ((AsteronBytes*)(v).as.ptr)
#define ASTERON_AS_HANDLE(v)    ((AsteronHandle*)(v).as.ptr)
#define ASTERON_ERROR_CODE(v)   ((AsteronResult)(v).extra)

/* =============================================================================
 * API PÚBLICA DO RUNTIME (FROZEN)
 * ============================================================================= */

/* Lifecycle */
ASTERON_API AsteronRuntime* asteron_runtime_create(void);
ASTERON_API void asteron_runtime_destroy(AsteronRuntime* rt);

/* Compilação e Execução */
ASTERON_API AsteronModule* asteron_compile(AsteronRuntime* rt, const char* source, const char* name);
ASTERON_API AsteronResult asteron_execute(AsteronRuntime* rt, AsteronModule* module);
ASTERON_API AsteronValue asteron_eval(AsteronRuntime* rt, const char* source);

/* Registro de Módulos */
ASTERON_API AsteronResult asteron_register_module(AsteronRuntime* rt, AsteronModuleDesc* desc);
ASTERON_API AsteronResult asteron_register_native(AsteronRuntime* rt, AsteronNativeDesc* desc);
ASTERON_API AsteronModule* asteron_load_module(AsteronRuntime* rt, const char* name);

/* Erros */
ASTERON_API const char* asteron_get_error(AsteronRuntime* rt);
ASTERON_API void asteron_clear_error(AsteronRuntime* rt);
ASTERON_API const char* asteron_result_string(AsteronResult result);

/* Versão */
ASTERON_API void asteron_get_version(int* major, int* minor, int* patch);
ASTERON_API const char* asteron_version_string(void);
ASTERON_API bool asteron_abi_compatible(int major, int minor);

/* =============================================================================
 * API DE MEMÓRIA (FROZEN)
 * ============================================================================= */

ASTERON_API void asteron_obj_retain(void* obj);
ASTERON_API void asteron_obj_release(void* obj);
ASTERON_API uint32_t asteron_obj_refcount(void* obj);

/* Strings */
ASTERON_API AsteronValue asteron_string_new(AsteronRuntime* rt, const char* str, int length);
ASTERON_API const char* asteron_string_cstr(AsteronValue str);
ASTERON_API size_t asteron_string_length(AsteronValue str);

/* Arrays */
ASTERON_API AsteronValue asteron_array_new(AsteronRuntime* rt, size_t capacity);
ASTERON_API void asteron_array_push(AsteronValue arr, AsteronValue value);
ASTERON_API AsteronValue asteron_array_get(AsteronValue arr, size_t index);
ASTERON_API size_t asteron_array_length(AsteronValue arr);

/* Bytes */
ASTERON_API AsteronValue asteron_bytes_new(AsteronRuntime* rt, size_t capacity);
ASTERON_API AsteronValue asteron_bytes_from(AsteronRuntime* rt, const uint8_t* data, size_t length);
ASTERON_API uint8_t* asteron_bytes_data(AsteronValue bytes);
ASTERON_API size_t asteron_bytes_length(AsteronValue bytes);

/* =============================================================================
 * MACRO PARA DEFINIR MÓDULOS (ABI)
 * Nota: Use as macros em module.h para módulos nativos completos
 * ============================================================================= */

#ifndef ASTERON_MODULE_BEGIN  /* Evita redefinição se module.h já incluiu */

#define ASTERON_MODULE_BEGIN(modname) \
    static AsteronNativeDesc modname##_functions[] = {

#define ASTERON_MODULE_FUNC(name, fn, min, max, sig, doc) \
    { .name = name, .fn = fn, .min_args = min, .max_args = max, .signature = sig, .doc = doc },

#define ASTERON_MODULE_END(modname, ver, desc) \
    }; \
    AsteronModuleDesc modname##_module = { \
        .name = #modname, \
        .version = ver, \
        .description = desc, \
        .functions = modname##_functions, \
        .function_count = sizeof(modname##_functions) / sizeof(modname##_functions[0]), \
        .constants = NULL, \
        .constant_count = 0, \
        .init = NULL, \
        .cleanup = NULL, \
        .deps = NULL, \
        .dep_count = 0 \
    };

#endif /* ASTERON_MODULE_BEGIN */

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_ABI_H */

