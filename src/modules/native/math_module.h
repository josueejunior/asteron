/**
 * =============================================================================
 * ASTERON MATH MODULE v1.0.0
 * =============================================================================
 * 
 * Módulo oficial de funções matemáticas.
 * 
 * FUNÇÕES EXPORTADAS:
 * 
 * Trigonometria:
 *   sin, cos, tan, asin, acos, atan, atan2
 * 
 * Hiperbólicas:
 *   sinh, cosh, tanh
 * 
 * Exponenciais e Logaritmos:
 *   exp, log, log10, log2, pow, sqrt, cbrt
 * 
 * Arredondamento:
 *   floor, ceil, round, trunc
 * 
 * Utilidades:
 *   abs, sign, min, max, clamp, lerp, hypot, fmod
 * 
 * Conversão:
 *   deg, rad
 * 
 * Aleatório:
 *   random
 * 
 * CONSTANTES:
 *   PI, E, TAU, SQRT2, LN2, LN10, INF, NAN
 * 
 * =============================================================================
 */

#ifndef ASTERON_MATH_MODULE_H
#define ASTERON_MATH_MODULE_H

#include "../module.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * API DO MÓDULO
 * ============================================================================= */

extern NativeModuleDesc math_module;
void math_module_register(void);

/* =============================================================================
 * FUNÇÕES NATIVAS
 * ============================================================================= */

/* Trigonometria */
AsteronValue math_sin(int argc, AsteronValue* args);
AsteronValue math_cos(int argc, AsteronValue* args);
AsteronValue math_tan(int argc, AsteronValue* args);
AsteronValue math_asin(int argc, AsteronValue* args);
AsteronValue math_acos(int argc, AsteronValue* args);
AsteronValue math_atan(int argc, AsteronValue* args);
AsteronValue math_atan2(int argc, AsteronValue* args);

/* Hiperbólicas */
AsteronValue math_sinh(int argc, AsteronValue* args);
AsteronValue math_cosh(int argc, AsteronValue* args);
AsteronValue math_tanh(int argc, AsteronValue* args);

/* Exponenciais e Logaritmos */
AsteronValue math_exp(int argc, AsteronValue* args);
AsteronValue math_log(int argc, AsteronValue* args);
AsteronValue math_log10(int argc, AsteronValue* args);
AsteronValue math_log2(int argc, AsteronValue* args);
AsteronValue math_pow(int argc, AsteronValue* args);
AsteronValue math_sqrt(int argc, AsteronValue* args);
AsteronValue math_cbrt(int argc, AsteronValue* args);

/* Arredondamento */
AsteronValue math_floor(int argc, AsteronValue* args);
AsteronValue math_ceil(int argc, AsteronValue* args);
AsteronValue math_round(int argc, AsteronValue* args);
AsteronValue math_trunc(int argc, AsteronValue* args);

/* Utilidades */
AsteronValue math_abs(int argc, AsteronValue* args);
AsteronValue math_sign(int argc, AsteronValue* args);
AsteronValue math_min(int argc, AsteronValue* args);
AsteronValue math_max(int argc, AsteronValue* args);
AsteronValue math_clamp(int argc, AsteronValue* args);
AsteronValue math_lerp(int argc, AsteronValue* args);
AsteronValue math_hypot(int argc, AsteronValue* args);
AsteronValue math_fmod(int argc, AsteronValue* args);

/* Conversão */
AsteronValue math_deg(int argc, AsteronValue* args);
AsteronValue math_rad(int argc, AsteronValue* args);

/* Aleatório */
AsteronValue math_random(int argc, AsteronValue* args);

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_MATH_MODULE_H */

