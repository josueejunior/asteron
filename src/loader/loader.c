/**
 * =============================================================================
 * ASTERON LOADER v1.0 - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "loader.h"
#include "../core/lexer/lexer.h"
#include "../core/parser/parser.h"
#include "../core/ast/ast.h"
#include "../core/typechecker/typechecker.h"
#include "../core/vm/vm.h"
#include "../core/abi.h"
#include "../utils/utils.h"

/* Módulos nativos */
#include "../modules/native/net_module.h"
#include "../modules/native/fs_module.h"
#include "../modules/native/time_module.h"
#include "../modules/native/os_module.h"
#include "../modules/native/math_module.h"
#include "../modules/native/graph_module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* =============================================================================
 * ESTADO GLOBAL DO LOADER
 * ============================================================================= */

static int g_loader_initialized = 0;

/* =============================================================================
 * IMPLEMENTAÇÃO DA API PÚBLICA
 * ============================================================================= */

AsteronResult asteron_loader_init(void) {
    if (g_loader_initialized) {
        return ASTERON_OK;  /* Já inicializado */
    }
    
    /* TODO: Inicializar subsistemas globais se necessário */
    
    g_loader_initialized = 1;
    
    printf("[Loader] Asteron Core v%s inicializado (%s)\n", 
           ASTERON_VERSION_STRING, ASTERON_CORE_STATUS);
    
    return ASTERON_OK;
}

void asteron_loader_shutdown(void) {
    if (!g_loader_initialized) {
        return;
    }
    
    /* TODO: Limpar subsistemas globais */
    
    g_loader_initialized = 0;
    
    printf("[Loader] Asteron Core finalizado\n");
}

int asteron_loader_is_initialized(void) {
    return g_loader_initialized;
}

void asteron_loader_print_info(void) {
    printf("=== Asteron Runtime Info ===\n");
    printf("  Version: %s\n", ASTERON_VERSION_STRING);
    printf("  Status:  %s\n", ASTERON_CORE_STATUS);
    printf("  ABI:     %d.%d.%d\n", 
           ASTERON_ABI_VERSION_MAJOR,
           ASTERON_ABI_VERSION_MINOR, 
           ASTERON_ABI_VERSION_PATCH);
    printf("  Freeze:  %s\n", ASTERON_FREEZE_DATE);
    printf("\n  Features:\n");
    printf("    - Lexer:           %s\n", ASTERON_FEATURE_LEXER ? "Yes" : "No");
    printf("    - Parser:          %s\n", ASTERON_FEATURE_PARSER ? "Yes" : "No");
    printf("    - AST:             %s\n", ASTERON_FEATURE_AST ? "Yes" : "No");
    printf("    - Type Checker:    %s\n", ASTERON_FEATURE_TYPECHECKER ? "Yes" : "No");
    printf("    - Interpreter:     %s\n", ASTERON_FEATURE_INTERPRETER ? "Yes" : "No");
    printf("    - VM:              %s\n", ASTERON_FEATURE_VM ? "Yes" : "No");
    printf("    - Bytecode:        %s\n", ASTERON_FEATURE_BYTECODE ? "Yes" : "No");
    printf("    - JIT:             %s\n", ASTERON_FEATURE_JIT ? "Yes" : "No");
    printf("    - SSA:             %s\n", ASTERON_FEATURE_SSA ? "Yes" : "No");
    printf("    - Escape Analysis: %s\n", ASTERON_FEATURE_ESCAPE_ANALYSIS ? "Yes" : "No");
    printf("    - Inlining:        %s\n", ASTERON_FEATURE_INLINE ? "Yes" : "No");
    printf("    - Reg Alloc:       %s\n", ASTERON_FEATURE_REG_ALLOC ? "Yes" : "No");
    printf("============================\n");
}

/* =============================================================================
 * IMPLEMENTAÇÃO DO RUNTIME
 * ============================================================================= */

AsteronRuntime* asteron_runtime_create(void) {
    /* Garante que o loader foi inicializado */
    if (!g_loader_initialized) {
        asteron_loader_init();
    }
    
    AsteronRuntime* rt = (AsteronRuntime*)malloc(sizeof(AsteronRuntime));
    if (rt == NULL) {
        return NULL;
    }
    
    /* Inicializa campos */
    rt->version_major = ASTERON_VERSION_MAJOR;
    rt->version_minor = ASTERON_VERSION_MINOR;
    rt->version_patch = ASTERON_VERSION_PATCH;
    
    rt->lexer = NULL;
    rt->parser = NULL;
    rt->typechecker = NULL;
    rt->vm = NULL;
    rt->jit_context = NULL;
    
    /* Tabela de funções nativas */
    rt->natives.capacity = 32;
    rt->natives.count = 0;
    rt->natives.functions = (AsteronNativeDesc*)malloc(
        sizeof(AsteronNativeDesc) * rt->natives.capacity);
    if (rt->natives.functions == NULL) {
        free(rt);
        return NULL;
    }
    
    rt->is_initialized = 1;
    rt->is_running = 0;
    rt->last_error = NULL;
    rt->last_result = ASTERON_OK;
    
    rt->memory_arena = NULL;
    rt->objects = NULL;
    
    /* Registra biblioteca padrão */
    asteron_register_stdlib(rt);
    
    return rt;
}

void asteron_runtime_destroy(AsteronRuntime* rt) {
    if (rt == NULL) return;
    
    /* Libera tabela de nativas */
    if (rt->natives.functions != NULL) {
        free(rt->natives.functions);
    }
    
    /* Libera mensagem de erro */
    if (rt->last_error != NULL) {
        free(rt->last_error);
    }
    
    /* TODO: Liberar lista de objetos */
    
    free(rt);
}

AsteronModule* asteron_compile(AsteronRuntime* rt, const char* source, const char* name) {
    if (rt == NULL || source == NULL) {
        return NULL;
    }
    
    AsteronModule* module = (AsteronModule*)malloc(sizeof(AsteronModule));
    if (module == NULL) {
        return NULL;
    }
    
    /* Copia nome */
    module->name = name ? strdup(name) : strdup("<anonymous>");
    module->is_compiled = 0;
    module->ast = NULL;
    module->bytecode = NULL;
    
    /* Fase 1: Lexer */
    Lexer* lexer = lexer_create(source);
    if (lexer == NULL) {
        rt->last_error = strdup("Falha ao criar lexer");
        rt->last_result = ASTERON_ERROR_COMPILE;
        free(module->name);
        free(module);
        return NULL;
    }
    
    /* Fase 2: Parser */
    Parser* parser = parser_create(lexer);
    if (parser == NULL) {
        rt->last_error = strdup("Falha ao criar parser");
        rt->last_result = ASTERON_ERROR_COMPILE;
        lexer_destroy(lexer);
        free(module->name);
        free(module);
        return NULL;
    }
    
    /* Parse */
    ASTNode* ast = parser_parse(parser);
    if (parser_had_error(parser)) {
        rt->last_error = strdup("Erro de sintaxe");
        rt->last_result = ASTERON_ERROR_COMPILE;
        if (ast != NULL) ast_destroy_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(module->name);
        free(module);
        return NULL;
    }
    
    module->ast = ast;
    
    /* Fase 3: Type Check */
    if (typechecker_check(ast)) {
        rt->last_error = strdup("Erro de tipo");
        rt->last_result = ASTERON_ERROR_TYPE;
        ast_destroy_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(module->name);
        free(module);
        return NULL;
    }
    
    /* Fase 4: Compilar para bytecode */
    BytecodeProgram* bytecode = compiler_compile(ast);
    if (bytecode == NULL) {
        rt->last_error = strdup("Falha na compilação para bytecode");
        rt->last_result = ASTERON_ERROR_COMPILE;
        ast_destroy_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(module->name);
        free(module);
        return NULL;
    }
    
    module->bytecode = bytecode;
    module->is_compiled = 1;
    
    /* Limpeza */
    parser_destroy(parser);
    lexer_destroy(lexer);
    
    return module;
}

AsteronResult asteron_execute(AsteronRuntime* rt, AsteronModule* module) {
    if (rt == NULL || module == NULL) {
        return ASTERON_ERROR_INVALID;
    }
    
    if (!module->is_compiled || module->bytecode == NULL) {
        rt->last_error = strdup("Módulo não compilado");
        return ASTERON_ERROR_RUNTIME;
    }
    
    /* Cria VM */
    VM* vm = vm_create((BytecodeProgram*)module->bytecode);
    if (vm == NULL) {
        rt->last_error = strdup("Falha ao criar VM");
        return ASTERON_ERROR_RUNTIME;
    }
    
    rt->vm = vm;
    rt->is_running = 1;
    
    /* Executa */
    int result = vm_execute(vm);
    
    rt->is_running = 0;
    rt->vm = NULL;
    
    /* Limpeza */
    vm_destroy(vm);
    
    if (result != 0) {
        rt->last_error = strdup("Erro durante execução");
        return ASTERON_ERROR_RUNTIME;
    }
    
    return ASTERON_OK;
}

AsteronResult asteron_register_native(AsteronRuntime* rt, AsteronNativeDesc* desc) {
    if (rt == NULL || desc == NULL || desc->name == NULL || desc->fn == NULL) {
        return ASTERON_ERROR_INVALID;
    }
    
    /* Expande tabela se necessário */
    if (rt->natives.count >= rt->natives.capacity) {
        size_t new_cap = rt->natives.capacity * 2;
        AsteronNativeDesc* new_funcs = (AsteronNativeDesc*)realloc(
            rt->natives.functions, sizeof(AsteronNativeDesc) * new_cap);
        if (new_funcs == NULL) {
            return ASTERON_ERROR_MEMORY;
        }
        rt->natives.functions = new_funcs;
        rt->natives.capacity = new_cap;
    }
    
    /* Adiciona função */
    rt->natives.functions[rt->natives.count] = *desc;
    rt->natives.count++;
    
    /* Registra também na VM global */
    vm_register_native(desc->name, desc->fn, desc->min_args, desc->max_args);
    
    return ASTERON_OK;
}

const char* asteron_get_error(AsteronRuntime* rt) {
    if (rt == NULL) return NULL;
    return rt->last_error;
}

void asteron_get_version(int* major, int* minor, int* patch) {
    if (major) *major = ASTERON_VERSION_MAJOR;
    if (minor) *minor = ASTERON_VERSION_MINOR;
    if (patch) *patch = ASTERON_VERSION_PATCH;
}

/* =============================================================================
 * GERENCIAMENTO DE MEMÓRIA
 * ============================================================================= */

void asteron_obj_retain(void* obj) {
    if (obj == NULL) return;
    AsteronObjHeader* header = (AsteronObjHeader*)obj;
    header->ref_count++;
}

void asteron_obj_release(void* obj) {
    if (obj == NULL) return;
    AsteronObjHeader* header = (AsteronObjHeader*)obj;
    
    if (header->ref_count > 0) {
        header->ref_count--;
    }
    
    if (header->ref_count == 0) {
        /* TODO: Liberar objeto baseado no tipo */
        free(obj);
    }
}

AsteronValue asteron_string_create(AsteronRuntime* rt, const char* str, int length) {
    if (rt == NULL || str == NULL) {
        return ASTERON_NIL();
    }
    
    size_t len = (length < 0) ? strlen(str) : (size_t)length;
    
    /* Aloca string com flexible array member */
    AsteronString* s = (AsteronString*)malloc(sizeof(AsteronString) + len + 1);
    if (s == NULL) {
        return ASTERON_NIL();
    }
    
    /* Inicializa header */
    s->header.obj_type = ASTERON_OBJ_STRING;
    s->header.flags = 0;
    s->header.reserved = 0;
    s->header.ref_count = 1;
    s->header.next = rt->objects;
    rt->objects = &s->header;
    
    /* Copia conteúdo */
    s->length = len;
    s->hash = 0;  /* TODO: Calcular hash */
    memcpy(s->chars, str, len);
    s->chars[len] = '\0';
    
    AsteronValue result;
    result.type = ASTERON_VAL_STRING;
    result.flags = ASTERON_FLAG_OWNED;
    result.reserved = 0;
    result.extra = 0;
    result.as.ptr = s;
    
    return result;
}

/* =============================================================================
 * BIBLIOTECA PADRÃO (FUNÇÕES NATIVAS)
 * ============================================================================= */

AsteronValue native_print(int argc, AsteronValue* args) {
    for (int i = 0; i < argc; i++) {
        AsteronValue v = args[i];
        switch (v.type) {
            case ASTERON_VAL_NIL:
                printf("nil");
                break;
            case ASTERON_VAL_BOOL:
                printf("%s", v.as.boolean ? "true" : "false");
                break;
            case ASTERON_VAL_NUMBER:
                printf("%g", v.as.number);
                break;
            case ASTERON_VAL_STRING: {
                AsteronString* s = (AsteronString*)v.as.ptr;
                if (s != NULL) {
                    printf("%s", s->chars);
                }
                break;
            }
            default:
                printf("<object>");
                break;
        }
        if (i < argc - 1) printf(" ");
    }
    return ASTERON_NIL();
}

AsteronValue native_println(int argc, AsteronValue* args) {
    native_print(argc, args);
    printf("\n");
    return ASTERON_NIL();
}

AsteronValue native_type(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_NIL();
    
    const char* type_name;
    switch (args[0].type) {
        case ASTERON_VAL_NIL:      type_name = "nil"; break;
        case ASTERON_VAL_BOOL:     type_name = "bool"; break;
        case ASTERON_VAL_NUMBER:   type_name = "number"; break;
        case ASTERON_VAL_STRING:   type_name = "string"; break;
        case ASTERON_VAL_FUNCTION: type_name = "function"; break;
        case ASTERON_VAL_NATIVE:   type_name = "native"; break;
        case ASTERON_VAL_OBJECT:   type_name = "object"; break;
        case ASTERON_VAL_ERROR:    type_name = "error"; break;
        default:                   type_name = "unknown"; break;
    }
    
    /* Retorna string com o nome do tipo */
    /* NOTA: Simplificado - deveria criar AsteronString */
    (void)type_name;
    return ASTERON_NIL();
}

AsteronValue native_len(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_NUMBER(0);
    
    // CORREÇÃO CRÍTICA: Verifica nil primeiro para evitar corrupção de estado
    if (ASTERON_IS_NIL(args[0])) {
        // Retorna 0 sem acessar ponteiros - evita corrupção de estado interno
        return ASTERON_NUMBER(0);
    }
    
    // Verifica se é string e se o ponteiro é válido
    if (args[0].type == ASTERON_VAL_STRING) {
        AsteronString* s = (AsteronString*)args[0].as.ptr;
        // CORREÇÃO: Verifica ponteiro antes de acessar campos
        if (s != NULL && s->header.obj_type == ASTERON_OBJ_STRING) {
            return ASTERON_NUMBER((double)s->length);
        }
    }
    
    // Para outros tipos ou strings inválidas, retorna 0 sem corromper estado
    return ASTERON_NUMBER(0);
}

AsteronValue native_str(int argc, AsteronValue* args) {
    /* TODO: Converter valor para string */
    (void)argc;
    (void)args;
    return ASTERON_NIL();
}

AsteronValue native_num(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_NUMBER(0);
    
    if (args[0].type == ASTERON_VAL_NUMBER) {
        return args[0];
    }
    if (args[0].type == ASTERON_VAL_BOOL) {
        return ASTERON_NUMBER(args[0].as.boolean ? 1.0 : 0.0);
    }
    
    return ASTERON_NUMBER(0);
}

AsteronValue native_clock(int argc, AsteronValue* args) {
    (void)argc;
    (void)args;
    return ASTERON_NUMBER((double)clock() / CLOCKS_PER_SEC);
}

/* =============================================================================
 * FUNÇÕES DE MANIPULAÇÃO DE STRINGS
 * ============================================================================= */

static AsteronValue make_string(const char* s) {
    if (!s) return ASTERON_NIL();
    
    size_t len = strlen(s);
    AsteronString* str = (AsteronString*)malloc(sizeof(AsteronString) + len + 1);
    if (!str) return ASTERON_NIL();
    
    str->header.obj_type = ASTERON_OBJ_STRING;
    str->header.flags = 0;
    str->header.reserved = 0;
    str->header.ref_count = 1;
    str->header.next = NULL;
    str->length = len;
    str->hash = 0;
    str->capacity = len + 1;
    strcpy(str->chars, s);
    
    return ASTERON_PTR(str, ASTERON_VAL_STRING);
}

static const char* get_string_arg(AsteronValue* args, int index) {
    if (ASTERON_IS_NIL(args[index])) return NULL;
    if (!ASTERON_IS_STRING(args[index])) return NULL;
    AsteronString* str = ASTERON_AS_STRING(args[index]);
    return str ? str->chars : NULL;
}

/**
 * char_at(str, index) -> string
 * Retorna o caractere na posição index como string de 1 caractere
 */
AsteronValue native_char_at(int argc, AsteronValue* args) {
    if (argc < 2) return ASTERON_NIL();
    
    const char* str = get_string_arg(args, 0);
    if (!str) return ASTERON_NIL();
    
    if (!ASTERON_IS_NUMBER(args[1])) return ASTERON_NIL();
    int index = (int)ASTERON_AS_NUMBER(args[1]);
    
    AsteronString* s = ASTERON_AS_STRING(args[0]);
    if (!s || index < 0 || index >= (int)s->length) {
        return make_string("");
    }
    
    char ch = s->chars[index];
    char buf[2] = { ch, '\0' };
    return make_string(buf);
}

/**
 * substr(str, start, end?) -> string
 * Retorna substring de start até end (ou até o fim se end não fornecido)
 */
AsteronValue native_substr(int argc, AsteronValue* args) {
    if (argc < 2) return ASTERON_NIL();
    
    const char* str = get_string_arg(args, 0);
    if (!str) return ASTERON_NIL();
    
    if (!ASTERON_IS_NUMBER(args[1])) return ASTERON_NIL();
    int start = (int)ASTERON_AS_NUMBER(args[1]);
    
    AsteronString* s = ASTERON_AS_STRING(args[0]);
    if (!s || start < 0) return make_string("");
    if (start >= (int)s->length) return make_string("");
    
    int end = (int)s->length;
    if (argc >= 3 && ASTERON_IS_NUMBER(args[2])) {
        end = (int)ASTERON_AS_NUMBER(args[2]);
        if (end > (int)s->length) end = (int)s->length;
        if (end < start) return make_string("");
    }
    
    int len = end - start;
    if (len <= 0) return make_string("");
    
    char* buf = (char*)malloc(len + 1);
    if (!buf) return ASTERON_NIL();
    
    memcpy(buf, s->chars + start, len);
    buf[len] = '\0';
    
    AsteronValue result = make_string(buf);
    free(buf);
    return result;
}

/**
 * index_of(str, search) -> number
 * Retorna o índice da primeira ocorrência de search em str, ou -1 se não encontrado
 */
AsteronValue native_index_of(int argc, AsteronValue* args) {
    if (argc < 2) return ASTERON_NUMBER(-1);
    
    const char* str = get_string_arg(args, 0);
    const char* search = get_string_arg(args, 1);
    
    if (!str || !search) return ASTERON_NUMBER(-1);
    
    const char* pos = strstr(str, search);
    if (!pos) return ASTERON_NUMBER(-1);
    
    return ASTERON_NUMBER((double)(pos - str));
}

/* Registra todas as funções padrão */
AsteronResult asteron_register_stdlib(AsteronRuntime* rt) {
    if (rt == NULL) return ASTERON_ERROR_INVALID;
    
    /* Funções built-in básicas */
    AsteronNativeDesc stdlib[] = {
        { "print",   native_print,   0, -1, "any... -> nil", NULL },
        { "println", native_println, 0, -1, "any... -> nil", NULL },
        { "type",    native_type,    1,  1, "any -> string", NULL },
        { "len",     native_len,     1,  1, "string -> number", NULL },
        { "str",     native_str,     1,  1, "any -> string", NULL },
        { "num",     native_num,     1,  1, "any -> number", NULL },
        { "clock",   native_clock,   0,  0, "-> number", NULL },
        { "char_at", native_char_at, 2, 2, "(str: string, index: number) -> string", NULL },
        { "substr",  native_substr,  2, 3, "(str: string, start: number, end?: number) -> string", NULL },
        { "index_of", native_index_of, 2, 2, "(str: string, search: string) -> number", NULL },
    };
    
    size_t count = sizeof(stdlib) / sizeof(stdlib[0]);
    for (size_t i = 0; i < count; i++) {
        AsteronResult res = asteron_register_native(rt, &stdlib[i]);
        if (res != ASTERON_OK) {
            return res;
        }
    }
    
    /* === MÓDULO NET === */
    AsteronNativeDesc net_funcs[] = {
        { "hostname",     net_hostname,     0, 0, "() -> string", NULL },
        { "resolve",      net_resolve,      1, 1, "(host: string) -> string", NULL },
        { "tcp_connect",  net_tcp_connect,  2, 2, "(host: string, port: number) -> handle", NULL },
        { "tcp_listen",   net_tcp_listen,   1, 2, "(port: number, backlog?: number) -> handle", NULL },
        { "tcp_accept",   net_tcp_accept,   1, 1, "(server: handle) -> handle", NULL },
        { "tcp_send",     net_tcp_send,     2, 2, "(sock: handle, data: string) -> number", NULL },
        { "tcp_recv",     net_tcp_recv,     1, 2, "(sock: handle, max?: number) -> string", NULL },
        { "tcp_close",    net_tcp_close,    1, 1, "(sock: handle) -> bool", NULL },
    };
    
    count = sizeof(net_funcs) / sizeof(net_funcs[0]);
    for (size_t i = 0; i < count; i++) {
        AsteronResult res = asteron_register_native(rt, &net_funcs[i]);
        if (res != ASTERON_OK) {
            return res;
        }
    }
    
    /* === MÓDULO FS === */
    AsteronNativeDesc fs_funcs[] = {
        { "read",      fs_read,      1, 1, "(path: string) -> string", NULL },
        { "write",     fs_write,     2, 2, "(path: string, content: string) -> bool", NULL },
        { "append",    fs_append,    2, 2, "(path: string, content: string) -> bool", NULL },
        { "exists",    fs_exists,    1, 1, "(path: string) -> bool", NULL },
        { "remove",    fs_remove,    1, 1, "(path: string) -> bool", NULL },
        { "mkdir",     fs_mkdir,     1, 1, "(path: string) -> bool", NULL },
        { "rmdir",     fs_rmdir,     1, 1, "(path: string) -> bool", NULL },
        { "list",      fs_list,      1, 1, "(path: string) -> array", NULL },
        { "stat",      fs_stat,      1, 1, "(path: string) -> map", NULL },
        { "copy",      fs_copy,      2, 2, "(src: string, dst: string) -> bool", NULL },
        { "move",      fs_move,      2, 2, "(src: string, dst: string) -> bool", NULL },
        { "cwd",       fs_cwd,       0, 0, "() -> string", NULL },
        { "chdir",     fs_chdir,     1, 1, "(path: string) -> bool", NULL },
    };
    
    count = sizeof(fs_funcs) / sizeof(fs_funcs[0]);
    for (size_t i = 0; i < count; i++) {
        AsteronResult res = asteron_register_native(rt, &fs_funcs[i]);
        if (res != ASTERON_OK) {
            return res;
        }
    }
    
    /* === MÓDULO TIME === */
    AsteronNativeDesc time_funcs[] = {
        { "now",       time_now,       0, 0, "() -> number", NULL },
        { "now_ns",    time_now_ns,    0, 0, "() -> number", NULL },
        { "monotonic", time_monotonic, 0, 0, "() -> number", NULL },
        { "sleep",     time_sleep,     1, 1, "(ms: number) -> nil", NULL },
        { "instant",   time_instant,   0, 0, "() -> handle", NULL },
        { "elapsed",   time_elapsed,   1, 1, "(instant: handle) -> number", NULL },
        { "year",      time_year,      0, 0, "() -> number", NULL },
        { "month",     time_month,     0, 0, "() -> number", NULL },
        { "day",       time_day,       0, 0, "() -> number", NULL },
        { "hour",      time_hour,      0, 0, "() -> number", NULL },
        { "minute",    time_minute,    0, 0, "() -> number", NULL },
        { "second",    time_second,    0, 0, "() -> number", NULL },
        { "weekday",   time_weekday,   0, 0, "() -> number", NULL },
        { "format",    time_format,    1, 2, "(format: string, ts?: number) -> string", NULL },
        { "timezone",  time_timezone,  0, 0, "() -> string", NULL },
    };
    
    count = sizeof(time_funcs) / sizeof(time_funcs[0]);
    for (size_t i = 0; i < count; i++) {
        AsteronResult res = asteron_register_native(rt, &time_funcs[i]);
        if (res != ASTERON_OK) {
            return res;
        }
    }
    
    /* === MÓDULO OS === */
    AsteronNativeDesc os_funcs[] = {
        { "platform",     os_platform,     0, 0, "() -> string", NULL },
        { "arch",         os_arch,         0, 0, "() -> string", NULL },
        { "os_hostname",  os_hostname,     0, 0, "() -> string", NULL },
        { "username",     os_username,     0, 0, "() -> string", NULL },
        { "homedir",      os_homedir,      0, 0, "() -> string", NULL },
        { "tmpdir",       os_tmpdir,       0, 0, "() -> string", NULL },
        { "cpus",         os_cpus,         0, 0, "() -> number", NULL },
        { "memory_total", os_memory_total, 0, 0, "() -> number", NULL },
        { "memory_free",  os_memory_free,  0, 0, "() -> number", NULL },
        { "uptime",       os_uptime,       0, 0, "() -> number", NULL },
        { "loadavg",      os_loadavg,      0, 0, "() -> array", NULL },
        { "env",          os_env,          1, 1, "(name: string) -> string | nil", NULL },
        { "env_set",      os_env_set,      2, 2, "(name: string, value: string) -> bool", NULL },
        { "env_unset",    os_env_unset,    1, 1, "(name: string) -> bool", NULL },
        { "pid",          os_pid,          0, 0, "() -> number", NULL },
        { "ppid",         os_ppid,         0, 0, "() -> number", NULL },
        { "exit",         os_exit_fn,      0, 1, "(code?: number) -> never", NULL },
        { "kill",         os_kill,         1, 2, "(pid: number, signal?: number) -> bool", NULL },
    };
    
    count = sizeof(os_funcs) / sizeof(os_funcs[0]);
    for (size_t i = 0; i < count; i++) {
        AsteronResult res = asteron_register_native(rt, &os_funcs[i]);
        if (res != ASTERON_OK) {
            return res;
        }
    }
    
    /* === MÓDULO MATH === */
    AsteronNativeDesc math_funcs[] = {
        { "sin",       math_sin,       1, 1, "(x: number) -> number", NULL },
        { "cos",       math_cos,       1, 1, "(x: number) -> number", NULL },
        { "tan",       math_tan,       1, 1, "(x: number) -> number", NULL },
        { "asin",      math_asin,      1, 1, "(x: number) -> number", NULL },
        { "acos",      math_acos,      1, 1, "(x: number) -> number", NULL },
        { "atan",      math_atan,      1, 1, "(x: number) -> number", NULL },
        { "atan2",     math_atan2,     2, 2, "(y: number, x: number) -> number", NULL },
        { "sinh",      math_sinh,      1, 1, "(x: number) -> number", NULL },
        { "cosh",      math_cosh,      1, 1, "(x: number) -> number", NULL },
        { "tanh",      math_tanh,      1, 1, "(x: number) -> number", NULL },
        { "sqrt",      math_sqrt,      1, 1, "(x: number) -> number", NULL },
        { "cbrt",      math_cbrt,      1, 1, "(x: number) -> number", NULL },
        { "pow",       math_pow,       2, 2, "(base: number, exp: number) -> number", NULL },
        { "exp",       math_exp,       1, 1, "(x: number) -> number", NULL },
        { "log",       math_log,       1, 1, "(x: number) -> number", NULL },
        { "log10",     math_log10,     1, 1, "(x: number) -> number", NULL },
        { "log2",      math_log2,      1, 1, "(x: number) -> number", NULL },
        { "floor",     math_floor,     1, 1, "(x: number) -> number", NULL },
        { "ceil",      math_ceil,      1, 1, "(x: number) -> number", NULL },
        { "round",     math_round,     1, 1, "(x: number) -> number", NULL },
        { "trunc",     math_trunc,     1, 1, "(x: number) -> number", NULL },
        { "abs",       math_abs,       1, 1, "(x: number) -> number", NULL },
        { "sign",      math_sign,      1, 1, "(x: number) -> number", NULL },
        { "min",       math_min,       2, 2, "(a: number, b: number) -> number", NULL },
        { "max",       math_max,       2, 2, "(a: number, b: number) -> number", NULL },
        { "clamp",     math_clamp,     3, 3, "(x: number, min: number, max: number) -> number", NULL },
        { "lerp",      math_lerp,      3, 3, "(a: number, b: number, t: number) -> number", NULL },
        { "hypot",     math_hypot,     2, 2, "(x: number, y: number) -> number", NULL },
        { "fmod",      math_fmod,      2, 2, "(x: number, y: number) -> number", NULL },
        { "deg",       math_deg,       1, 1, "(radians: number) -> number", NULL },
        { "rad",       math_rad,       1, 1, "(degrees: number) -> number", NULL },
        { "random",    math_random,    0, 0, "() -> number", NULL },
    };
    
    count = sizeof(math_funcs) / sizeof(math_funcs[0]);
    for (size_t i = 0; i < count; i++) {
        AsteronResult res = asteron_register_native(rt, &math_funcs[i]);
        if (res != ASTERON_OK) {
            return res;
        }
    }
    
    /* === MÓDULO GRAPH === */
    /* Registra módulo graph como built-in (as funções são registradas pelo módulo) */
    graph_module_register();
    
    return ASTERON_OK;
}
