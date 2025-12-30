/**
 * =============================================================================
 * ASTERON TIME MODULE - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "time_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/time.h>
    #include <unistd.h>
#endif

/* =============================================================================
 * HELPERS
 * ============================================================================= */

static AsteronValue make_error(AsteronResult code) {
    return ASTERON_ERROR_VAL(code);
}

static AsteronValue make_string(const char* s) {
    if (!s) return ASTERON_NIL();
    
    size_t len = strlen(s);
    AsteronString* str = (AsteronString*)malloc(sizeof(AsteronString) + len + 1);
    str->header.obj_type = ASTERON_OBJ_STRING;
    str->header.flags = 0;
    str->header.ref_count = 1;
    str->header.next = NULL;
    str->length = len;
    str->hash = 0;
    str->capacity = len + 1;
    strcpy(str->chars, s);
    
    return ASTERON_PTR(str, ASTERON_VAL_STRING);
}

/* =============================================================================
 * time.now() -> number (timestamp em ms)
 * ============================================================================= */

AsteronValue time_now(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t time = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    /* Converte de 100ns desde 1601 para ms desde 1970 */
    time -= 116444736000000000ULL;
    time /= 10000;
    return ASTERON_NUMBER((double)time);
    #else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double ms = (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
    return ASTERON_NUMBER(ms);
    #endif
}

/* =============================================================================
 * time.now_ns() -> number (timestamp em ns)
 * ============================================================================= */

AsteronValue time_now_ns(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    double ns = (double)counter.QuadPart * 1e9 / (double)freq.QuadPart;
    return ASTERON_NUMBER(ns);
    #else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    double ns = (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
    return ASTERON_NUMBER(ns);
    #endif
}

/* =============================================================================
 * time.monotonic() -> number (relógio monotônico em ms)
 * ============================================================================= */

AsteronValue time_monotonic(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    double ms = (double)counter.QuadPart * 1000.0 / (double)freq.QuadPart;
    return ASTERON_NUMBER(ms);
    #else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double ms = (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
    return ASTERON_NUMBER(ms);
    #endif
}

/* =============================================================================
 * time.sleep(ms) -> nil
 * ============================================================================= */

AsteronValue time_sleep(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return ASTERON_NIL();
    }
    
    double ms = ASTERON_AS_NUMBER(args[0]);
    if (ms <= 0) return ASTERON_NIL();
    
    #ifdef _WIN32
    Sleep((DWORD)ms);
    #else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)((ms - ts.tv_sec * 1000) * 1e6);
    nanosleep(&ts, NULL);
    #endif
    
    return ASTERON_NIL();
}

/* =============================================================================
 * time.sleep_ns(ns) -> nil
 * ============================================================================= */

AsteronValue time_sleep_ns(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return ASTERON_NIL();
    }
    
    double ns = ASTERON_AS_NUMBER(args[0]);
    if (ns <= 0) return ASTERON_NIL();
    
    #ifdef _WIN32
    /* Windows não suporta precisão de nanosegundos */
    Sleep((DWORD)(ns / 1e6));
    #else
    struct timespec ts;
    ts.tv_sec = (time_t)(ns / 1e9);
    ts.tv_nsec = (long)(ns - ts.tv_sec * 1e9);
    nanosleep(&ts, NULL);
    #endif
    
    return ASTERON_NIL();
}

/* =============================================================================
 * time.instant() -> handle (Instant para medição)
 * ============================================================================= */

typedef struct {
    #ifdef _WIN32
    LARGE_INTEGER counter;
    #else
    struct timespec ts;
    #endif
} Instant;

AsteronValue time_instant(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    Instant* inst = (Instant*)malloc(sizeof(Instant));
    
    #ifdef _WIN32
    QueryPerformanceCounter(&inst->counter);
    #else
    clock_gettime(CLOCK_MONOTONIC, &inst->ts);
    #endif
    
    AsteronHandle* handle = (AsteronHandle*)malloc(sizeof(AsteronHandle));
    handle->header.obj_type = ASTERON_OBJ_TIMER;
    handle->header.flags = 0;
    handle->header.ref_count = 1;
    handle->header.next = NULL;
    handle->handle_type = HANDLE_TIMER;
    handle->fd = 0;
    handle->data = inst;
    handle->close = free;
    
    return ASTERON_HANDLE(handle, HANDLE_TIMER);
}

/* =============================================================================
 * time.elapsed(instant) -> number (ms desde instant)
 * ============================================================================= */

AsteronValue time_elapsed(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_HANDLE(args[0])) {
        return make_error(ASTERON_ERROR_INVALID);
    }
    
    AsteronHandle* handle = ASTERON_AS_HANDLE(args[0]);
    Instant* start = (Instant*)handle->data;
    
    #ifdef _WIN32
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    double elapsed = (double)(now.QuadPart - start->counter.QuadPart) * 1000.0 / 
                     (double)freq.QuadPart;
    #else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed = (now.tv_sec - start->ts.tv_sec) * 1000.0 +
                     (now.tv_nsec - start->ts.tv_nsec) / 1e6;
    #endif
    
    return ASTERON_NUMBER(elapsed);
}

/* =============================================================================
 * time.year/month/day/hour/minute/second() -> number
 * ============================================================================= */

static struct tm* get_local_time(void) {
    time_t t = time(NULL);
    return localtime(&t);
}

AsteronValue time_year(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    return ASTERON_NUMBER(get_local_time()->tm_year + 1900);
}

AsteronValue time_month(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    return ASTERON_NUMBER(get_local_time()->tm_mon + 1);
}

AsteronValue time_day(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    return ASTERON_NUMBER(get_local_time()->tm_mday);
}

AsteronValue time_hour(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    return ASTERON_NUMBER(get_local_time()->tm_hour);
}

AsteronValue time_minute(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    return ASTERON_NUMBER(get_local_time()->tm_min);
}

AsteronValue time_second(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    return ASTERON_NUMBER(get_local_time()->tm_sec);
}

AsteronValue time_weekday(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    return ASTERON_NUMBER(get_local_time()->tm_wday);
}

/* =============================================================================
 * time.format(timestamp?, format) -> string
 * ============================================================================= */

AsteronValue time_format(int argc, AsteronValue* args) {
    time_t t;
    const char* fmt = "%Y-%m-%d %H:%M:%S";
    
    if (argc == 0) {
        t = time(NULL);
    } else if (argc == 1) {
        if (ASTERON_IS_NUMBER(args[0])) {
            t = (time_t)(ASTERON_AS_NUMBER(args[0]) / 1000);
        } else if (ASTERON_IS_STRING(args[0])) {
            t = time(NULL);
            AsteronString* s = ASTERON_AS_STRING(args[0]);
            fmt = s->chars;
        } else {
            return make_error(ASTERON_ERROR_TYPE);
        }
    } else {
        t = (time_t)(ASTERON_AS_NUMBER(args[0]) / 1000);
        if (ASTERON_IS_STRING(args[1])) {
            AsteronString* s = ASTERON_AS_STRING(args[1]);
            fmt = s->chars;
        }
    }
    
    struct tm* tm = localtime(&t);
    char buffer[256];
    
    strftime(buffer, sizeof(buffer), fmt, tm);
    
    return make_string(buffer);
}

/* =============================================================================
 * time.timezone() -> string
 * ============================================================================= */

AsteronValue time_timezone(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #ifdef _WIN32
    TIME_ZONE_INFORMATION tz;
    GetTimeZoneInformation(&tz);
    /* Simplificado - retorna offset */
    char buf[32];
    snprintf(buf, sizeof(buf), "UTC%+d", -tz.Bias / 60);
    return make_string(buf);
    #else
    /* Usa tzname que é mais portável */
    tzset();
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    return make_string(tzname[tm->tm_isdst > 0 ? 1 : 0]);
    #endif
}

/* =============================================================================
 * time.is_dst() -> bool
 * ============================================================================= */

AsteronValue time_is_dst(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    return ASTERON_BOOL(get_local_time()->tm_isdst > 0);
}

/* Funções não implementadas */
AsteronValue time_date(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }
AsteronValue time_parse(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }

/* =============================================================================
 * DESCRITOR DO MÓDULO
 * ============================================================================= */

static ModuleExport time_exports[] = {
    /* Tempo */
    { "now",       time_now,       ASTERON_NIL(), 0, 0, "() -> number (ms)" },
    { "now_ns",    time_now_ns,    ASTERON_NIL(), 0, 0, "() -> number (ns)" },
    { "monotonic", time_monotonic, ASTERON_NIL(), 0, 0, "() -> number (ms)" },
    { "sleep",     time_sleep,     ASTERON_NIL(), 1, 1, "(ms: number) -> nil" },
    { "sleep_ns",  time_sleep_ns,  ASTERON_NIL(), 1, 1, "(ns: number) -> nil" },
    
    /* Medição */
    { "instant",   time_instant,   ASTERON_NIL(), 0, 0, "() -> handle" },
    { "elapsed",   time_elapsed,   ASTERON_NIL(), 1, 1, "(instant: handle) -> number" },
    
    /* Data/Hora */
    { "year",      time_year,      ASTERON_NIL(), 0, 0, "() -> number" },
    { "month",     time_month,     ASTERON_NIL(), 0, 0, "() -> number" },
    { "day",       time_day,       ASTERON_NIL(), 0, 0, "() -> number" },
    { "hour",      time_hour,      ASTERON_NIL(), 0, 0, "() -> number" },
    { "minute",    time_minute,    ASTERON_NIL(), 0, 0, "() -> number" },
    { "second",    time_second,    ASTERON_NIL(), 0, 0, "() -> number" },
    { "weekday",   time_weekday,   ASTERON_NIL(), 0, 0, "() -> number (0=Sun)" },
    { "format",    time_format,    ASTERON_NIL(), 0, 2, "(ts?, fmt?) -> string" },
    
    /* Utilitários */
    { "timezone",  time_timezone,  ASTERON_NIL(), 0, 0, "() -> string" },
    { "is_dst",    time_is_dst,    ASTERON_NIL(), 0, 0, "() -> bool" },
    
    /* Constantes */
    { "SECOND",    NULL, { .type = ASTERON_VAL_NUMBER, .as.number = 1000 }, 0, 0, NULL },
    { "MINUTE",    NULL, { .type = ASTERON_VAL_NUMBER, .as.number = 60000 }, 0, 0, NULL },
    { "HOUR",      NULL, { .type = ASTERON_VAL_NUMBER, .as.number = 3600000 }, 0, 0, NULL },
    { "DAY",       NULL, { .type = ASTERON_VAL_NUMBER, .as.number = 86400000 }, 0, 0, NULL },
};

NativeModuleDesc time_module = {
    .name = "time",
    .version = "1.0.0",
    .description = "Operações de tempo e data",
    .exports = time_exports,
    .export_count = sizeof(time_exports) / sizeof(time_exports[0]),
    .init = NULL,
    .cleanup = NULL
};

void time_module_register(void) {
    module_register_builtin(&time_module);
}

