/**
 * =============================================================================
 * ASTERON TLS/HTTPS MODULE
 * =============================================================================
 * 
 * Suporte a conexões seguras via TLS/SSL.
 * 
 * FUNÇÕES:
 *   tls_connect(host, port)           -> handle seguro
 *   tls_send(handle, data)            -> bytes enviados
 *   tls_recv(handle, max_bytes)       -> string
 *   tls_close(handle)                 -> bool
 *   https_get(url)                    -> resposta HTTP
 *   https_post(url, body)             -> resposta HTTP
 * 
 * NOTA: Requer OpenSSL ou mbedTLS no sistema.
 *       Esta é uma implementação simplificada.
 * 
 * =============================================================================
 */

#ifndef ASTERON_TLS_MODULE_H
#define ASTERON_TLS_MODULE_H

#include "../module.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Inicialização
 * ============================================================================= */

/**
 * Inicializa o subsistema TLS.
 * Deve ser chamado antes de qualquer operação TLS.
 */
int tls_init(void);

/**
 * Finaliza o subsistema TLS.
 */
void tls_cleanup(void);

/* =============================================================================
 * API do Módulo
 * ============================================================================= */

AsteronValue tls_connect(int argc, AsteronValue* args);
AsteronValue tls_send(int argc, AsteronValue* args);
AsteronValue tls_recv(int argc, AsteronValue* args);
AsteronValue tls_close(int argc, AsteronValue* args);
AsteronValue https_get(int argc, AsteronValue* args);
AsteronValue https_post(int argc, AsteronValue* args);

/* =============================================================================
 * Status TLS
 * ============================================================================= */

/**
 * Verifica se TLS está disponível no sistema.
 */
AsteronValue tls_available(int argc, AsteronValue* args);

/**
 * Retorna versão do TLS.
 */
AsteronValue tls_version(int argc, AsteronValue* args);

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_TLS_MODULE_H */

