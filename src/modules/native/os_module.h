/**
 * =============================================================================
 * ASTERON OS MODULE v1.0.0
 * =============================================================================
 * 
 * Módulo oficial para informações e operações do sistema operacional.
 * 
 * FUNÇÕES EXPORTADAS:
 * 
 * Informações do Sistema:
 *   os.platform()           -> string ("linux", "windows", "darwin")
 *   os.arch()               -> string ("x86_64", "arm64", etc)
 *   os.hostname()           -> string
 *   os.username()           -> string
 *   os.homedir()            -> string
 *   os.tmpdir()             -> string
 *   os.cpus()               -> number
 *   os.memory_total()       -> number (bytes)
 *   os.memory_free()        -> number (bytes)
 *   os.uptime()             -> number (seconds)
 *   os.loadavg()            -> array [1min, 5min, 15min]
 * 
 * Variáveis de Ambiente:
 *   os.env(name)            -> string | nil
 *   os.env_set(name, value) -> bool
 *   os.env_unset(name)      -> bool
 *   os.env_all()            -> map
 * 
 * Processos:
 *   os.pid()                -> number
 *   os.ppid()               -> number
 *   os.exec(cmd, args?)     -> { code, stdout, stderr }
 *   os.spawn(cmd, args?)    -> handle
 *   os.exit(code)           -> never
 *   os.kill(pid, signal?)   -> bool
 * 
 * Sinais:
 *   os.on_signal(sig, fn)   -> handle
 * 
 * CONSTANTES:
 *   os.EOL                  -> "\n" ou "\r\n"
 *   os.PATH_SEP             -> "/" ou "\\"
 * 
 * =============================================================================
 */

#ifndef ASTERON_OS_MODULE_H
#define ASTERON_OS_MODULE_H

#include "../module.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * API DO MÓDULO
 * ============================================================================= */

extern NativeModuleDesc os_module;
void os_module_register(void);

/* =============================================================================
 * FUNÇÕES NATIVAS
 * ============================================================================= */

/* Informações do Sistema */
AsteronValue os_platform(int argc, AsteronValue* args);
AsteronValue os_arch(int argc, AsteronValue* args);
AsteronValue os_hostname(int argc, AsteronValue* args);
AsteronValue os_username(int argc, AsteronValue* args);
AsteronValue os_homedir(int argc, AsteronValue* args);
AsteronValue os_tmpdir(int argc, AsteronValue* args);
AsteronValue os_cpus(int argc, AsteronValue* args);
AsteronValue os_memory_total(int argc, AsteronValue* args);
AsteronValue os_memory_free(int argc, AsteronValue* args);
AsteronValue os_uptime(int argc, AsteronValue* args);
AsteronValue os_loadavg(int argc, AsteronValue* args);

/* Variáveis de Ambiente */
AsteronValue os_env(int argc, AsteronValue* args);
AsteronValue os_env_set(int argc, AsteronValue* args);
AsteronValue os_env_unset(int argc, AsteronValue* args);
AsteronValue os_env_all(int argc, AsteronValue* args);

/* Processos */
AsteronValue os_pid(int argc, AsteronValue* args);
AsteronValue os_ppid(int argc, AsteronValue* args);
AsteronValue os_exec(int argc, AsteronValue* args);
AsteronValue os_spawn(int argc, AsteronValue* args);
AsteronValue os_exit_fn(int argc, AsteronValue* args);
AsteronValue os_kill(int argc, AsteronValue* args);

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_OS_MODULE_H */

