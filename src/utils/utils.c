#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char* string_copy(const char* source, size_t length) {
    // Proteção contra source NULL
    if (!source) {
        char* copy = (char*)malloc(1);
        if (copy) copy[0] = '\0';
        return copy;
    }
    char* copy = (char*)malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, source, length);
    copy[length] = '\0';
    return copy;
}

void string_free(char* str) {
    if (str != NULL) {
        free(str);
    }
}

void error_report(int line, const char* message) {
    fprintf(stderr, "[Linha %d] Erro: %s\n", line, message);
}

void error_at(const char* location, const char* message) {
    fprintf(stderr, "Erro em '%s': %s\n", location, message);
}

// Implementação da Arena de Memória
MemoryArena* arena_create(size_t capacity) {
    MemoryArena* arena = (MemoryArena*)malloc(sizeof(MemoryArena));
    if (arena == NULL) return NULL;
    
    arena->data = malloc(capacity);
    if (arena->data == NULL) {
        free(arena);
        return NULL;
    }
    
    arena->size = 0;
    arena->capacity = capacity;
    return arena;
}

void arena_destroy(MemoryArena* arena) {
    if (arena == NULL) return;
    free(arena->data);
    free(arena);
}

void* arena_alloc(MemoryArena* arena, size_t size) {
    if (arena == NULL || arena->size + size > arena->capacity) {
        return NULL; // Out of memory in arena
    }
    
    void* ptr = (char*)arena->data + arena->size;
    arena->size += size;
    
    // Alinhamento simples (opcional, mas bom)
    size_t alignment = sizeof(void*);
    arena->size = (arena->size + alignment - 1) & ~(alignment - 1);
    
    return ptr;
}

void arena_reset(MemoryArena* arena) {
    if (arena != NULL) {
        arena->size = 0;
    }
}

/**
 * Processa escape sequences em uma string.
 * Suporta: \n \r \t \\ \" \' \0 \x00 (hex)
 */
char* string_unescape(const char* source, size_t length) {
    if (source == NULL || length == 0) {
        return string_copy("", 0);
    }
    
    /* Remove aspas se presentes */
    const char* start = source;
    size_t len = length;
    
    if (len >= 2 && start[0] == '"' && start[len-1] == '"') {
        start++;
        len -= 2;
    }
    
    /* Aloca buffer (pode ser menor após processamento) */
    char* result = (char*)malloc(len + 1);
    if (result == NULL) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (start[i] == '\\' && i + 1 < len) {
            char next = start[i + 1];
            switch (next) {
                case 'n':  result[j++] = '\n'; i++; break;
                case 'r':  result[j++] = '\r'; i++; break;
                case 't':  result[j++] = '\t'; i++; break;
                case '\\': result[j++] = '\\'; i++; break;
                case '"':  result[j++] = '"';  i++; break;
                case '\'': result[j++] = '\''; i++; break;
                case '0':  result[j++] = '\0'; i++; break;
                case 'x':  /* Hex escape \x00 */
                    if (i + 3 < len) {
                        char hex[3] = { start[i+2], start[i+3], '\0' };
                        result[j++] = (char)strtol(hex, NULL, 16);
                        i += 3;
                    } else {
                        result[j++] = start[i];
                    }
                    break;
                default:
                    /* Escape não reconhecido, mantém literal */
                    result[j++] = start[i];
                    break;
            }
        } else {
            result[j++] = start[i];
        }
    }
    
    result[j] = '\0';
    
    /* Realoca para tamanho exato */
    char* final = (char*)realloc(result, j + 1);
    return final ? final : result;
}

