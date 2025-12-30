/**
 * =============================================================================
 * ASTERON TIME MODULE v1.0.0
 * =============================================================================
 * 
 * Módulo oficial para operações de tempo e data.
 * 
 * FUNÇÕES EXPORTADAS:
 * 
 * Tempo:
 *   time.now()              -> number (timestamp ms)
 *   time.now_ns()           -> number (timestamp ns)
 *   time.monotonic()        -> number (monotonic clock)
 *   time.sleep(ms)          -> nil
 *   time.sleep_ns(ns)       -> nil
 * 
 * Medição:
 *   time.measure(fn)        -> number (duração em ms)
 *   time.instant()          -> handle (Instant)
 *   time.elapsed(instant)   -> number (ms desde instant)
 * 
 * Data/Hora:
 *   time.date()             -> { year, month, day, ... }
 *   time.format(ts, fmt)    -> string
 *   time.parse(str, fmt)    -> number | error
 * 
 * Utilitários:
 *   time.timezone()         -> string
 *   time.is_dst()           -> bool
 * 
 * CONSTANTES:
 *   time.SECOND             -> 1000
 *   time.MINUTE             -> 60000
 *   time.HOUR               -> 3600000
 *   time.DAY                -> 86400000
 * 
 * =============================================================================
 */

#ifndef ASTERON_TIME_MODULE_H
#define ASTERON_TIME_MODULE_H

#include "../module.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * API DO MÓDULO
 * ============================================================================= */

extern NativeModuleDesc time_module;
void time_module_register(void);

/* =============================================================================
 * FUNÇÕES NATIVAS
 * ============================================================================= */

/* Tempo */
AsteronValue time_now(int argc, AsteronValue* args);
AsteronValue time_now_ns(int argc, AsteronValue* args);
AsteronValue time_monotonic(int argc, AsteronValue* args);
AsteronValue time_sleep(int argc, AsteronValue* args);
AsteronValue time_sleep_ns(int argc, AsteronValue* args);

/* Medição */
AsteronValue time_instant(int argc, AsteronValue* args);
AsteronValue time_elapsed(int argc, AsteronValue* args);

/* Data/Hora */
AsteronValue time_date(int argc, AsteronValue* args);
AsteronValue time_format(int argc, AsteronValue* args);
AsteronValue time_parse(int argc, AsteronValue* args);
AsteronValue time_year(int argc, AsteronValue* args);
AsteronValue time_month(int argc, AsteronValue* args);
AsteronValue time_day(int argc, AsteronValue* args);
AsteronValue time_hour(int argc, AsteronValue* args);
AsteronValue time_minute(int argc, AsteronValue* args);
AsteronValue time_second(int argc, AsteronValue* args);
AsteronValue time_weekday(int argc, AsteronValue* args);

/* Utilitários */
AsteronValue time_timezone(int argc, AsteronValue* args);
AsteronValue time_is_dst(int argc, AsteronValue* args);

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_TIME_MODULE_H */

