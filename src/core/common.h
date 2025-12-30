#ifndef ASTERON_COMMON_H
#define ASTERON_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Definição de tipos internos da linguagem
typedef enum {
    VAL_NIL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_OBJ,      // Para tipos complexos (String, List, Map, Function)
    VAL_NATIVE_PTR // Para ponteiros de C (interop)
} AsteronValueType;

// Tipos de Objetos (Base para RefCounting)
typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_MODULE,
    OBJ_NATIVE_FUNC
} AsteronObjType;

// Cabeçalho de objeto para RefCounting
typedef struct AsteronObj {
    AsteronObjType type;
    uint32_t ref_count;
    bool is_readonly; // Para evitar modificações em objetos "congelados"
} AsteronObj;

// A unidade fundamental de dados da linguagem (ABI)
typedef struct {
    AsteronValueType type;
    union {
        double number;
        bool boolean;
        AsteronObj* obj;
        void* ptr;
    } as;
} AsteronValue;

// Convenção de chamada para funções nativas (C -> Asteron)
// Recebe o número de argumentos e um array de valores
typedef AsteronValue (*AsteronNativeFn)(int arg_count, AsteronValue* args);

#endif // ASTERON_COMMON_H

