#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

// Funções auxiliares para strings
char* string_copy(const char* source, size_t length);
void string_free(char* str);

/**
 * Processa escape sequences em uma string.
 * Converte \n, \r, \t, \\, \", etc. para os caracteres reais.
 * 
 * @param source String fonte (com aspas ou sem)
 * @param length Tamanho da string fonte
 * @return Nova string com escapes processados (deve ser liberada com string_free)
 */
char* string_unescape(const char* source, size_t length);

// Funções de erro
void error_report(int line, const char* message);
void error_at(const char* location, const char* message);

// Arena de Memória
typedef struct {
    void* data;
    size_t size;
    size_t capacity;
} MemoryArena;

MemoryArena* arena_create(size_t capacity);
void arena_destroy(MemoryArena* arena);
void* arena_alloc(MemoryArena* arena, size_t size);
void arena_reset(MemoryArena* arena);

#endif // UTILS_H



