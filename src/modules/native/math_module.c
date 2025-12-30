/**
 * =============================================================================
 * MÓDULO MATH - Funções Matemáticas
 * =============================================================================
 * 
 * Fornece funções matemáticas comuns para Asteron.
 * 
 * Uso:
 *   import math
 *   let x = sin(3.14159)
 *   let y = sqrt(16)
 * 
 * =============================================================================
 */

#include "math_module.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Macro para erro */
#define MATH_ERROR() ASTERON_ERROR_VAL(ASTERON_ERROR_INVALID)

/* =============================================================================
 * FUNÇÕES MATEMÁTICAS
 * ============================================================================= */

AsteronValue math_sin(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return MATH_ERROR();
    }
    return ASTERON_NUMBER(sin(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_cos(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return MATH_ERROR();
    }
    return ASTERON_NUMBER(cos(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_tan(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return MATH_ERROR();
    }
    return ASTERON_NUMBER(tan(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_asin(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return MATH_ERROR();
    }
    return ASTERON_NUMBER(asin(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_acos(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return MATH_ERROR();
    }
    return ASTERON_NUMBER(acos(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_atan(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return MATH_ERROR();
    }
    return ASTERON_NUMBER(atan(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_atan2(int argc, AsteronValue* args) {
    if (argc < 2 || !ASTERON_IS_NUMBER(args[0]) || !ASTERON_IS_NUMBER(args[1])) {
        return MATH_ERROR();
    }
    return ASTERON_NUMBER(atan2(ASTERON_AS_NUMBER(args[0]), ASTERON_AS_NUMBER(args[1])));
}

AsteronValue math_sqrt(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return MATH_ERROR();
    }
    double x = ASTERON_AS_NUMBER(args[0]);
    if (x < 0) return MATH_ERROR();
    return ASTERON_NUMBER(sqrt(x));
}

AsteronValue math_pow(int argc, AsteronValue* args) {
    if (argc < 2 || !ASTERON_IS_NUMBER(args[0]) || !ASTERON_IS_NUMBER(args[1])) {
        return MATH_ERROR();
    }
    return ASTERON_NUMBER(pow(ASTERON_AS_NUMBER(args[0]), ASTERON_AS_NUMBER(args[1])));
}

AsteronValue math_exp(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return MATH_ERROR();
    }
    return ASTERON_NUMBER(exp(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_log(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return MATH_ERROR();
    }
    double x = ASTERON_AS_NUMBER(args[0]);
    if (x <= 0) return MATH_ERROR();
    return ASTERON_NUMBER(log(x));
}

AsteronValue math_log10(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return MATH_ERROR();
    }
    double x = ASTERON_AS_NUMBER(args[0]);
    if (x <= 0) return MATH_ERROR();
    return ASTERON_NUMBER(log10(x));
}

AsteronValue math_floor(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return MATH_ERROR();
    }
    return ASTERON_NUMBER(floor(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_ceil(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return MATH_ERROR();
    }
    return ASTERON_NUMBER(ceil(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_round(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return MATH_ERROR();
    }
    return ASTERON_NUMBER(round(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_abs(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return MATH_ERROR();
    }
    return ASTERON_NUMBER(fabs(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_min(int argc, AsteronValue* args) {
    if (argc < 2 || !ASTERON_IS_NUMBER(args[0]) || !ASTERON_IS_NUMBER(args[1])) {
        return MATH_ERROR();
    }
    double a = ASTERON_AS_NUMBER(args[0]);
    double b = ASTERON_AS_NUMBER(args[1]);
    return ASTERON_NUMBER(a < b ? a : b);
}

AsteronValue math_max(int argc, AsteronValue* args) {
    if (argc < 2 || !ASTERON_IS_NUMBER(args[0]) || !ASTERON_IS_NUMBER(args[1])) {
        return MATH_ERROR();
    }
    double a = ASTERON_AS_NUMBER(args[0]);
    double b = ASTERON_AS_NUMBER(args[1]);
    return ASTERON_NUMBER(a > b ? a : b);
}

AsteronValue math_random(int argc, AsteronValue* args) {
    (void)argc;
    (void)args;
    return ASTERON_NUMBER((double)rand() / RAND_MAX);
}

/* Funções hiperbólicas */
AsteronValue math_sinh(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) return MATH_ERROR();
    return ASTERON_NUMBER(sinh(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_cosh(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) return MATH_ERROR();
    return ASTERON_NUMBER(cosh(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_tanh(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) return MATH_ERROR();
    return ASTERON_NUMBER(tanh(ASTERON_AS_NUMBER(args[0])));
}

/* Funções extras */
AsteronValue math_log2(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) return MATH_ERROR();
    double x = ASTERON_AS_NUMBER(args[0]);
    if (x <= 0) return MATH_ERROR();
    return ASTERON_NUMBER(log2(x));
}

AsteronValue math_cbrt(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) return MATH_ERROR();
    return ASTERON_NUMBER(cbrt(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_hypot(int argc, AsteronValue* args) {
    if (argc < 2 || !ASTERON_IS_NUMBER(args[0]) || !ASTERON_IS_NUMBER(args[1])) {
        return MATH_ERROR();
    }
    return ASTERON_NUMBER(hypot(ASTERON_AS_NUMBER(args[0]), ASTERON_AS_NUMBER(args[1])));
}

AsteronValue math_sign(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) return MATH_ERROR();
    double x = ASTERON_AS_NUMBER(args[0]);
    return ASTERON_NUMBER(x > 0 ? 1.0 : (x < 0 ? -1.0 : 0.0));
}

AsteronValue math_clamp(int argc, AsteronValue* args) {
    if (argc < 3 || !ASTERON_IS_NUMBER(args[0]) || 
        !ASTERON_IS_NUMBER(args[1]) || !ASTERON_IS_NUMBER(args[2])) {
        return MATH_ERROR();
    }
    double x = ASTERON_AS_NUMBER(args[0]);
    double lo = ASTERON_AS_NUMBER(args[1]);
    double hi = ASTERON_AS_NUMBER(args[2]);
    if (x < lo) return ASTERON_NUMBER(lo);
    if (x > hi) return ASTERON_NUMBER(hi);
    return ASTERON_NUMBER(x);
}

AsteronValue math_lerp(int argc, AsteronValue* args) {
    if (argc < 3 || !ASTERON_IS_NUMBER(args[0]) || 
        !ASTERON_IS_NUMBER(args[1]) || !ASTERON_IS_NUMBER(args[2])) {
        return MATH_ERROR();
    }
    double a = ASTERON_AS_NUMBER(args[0]);
    double b = ASTERON_AS_NUMBER(args[1]);
    double t = ASTERON_AS_NUMBER(args[2]);
    return ASTERON_NUMBER(a + (b - a) * t);
}

AsteronValue math_trunc(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) return MATH_ERROR();
    return ASTERON_NUMBER(trunc(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_fmod(int argc, AsteronValue* args) {
    if (argc < 2 || !ASTERON_IS_NUMBER(args[0]) || !ASTERON_IS_NUMBER(args[1])) {
        return MATH_ERROR();
    }
    return ASTERON_NUMBER(fmod(ASTERON_AS_NUMBER(args[0]), ASTERON_AS_NUMBER(args[1])));
}

AsteronValue math_deg(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) return MATH_ERROR();
    return ASTERON_NUMBER(ASTERON_AS_NUMBER(args[0]) * 180.0 / 3.14159265358979323846);
}

AsteronValue math_rad(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) return MATH_ERROR();
    return ASTERON_NUMBER(ASTERON_AS_NUMBER(args[0]) * 3.14159265358979323846 / 180.0);
}

AsteronValue math_is_nan(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) return ASTERON_BOOL(false);
    return ASTERON_BOOL(isnan(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_is_inf(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) return ASTERON_BOOL(false);
    return ASTERON_BOOL(isinf(ASTERON_AS_NUMBER(args[0])));
}

AsteronValue math_is_finite(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) return ASTERON_BOOL(false);
    return ASTERON_BOOL(isfinite(ASTERON_AS_NUMBER(args[0])));
}

/* =============================================================================
 * DEFINIÇÃO DO MÓDULO
 * ============================================================================= */

static ModuleExport math_exports[] = {
    /* Trigonométricas */
    { "sin",   math_sin,   ASTERON_NIL(), 1, 1, "number -> number" },
    { "cos",   math_cos,   ASTERON_NIL(), 1, 1, "number -> number" },
    { "tan",   math_tan,   ASTERON_NIL(), 1, 1, "number -> number" },
    { "asin",  math_asin,  ASTERON_NIL(), 1, 1, "number -> number" },
    { "acos",  math_acos,  ASTERON_NIL(), 1, 1, "number -> number" },
    { "atan",  math_atan,  ASTERON_NIL(), 1, 1, "number -> number" },
    { "atan2", math_atan2, ASTERON_NIL(), 1, 2, "number, number -> number" },
    
    /* Exponenciais e logaritmos */
    { "sqrt",  math_sqrt,  ASTERON_NIL(), 1, 1, "number -> number" },
    { "pow",   math_pow,   ASTERON_NIL(), 1, 2, "number, number -> number" },
    { "exp",   math_exp,   ASTERON_NIL(), 1, 1, "number -> number" },
    { "log",   math_log,   ASTERON_NIL(), 1, 1, "number -> number" },
    { "log10", math_log10, ASTERON_NIL(), 1, 1, "number -> number" },
    
    /* Arredondamento */
    { "floor", math_floor, ASTERON_NIL(), 1, 1, "number -> number" },
    { "ceil",  math_ceil,  ASTERON_NIL(), 1, 1, "number -> number" },
    { "round", math_round, ASTERON_NIL(), 1, 1, "number -> number" },
    
    /* Hiperbólicas */
    { "sinh",  math_sinh,  ASTERON_NIL(), 1, 1, "(x: number) -> number" },
    { "cosh",  math_cosh,  ASTERON_NIL(), 1, 1, "(x: number) -> number" },
    { "tanh",  math_tanh,  ASTERON_NIL(), 1, 1, "(x: number) -> number" },
    
    /* Raízes e logaritmos extras */
    { "cbrt",  math_cbrt,  ASTERON_NIL(), 1, 1, "(x: number) -> number" },
    { "log2",  math_log2,  ASTERON_NIL(), 1, 1, "(x: number) -> number" },
    { "hypot", math_hypot, ASTERON_NIL(), 2, 2, "(x: number, y: number) -> number" },
    
    /* Utilitárias */
    { "abs",    math_abs,    ASTERON_NIL(), 1, 1, "(x: number) -> number" },
    { "min",    math_min,    ASTERON_NIL(), 2, 2, "(a: number, b: number) -> number" },
    { "max",    math_max,    ASTERON_NIL(), 2, 2, "(a: number, b: number) -> number" },
    { "sign",   math_sign,   ASTERON_NIL(), 1, 1, "(x: number) -> number (-1, 0, 1)" },
    { "clamp",  math_clamp,  ASTERON_NIL(), 3, 3, "(x: number, min: number, max: number) -> number" },
    { "lerp",   math_lerp,   ASTERON_NIL(), 3, 3, "(a: number, b: number, t: number) -> number" },
    { "trunc",  math_trunc,  ASTERON_NIL(), 1, 1, "(x: number) -> number" },
    { "fmod",   math_fmod,   ASTERON_NIL(), 2, 2, "(x: number, y: number) -> number" },
    { "random", math_random, ASTERON_NIL(), 0, 0, "() -> number [0, 1)" },
    
    /* Conversões */
    { "deg",    math_deg,    ASTERON_NIL(), 1, 1, "(radians: number) -> number (degrees)" },
    { "rad",    math_rad,    ASTERON_NIL(), 1, 1, "(degrees: number) -> number (radians)" },
    
    /* Verificações */
    { "is_nan",    math_is_nan,    ASTERON_NIL(), 1, 1, "(x: number) -> bool" },
    { "is_inf",    math_is_inf,    ASTERON_NIL(), 1, 1, "(x: number) -> bool" },
    { "is_finite", math_is_finite, ASTERON_NIL(), 1, 1, "(x: number) -> bool" },
    
    /* Constantes */
    { "PI",      NULL, { .type = ASTERON_VAL_NUMBER, .as.number = 3.14159265358979323846 }, 0, 0, NULL },
    { "E",       NULL, { .type = ASTERON_VAL_NUMBER, .as.number = 2.71828182845904523536 }, 0, 0, NULL },
    { "TAU",     NULL, { .type = ASTERON_VAL_NUMBER, .as.number = 6.28318530717958647692 }, 0, 0, NULL },
    { "INF",     NULL, { .type = ASTERON_VAL_NUMBER, .as.number = 1.0/0.0 }, 0, 0, NULL },
    { "NAN",     NULL, { .type = ASTERON_VAL_NUMBER, .as.number = 0.0/0.0 }, 0, 0, NULL },
    { "SQRT2",   NULL, { .type = ASTERON_VAL_NUMBER, .as.number = 1.41421356237309504880 }, 0, 0, NULL },
    { "LN2",     NULL, { .type = ASTERON_VAL_NUMBER, .as.number = 0.69314718055994530942 }, 0, 0, NULL },
    { "LN10",    NULL, { .type = ASTERON_VAL_NUMBER, .as.number = 2.30258509299404568402 }, 0, 0, NULL },
};

NativeModuleDesc math_module = {
    .name = "math",
    .version = "1.0.0",
    .description = "Funções matemáticas padrão",
    .exports = math_exports,
    .export_count = sizeof(math_exports) / sizeof(math_exports[0]),
    .init = NULL,
    .cleanup = NULL
};

/* Função de registro (chamada pelo loader) */
void math_module_register(void) {
    module_register_builtin(&math_module);
}

