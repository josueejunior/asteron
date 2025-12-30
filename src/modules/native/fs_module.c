/**
 * =============================================================================
 * ASTERON FS MODULE - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "fs_module.h"
#include "../../sys/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

/* =============================================================================
 * HELPERS
 * ============================================================================= */

static AsteronValue make_error(AsteronResult code) {
    return ASTERON_ERROR_VAL(code);
}

static const char* get_string_arg(AsteronValue* args, int index) {
    // Verifica nil primeiro (segurança crítica)
    if (ASTERON_IS_NIL(args[index])) return NULL;
    if (!ASTERON_IS_STRING(args[index])) return NULL;
    AsteronString* str = ASTERON_AS_STRING(args[index]);
    return str ? str->chars : NULL;
}

/* =============================================================================
 * fs.read(path) -> string | error
 * ============================================================================= */

AsteronValue fs_read(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_STRING(args[0])) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    const char* path = get_string_arg(args, 0);
    if (!path) return make_error(ASTERON_ERROR_INVALID);
    
    FILE* f = fopen(path, "rb");
    if (!f) {
        if (errno == ENOENT) return make_error(ASTERON_ERROR_NOT_FOUND);
        if (errno == EACCES) return make_error(ASTERON_ERROR_PERM);
        return make_error(ASTERON_ERROR_IO);
    }
    
    /* Obtém tamanho */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size < 0 || size > 1024 * 1024 * 100) { /* Max 100MB */
        fclose(f);
        return make_error(ASTERON_ERROR_OVERFLOW);
    }
    
    /* Aloca buffer */
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return make_error(ASTERON_ERROR_MEMORY);
    }
    
    /* Lê conteúdo */
    size_t read = fread(buffer, 1, size, f);
    buffer[read] = '\0';
    fclose(f);
    
    /* Cria AsteronString */
    AsteronString* str = (AsteronString*)malloc(sizeof(AsteronString) + read + 1);
    if (!str) {
        free(buffer);
        return make_error(ASTERON_ERROR_MEMORY);
    }
    
    str->header.obj_type = ASTERON_OBJ_STRING;
    str->header.flags = 0;
    str->header.ref_count = 1;
    str->header.next = NULL;
    str->length = read;
    str->hash = 0;
    str->capacity = read + 1;
    memcpy(str->chars, buffer, read + 1);
    
    free(buffer);
    
    return ASTERON_PTR(str, ASTERON_VAL_STRING);
}

/* =============================================================================
 * fs.write(path, data) -> bool | error
 * ============================================================================= */

AsteronValue fs_write(int argc, AsteronValue* args) {
    if (argc < 2) return make_error(ASTERON_ERROR_INVALID);
    
    const char* path = get_string_arg(args, 0);
    if (!path) return make_error(ASTERON_ERROR_INVALID);
    
    const char* data = NULL;
    size_t len = 0;
    
    if (ASTERON_IS_STRING(args[1])) {
        AsteronString* str = ASTERON_AS_STRING(args[1]);
        data = str->chars;
        len = str->length;
    } else {
        return make_error(ASTERON_ERROR_TYPE);
    }
    
    FILE* f = fopen(path, "wb");
    if (!f) {
        if (errno == EACCES) return make_error(ASTERON_ERROR_PERM);
        return make_error(ASTERON_ERROR_IO);
    }
    
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    
    if (written != len) {
        return make_error(ASTERON_ERROR_IO);
    }
    
    return ASTERON_BOOL(true);
}

/* =============================================================================
 * fs.append(path, data) -> bool | error
 * ============================================================================= */

AsteronValue fs_append(int argc, AsteronValue* args) {
    if (argc < 2) return make_error(ASTERON_ERROR_INVALID);
    
    const char* path = get_string_arg(args, 0);
    const char* data = get_string_arg(args, 1);
    
    if (!path || !data) return make_error(ASTERON_ERROR_INVALID);
    
    FILE* f = fopen(path, "ab");
    if (!f) return make_error(ASTERON_ERROR_IO);
    
    AsteronString* str = ASTERON_AS_STRING(args[1]);
    size_t written = fwrite(data, 1, str->length, f);
    fclose(f);
    
    return ASTERON_BOOL(written == str->length);
}

/* =============================================================================
 * fs.exists(path) -> bool
 * ============================================================================= */

AsteronValue fs_exists(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_BOOL(false);
    
    const char* path = get_string_arg(args, 0);
    if (!path) return ASTERON_BOOL(false);
    
    struct stat st;
    return ASTERON_BOOL(stat(path, &st) == 0);
}

/* =============================================================================
 * fs.remove(path) -> bool | error
 * ============================================================================= */

AsteronValue fs_remove(int argc, AsteronValue* args) {
    if (argc < 1) return make_error(ASTERON_ERROR_INVALID);
    
    const char* path = get_string_arg(args, 0);
    if (!path) return make_error(ASTERON_ERROR_INVALID);
    
    if (unlink(path) != 0) {
        if (errno == ENOENT) return make_error(ASTERON_ERROR_NOT_FOUND);
        if (errno == EACCES) return make_error(ASTERON_ERROR_PERM);
        return make_error(ASTERON_ERROR_IO);
    }
    
    return ASTERON_BOOL(true);
}

/* =============================================================================
 * fs.mkdir(path) -> bool | error
 * ============================================================================= */

AsteronValue fs_mkdir(int argc, AsteronValue* args) {
    if (argc < 1) return make_error(ASTERON_ERROR_INVALID);
    
    const char* path = get_string_arg(args, 0);
    if (!path) return make_error(ASTERON_ERROR_INVALID);
    
    #ifdef _WIN32
    int result = mkdir(path);
    #else
    int result = mkdir(path, 0755);
    #endif
    
    if (result != 0) {
        if (errno == EEXIST) return make_error(ASTERON_ERROR_EXISTS);
        if (errno == EACCES) return make_error(ASTERON_ERROR_PERM);
        return make_error(ASTERON_ERROR_IO);
    }
    
    return ASTERON_BOOL(true);
}

/* =============================================================================
 * fs.rmdir(path) -> bool | error
 * ============================================================================= */

AsteronValue fs_rmdir(int argc, AsteronValue* args) {
    if (argc < 1) return make_error(ASTERON_ERROR_INVALID);
    
    const char* path = get_string_arg(args, 0);
    if (!path) return make_error(ASTERON_ERROR_INVALID);
    
    if (rmdir(path) != 0) {
        if (errno == ENOENT) return make_error(ASTERON_ERROR_NOT_FOUND);
        if (errno == EACCES) return make_error(ASTERON_ERROR_PERM);
        if (errno == ENOTEMPTY) return make_error(ASTERON_ERROR_BUSY);
        return make_error(ASTERON_ERROR_IO);
    }
    
    return ASTERON_BOOL(true);
}

/* =============================================================================
 * fs.list(path) -> array | error
 * ============================================================================= */

AsteronValue fs_list(int argc, AsteronValue* args) {
    if (argc < 1) return make_error(ASTERON_ERROR_INVALID);
    
    const char* path = get_string_arg(args, 0);
    if (!path) return make_error(ASTERON_ERROR_INVALID);
    
    DIR* dir = opendir(path);
    if (!dir) {
        if (errno == ENOENT) return make_error(ASTERON_ERROR_NOT_FOUND);
        if (errno == EACCES) return make_error(ASTERON_ERROR_PERM);
        return make_error(ASTERON_ERROR_IO);
    }
    
    /* Cria array para resultados */
    AsteronArray* arr = (AsteronArray*)malloc(sizeof(AsteronArray));
    arr->header.obj_type = ASTERON_OBJ_ARRAY;
    arr->header.flags = 0;
    arr->header.ref_count = 1;
    arr->header.next = NULL;
    arr->capacity = 32;
    arr->length = 0;
    arr->items = (AsteronValue*)malloc(sizeof(AsteronValue) * arr->capacity);
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Ignora . e .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        /* Expande se necessário */
        if (arr->length >= arr->capacity) {
            arr->capacity *= 2;
            arr->items = (AsteronValue*)realloc(arr->items, 
                sizeof(AsteronValue) * arr->capacity);
        }
        
        /* Cria string para o nome */
        size_t namelen = strlen(entry->d_name);
        AsteronString* str = (AsteronString*)malloc(sizeof(AsteronString) + namelen + 1);
        str->header.obj_type = ASTERON_OBJ_STRING;
        str->header.flags = 0;
        str->header.ref_count = 1;
        str->header.next = NULL;
        str->length = namelen;
        str->hash = 0;
        str->capacity = namelen + 1;
        strcpy(str->chars, entry->d_name);
        
        arr->items[arr->length++] = ASTERON_PTR(str, ASTERON_VAL_STRING);
    }
    
    closedir(dir);
    
    return ASTERON_PTR(arr, ASTERON_VAL_ARRAY);
}

/* =============================================================================
 * fs.is_file(path) -> bool
 * ============================================================================= */

AsteronValue fs_is_file(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_BOOL(false);
    
    const char* path = get_string_arg(args, 0);
    if (!path) return ASTERON_BOOL(false);
    
    struct stat st;
    if (stat(path, &st) != 0) return ASTERON_BOOL(false);
    
    return ASTERON_BOOL(S_ISREG(st.st_mode));
}

/* =============================================================================
 * fs.is_dir(path) -> bool
 * ============================================================================= */

AsteronValue fs_is_dir(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_BOOL(false);
    
    const char* path = get_string_arg(args, 0);
    if (!path) return ASTERON_BOOL(false);
    
    struct stat st;
    if (stat(path, &st) != 0) return ASTERON_BOOL(false);
    
    return ASTERON_BOOL(S_ISDIR(st.st_mode));
}

/* =============================================================================
 * fs.size(path) -> number | error
 * ============================================================================= */

AsteronValue fs_size(int argc, AsteronValue* args) {
    if (argc < 1) return make_error(ASTERON_ERROR_INVALID);
    
    const char* path = get_string_arg(args, 0);
    if (!path) return make_error(ASTERON_ERROR_INVALID);
    
    struct stat st;
    if (stat(path, &st) != 0) {
        if (errno == ENOENT) return make_error(ASTERON_ERROR_NOT_FOUND);
        return make_error(ASTERON_ERROR_IO);
    }
    
    return ASTERON_NUMBER((double)st.st_size);
}

/* =============================================================================
 * fs.cwd() -> string
 * ============================================================================= */

AsteronValue fs_cwd(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    char buffer[4096];
    if (getcwd(buffer, sizeof(buffer)) == NULL) {
        return make_error(ASTERON_ERROR_IO);
    }
    
    size_t len = strlen(buffer);
    AsteronString* str = (AsteronString*)malloc(sizeof(AsteronString) + len + 1);
    str->header.obj_type = ASTERON_OBJ_STRING;
    str->header.flags = 0;
    str->header.ref_count = 1;
    str->header.next = NULL;
    str->length = len;
    str->hash = 0;
    str->capacity = len + 1;
    strcpy(str->chars, buffer);
    
    return ASTERON_PTR(str, ASTERON_VAL_STRING);
}

/* =============================================================================
 * fs.chdir(path) -> bool | error
 * ============================================================================= */

AsteronValue fs_chdir(int argc, AsteronValue* args) {
    if (argc < 1) return make_error(ASTERON_ERROR_INVALID);
    
    const char* path = get_string_arg(args, 0);
    if (!path) return make_error(ASTERON_ERROR_INVALID);
    
    if (chdir(path) != 0) {
        if (errno == ENOENT) return make_error(ASTERON_ERROR_NOT_FOUND);
        if (errno == EACCES) return make_error(ASTERON_ERROR_PERM);
        return make_error(ASTERON_ERROR_IO);
    }
    
    return ASTERON_BOOL(true);
}

/* =============================================================================
 * fs.copy(src, dst) -> bool | error
 * ============================================================================= */

AsteronValue fs_copy(int argc, AsteronValue* args) {
    if (argc < 2) return make_error(ASTERON_ERROR_INVALID);
    
    const char* src = get_string_arg(args, 0);
    const char* dst = get_string_arg(args, 1);
    
    if (!src || !dst) return make_error(ASTERON_ERROR_INVALID);
    
    FILE* fsrc = fopen(src, "rb");
    if (!fsrc) {
        if (errno == ENOENT) return make_error(ASTERON_ERROR_NOT_FOUND);
        return make_error(ASTERON_ERROR_IO);
    }
    
    FILE* fdst = fopen(dst, "wb");
    if (!fdst) {
        fclose(fsrc);
        return make_error(ASTERON_ERROR_IO);
    }
    
    char buffer[8192];
    size_t bytes;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), fsrc)) > 0) {
        if (fwrite(buffer, 1, bytes, fdst) != bytes) {
            fclose(fsrc);
            fclose(fdst);
            return make_error(ASTERON_ERROR_IO);
        }
    }
    
    fclose(fsrc);
    fclose(fdst);
    
    return ASTERON_BOOL(true);
}

/* =============================================================================
 * fs.move(src, dst) -> bool | error
 * ============================================================================= */

AsteronValue fs_move(int argc, AsteronValue* args) {
    if (argc < 2) return make_error(ASTERON_ERROR_INVALID);
    
    const char* src = get_string_arg(args, 0);
    const char* dst = get_string_arg(args, 1);
    
    if (!src || !dst) return make_error(ASTERON_ERROR_INVALID);
    
    if (rename(src, dst) != 0) {
        /* Tenta copy + delete */
        AsteronValue copy_result = fs_copy(argc, args);
        if (ASTERON_IS_ERROR(copy_result)) return copy_result;
        
        unlink(src);
    }
    
    return ASTERON_BOOL(true);
}

/* Funções não implementadas ainda retornam NIL */
AsteronValue fs_stat(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }
AsteronValue fs_open(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }
AsteronValue fs_close(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }
AsteronValue fs_read_bytes(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }
AsteronValue fs_write_bytes(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }
AsteronValue fs_read_line(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }

/* =============================================================================
 * DESCRITOR DO MÓDULO
 * ============================================================================= */

static ModuleExport fs_exports[] = {
    { "read",      fs_read,      ASTERON_NIL(), 1, 1, "(path: string) -> string" },
    { "write",     fs_write,     ASTERON_NIL(), 2, 2, "(path: string, data: string) -> bool" },
    { "append",    fs_append,    ASTERON_NIL(), 2, 2, "(path: string, data: string) -> bool" },
    { "exists",    fs_exists,    ASTERON_NIL(), 1, 1, "(path: string) -> bool" },
    { "remove",    fs_remove,    ASTERON_NIL(), 1, 1, "(path: string) -> bool" },
    { "mkdir",     fs_mkdir,     ASTERON_NIL(), 1, 1, "(path: string) -> bool" },
    { "rmdir",     fs_rmdir,     ASTERON_NIL(), 1, 1, "(path: string) -> bool" },
    { "list",      fs_list,      ASTERON_NIL(), 1, 1, "(path: string) -> array" },
    { "is_file",   fs_is_file,   ASTERON_NIL(), 1, 1, "(path: string) -> bool" },
    { "is_dir",    fs_is_dir,    ASTERON_NIL(), 1, 1, "(path: string) -> bool" },
    { "size",      fs_size,      ASTERON_NIL(), 1, 1, "(path: string) -> number" },
    { "cwd",       fs_cwd,       ASTERON_NIL(), 0, 0, "() -> string" },
    { "chdir",     fs_chdir,     ASTERON_NIL(), 1, 1, "(path: string) -> bool" },
    { "copy",      fs_copy,      ASTERON_NIL(), 2, 2, "(src: string, dst: string) -> bool" },
    { "move",      fs_move,      ASTERON_NIL(), 2, 2, "(src: string, dst: string) -> bool" },
};

NativeModuleDesc fs_module = {
    .name = "fs",
    .version = "1.0.0",
    .description = "Operações de sistema de arquivos",
    .exports = fs_exports,
    .export_count = sizeof(fs_exports) / sizeof(fs_exports[0]),
    .init = NULL,
    .cleanup = NULL
};

void fs_module_register(void) {
    module_register_builtin(&fs_module);
}

