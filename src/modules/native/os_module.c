/**
 * =============================================================================
 * ASTERON OS MODULE - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "os_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
    #include <windows.h>
    #include <lmcons.h>
    #define PATH_SEP "\\"
    #define EOL "\r\n"
#else
    #include <unistd.h>
    #include <sys/utsname.h>
    #include <sys/sysinfo.h>
    #include <pwd.h>
    #include <signal.h>
    #include <sys/wait.h>
    #define PATH_SEP "/"
    #define EOL "\n"
#endif

/* =============================================================================
 * HELPERS
 * ============================================================================= */

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

static const char* get_string_arg(AsteronValue* args, int index) {
    // Verifica nil primeiro (segurança crítica)
    if (ASTERON_IS_NIL(args[index])) return NULL;
    if (!ASTERON_IS_STRING(args[index])) return NULL;
    AsteronString* str = ASTERON_AS_STRING(args[index]);
    return str ? str->chars : NULL;
}

/* =============================================================================
 * os.platform() -> string
 * ============================================================================= */

AsteronValue os_platform(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #if defined(_WIN32)
    return make_string("windows");
    #elif defined(__APPLE__)
    return make_string("darwin");
    #elif defined(__linux__)
    return make_string("linux");
    #elif defined(__FreeBSD__)
    return make_string("freebsd");
    #else
    return make_string("unknown");
    #endif
}

/* =============================================================================
 * os.arch() -> string
 * ============================================================================= */

AsteronValue os_arch(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #if defined(__x86_64__) || defined(_M_X64)
    return make_string("x86_64");
    #elif defined(__aarch64__) || defined(_M_ARM64)
    return make_string("arm64");
    #elif defined(__i386__) || defined(_M_IX86)
    return make_string("x86");
    #elif defined(__arm__) || defined(_M_ARM)
    return make_string("arm");
    #else
    return make_string("unknown");
    #endif
}

/* =============================================================================
 * os.hostname() -> string
 * ============================================================================= */

AsteronValue os_hostname(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    char hostname[256];
    
    #ifdef _WIN32
    DWORD size = sizeof(hostname);
    GetComputerNameA(hostname, &size);
    #else
    gethostname(hostname, sizeof(hostname));
    #endif
    
    return make_string(hostname);
}

/* =============================================================================
 * os.username() -> string
 * ============================================================================= */

AsteronValue os_username(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #ifdef _WIN32
    char username[UNLEN + 1];
    DWORD size = sizeof(username);
    GetUserNameA(username, &size);
    return make_string(username);
    #else
    struct passwd* pw = getpwuid(getuid());
    if (pw) {
        return make_string(pw->pw_name);
    }
    char* user = getenv("USER");
    return user ? make_string(user) : ASTERON_NIL();
    #endif
}

/* =============================================================================
 * os.homedir() -> string
 * ============================================================================= */

AsteronValue os_homedir(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #ifdef _WIN32
    char* home = getenv("USERPROFILE");
    if (!home) {
        char* drive = getenv("HOMEDRIVE");
        char* path = getenv("HOMEPATH");
        if (drive && path) {
            static char buf[512];
            snprintf(buf, sizeof(buf), "%s%s", drive, path);
            return make_string(buf);
        }
    }
    return home ? make_string(home) : ASTERON_NIL();
    #else
    char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    return home ? make_string(home) : ASTERON_NIL();
    #endif
}

/* =============================================================================
 * os.tmpdir() -> string
 * ============================================================================= */

AsteronValue os_tmpdir(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #ifdef _WIN32
    char tmp[MAX_PATH];
    GetTempPathA(sizeof(tmp), tmp);
    return make_string(tmp);
    #else
    char* tmp = getenv("TMPDIR");
    if (!tmp) tmp = getenv("TMP");
    if (!tmp) tmp = getenv("TEMP");
    if (!tmp) tmp = "/tmp";
    return make_string(tmp);
    #endif
}

/* =============================================================================
 * os.cpus() -> number
 * ============================================================================= */

AsteronValue os_cpus(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return ASTERON_NUMBER((double)si.dwNumberOfProcessors);
    #else
    return ASTERON_NUMBER((double)sysconf(_SC_NPROCESSORS_ONLN));
    #endif
}

/* =============================================================================
 * os.memory_total() -> number (bytes)
 * ============================================================================= */

AsteronValue os_memory_total(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #ifdef _WIN32
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    return ASTERON_NUMBER((double)mem.ullTotalPhys);
    #elif defined(__linux__)
    struct sysinfo si;
    sysinfo(&si);
    return ASTERON_NUMBER((double)si.totalram * si.mem_unit);
    #else
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return ASTERON_NUMBER((double)(pages * page_size));
    #endif
}

/* =============================================================================
 * os.memory_free() -> number (bytes)
 * ============================================================================= */

AsteronValue os_memory_free(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #ifdef _WIN32
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    return ASTERON_NUMBER((double)mem.ullAvailPhys);
    #elif defined(__linux__)
    struct sysinfo si;
    sysinfo(&si);
    return ASTERON_NUMBER((double)si.freeram * si.mem_unit);
    #else
    long pages = sysconf(_SC_AVPHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return ASTERON_NUMBER((double)(pages * page_size));
    #endif
}

/* =============================================================================
 * os.uptime() -> number (seconds)
 * ============================================================================= */

AsteronValue os_uptime(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #ifdef _WIN32
    return ASTERON_NUMBER((double)GetTickCount64() / 1000.0);
    #elif defined(__linux__)
    struct sysinfo si;
    sysinfo(&si);
    return ASTERON_NUMBER((double)si.uptime);
    #else
    /* Fallback */
    return ASTERON_NUMBER(0);
    #endif
}

/* =============================================================================
 * os.loadavg() -> array [1min, 5min, 15min]
 * ============================================================================= */

AsteronValue os_loadavg(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #if defined(__linux__) || defined(__APPLE__)
    double load[3];
    getloadavg(load, 3);
    
    AsteronArray* arr = (AsteronArray*)malloc(sizeof(AsteronArray));
    arr->header.obj_type = ASTERON_OBJ_ARRAY;
    arr->header.flags = 0;
    arr->header.ref_count = 1;
    arr->header.next = NULL;
    arr->capacity = 3;
    arr->length = 3;
    arr->items = (AsteronValue*)malloc(3 * sizeof(AsteronValue));
    arr->items[0] = ASTERON_NUMBER(load[0]);
    arr->items[1] = ASTERON_NUMBER(load[1]);
    arr->items[2] = ASTERON_NUMBER(load[2]);
    
    return ASTERON_PTR(arr, ASTERON_VAL_ARRAY);
    #else
    return ASTERON_NIL();
    #endif
}

/* =============================================================================
 * os.env(name) -> string | nil
 * ============================================================================= */

AsteronValue os_env(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_NIL();
    
    const char* name = get_string_arg(args, 0);
    if (!name) return ASTERON_NIL();
    
    char* value = getenv(name);
    return value ? make_string(value) : ASTERON_NIL();
}

/* =============================================================================
 * os.env_set(name, value) -> bool
 * ============================================================================= */

AsteronValue os_env_set(int argc, AsteronValue* args) {
    if (argc < 2) return ASTERON_BOOL(false);
    
    const char* name = get_string_arg(args, 0);
    const char* value = get_string_arg(args, 1);
    
    if (!name || !value) return ASTERON_BOOL(false);
    
    #ifdef _WIN32
    return ASTERON_BOOL(SetEnvironmentVariableA(name, value) != 0);
    #else
    return ASTERON_BOOL(setenv(name, value, 1) == 0);
    #endif
}

/* =============================================================================
 * os.env_unset(name) -> bool
 * ============================================================================= */

AsteronValue os_env_unset(int argc, AsteronValue* args) {
    if (argc < 1) return ASTERON_BOOL(false);
    
    const char* name = get_string_arg(args, 0);
    if (!name) return ASTERON_BOOL(false);
    
    #ifdef _WIN32
    return ASTERON_BOOL(SetEnvironmentVariableA(name, NULL) != 0);
    #else
    return ASTERON_BOOL(unsetenv(name) == 0);
    #endif
}

/* =============================================================================
 * os.pid() -> number
 * ============================================================================= */

AsteronValue os_pid(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #ifdef _WIN32
    return ASTERON_NUMBER((double)GetCurrentProcessId());
    #else
    return ASTERON_NUMBER((double)getpid());
    #endif
}

/* =============================================================================
 * os.ppid() -> number
 * ============================================================================= */

AsteronValue os_ppid(int argc, AsteronValue* args) {
    (void)argc; (void)args;
    
    #ifdef _WIN32
    return ASTERON_NUMBER(0);  /* Não trivial no Windows */
    #else
    return ASTERON_NUMBER((double)getppid());
    #endif
}

/* =============================================================================
 * os.exit(code) -> never
 * ============================================================================= */

AsteronValue os_exit_fn(int argc, AsteronValue* args) {
    int code = 0;
    if (argc >= 1 && ASTERON_IS_NUMBER(args[0])) {
        code = (int)ASTERON_AS_NUMBER(args[0]);
    }
    
    exit(code);
    
    /* Nunca alcançado */
    return ASTERON_NIL();
}

/* =============================================================================
 * os.kill(pid, signal?) -> bool
 * ============================================================================= */

AsteronValue os_kill(int argc, AsteronValue* args) {
    if (argc < 1 || !ASTERON_IS_NUMBER(args[0])) {
        return ASTERON_BOOL(false);
    }
    
    int pid = (int)ASTERON_AS_NUMBER(args[0]);
    int sig = 15;  /* SIGTERM */
    
    if (argc >= 2 && ASTERON_IS_NUMBER(args[1])) {
        sig = (int)ASTERON_AS_NUMBER(args[1]);
    }
    
    #ifdef _WIN32
    HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (proc) {
        BOOL result = TerminateProcess(proc, 1);
        CloseHandle(proc);
        return ASTERON_BOOL(result != 0);
    }
    return ASTERON_BOOL(false);
    #else
    return ASTERON_BOOL(kill(pid, sig) == 0);
    #endif
}

/* Funções não implementadas completamente */
AsteronValue os_env_all(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }
AsteronValue os_exec(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }
AsteronValue os_spawn(int argc, AsteronValue* args) { (void)argc; (void)args; return ASTERON_NIL(); }

/* =============================================================================
 * DESCRITOR DO MÓDULO
 * ============================================================================= */

static ModuleExport os_exports[] = {
    /* Informações do Sistema */
    { "platform",     os_platform,     ASTERON_NIL(), 0, 0, "() -> string" },
    { "arch",         os_arch,         ASTERON_NIL(), 0, 0, "() -> string" },
    { "hostname",     os_hostname,     ASTERON_NIL(), 0, 0, "() -> string" },
    { "username",     os_username,     ASTERON_NIL(), 0, 0, "() -> string" },
    { "homedir",      os_homedir,      ASTERON_NIL(), 0, 0, "() -> string" },
    { "tmpdir",       os_tmpdir,       ASTERON_NIL(), 0, 0, "() -> string" },
    { "cpus",         os_cpus,         ASTERON_NIL(), 0, 0, "() -> number" },
    { "memory_total", os_memory_total, ASTERON_NIL(), 0, 0, "() -> number" },
    { "memory_free",  os_memory_free,  ASTERON_NIL(), 0, 0, "() -> number" },
    { "uptime",       os_uptime,       ASTERON_NIL(), 0, 0, "() -> number" },
    { "loadavg",      os_loadavg,      ASTERON_NIL(), 0, 0, "() -> array" },
    
    /* Variáveis de Ambiente */
    { "env",          os_env,          ASTERON_NIL(), 1, 1, "(name: string) -> string | nil" },
    { "env_set",      os_env_set,      ASTERON_NIL(), 2, 2, "(name: string, value: string) -> bool" },
    { "env_unset",    os_env_unset,    ASTERON_NIL(), 1, 1, "(name: string) -> bool" },
    
    /* Processos */
    { "pid",          os_pid,          ASTERON_NIL(), 0, 0, "() -> number" },
    { "ppid",         os_ppid,         ASTERON_NIL(), 0, 0, "() -> number" },
    { "exit",         os_exit_fn,      ASTERON_NIL(), 0, 1, "(code?: number) -> never" },
    { "kill",         os_kill,         ASTERON_NIL(), 1, 2, "(pid: number, signal?: number) -> bool" },
    
    /* Constantes */
    { "EOL",          NULL, { .type = ASTERON_VAL_STRING }, 0, 0, NULL },
    { "PATH_SEP",     NULL, { .type = ASTERON_VAL_STRING }, 0, 0, NULL },
};

NativeModuleDesc os_module = {
    .name = "os",
    .version = "1.0.0",
    .description = "Sistema operacional e ambiente",
    .exports = os_exports,
    .export_count = sizeof(os_exports) / sizeof(os_exports[0]),
    .init = NULL,
    .cleanup = NULL
};

void os_module_register(void) {
    module_register_builtin(&os_module);
}

