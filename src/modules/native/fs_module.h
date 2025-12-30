/**
 * =============================================================================
 * ASTERON FS MODULE v1.0.0
 * =============================================================================
 * 
 * Módulo oficial para operações de sistema de arquivos.
 * 
 * FUNÇÕES EXPORTADAS:
 * 
 *   fs.read(path)           -> string | error
 *   fs.write(path, data)    -> bool | error
 *   fs.append(path, data)   -> bool | error
 *   fs.exists(path)         -> bool
 *   fs.remove(path)         -> bool | error
 *   fs.mkdir(path)          -> bool | error
 *   fs.rmdir(path)          -> bool | error
 *   fs.list(path)           -> array | error
 *   fs.stat(path)           -> object | error
 *   fs.copy(src, dst)       -> bool | error
 *   fs.move(src, dst)       -> bool | error
 *   fs.open(path, mode)     -> handle | error
 *   fs.close(handle)        -> bool
 *   fs.read_bytes(path)     -> bytes | error
 *   fs.write_bytes(path, b) -> bool | error
 * 
 * CONSTANTES:
 *   fs.SEPARATOR            -> "/" ou "\\"
 *   fs.PATH_MAX             -> 4096
 * 
 * =============================================================================
 */

#ifndef ASTERON_FS_MODULE_H
#define ASTERON_FS_MODULE_H

#include "../module.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * API DO MÓDULO
 * ============================================================================= */

/**
 * Descritor do módulo fs
 */
extern NativeModuleDesc fs_module;

/**
 * Registra módulo fs no sistema
 */
void fs_module_register(void);

/* =============================================================================
 * FUNÇÕES NATIVAS (Implementação em fs_module.c)
 * ============================================================================= */

AsteronValue fs_read(int argc, AsteronValue* args);
AsteronValue fs_write(int argc, AsteronValue* args);
AsteronValue fs_append(int argc, AsteronValue* args);
AsteronValue fs_exists(int argc, AsteronValue* args);
AsteronValue fs_remove(int argc, AsteronValue* args);
AsteronValue fs_mkdir(int argc, AsteronValue* args);
AsteronValue fs_rmdir(int argc, AsteronValue* args);
AsteronValue fs_list(int argc, AsteronValue* args);
AsteronValue fs_stat(int argc, AsteronValue* args);
AsteronValue fs_copy(int argc, AsteronValue* args);
AsteronValue fs_move(int argc, AsteronValue* args);
AsteronValue fs_open(int argc, AsteronValue* args);
AsteronValue fs_close(int argc, AsteronValue* args);
AsteronValue fs_read_bytes(int argc, AsteronValue* args);
AsteronValue fs_write_bytes(int argc, AsteronValue* args);
AsteronValue fs_read_line(int argc, AsteronValue* args);
AsteronValue fs_cwd(int argc, AsteronValue* args);
AsteronValue fs_chdir(int argc, AsteronValue* args);
AsteronValue fs_is_file(int argc, AsteronValue* args);
AsteronValue fs_is_dir(int argc, AsteronValue* args);
AsteronValue fs_size(int argc, AsteronValue* args);

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_FS_MODULE_H */

