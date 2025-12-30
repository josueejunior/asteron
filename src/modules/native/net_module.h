/**
 * =============================================================================
 * ASTERON NET MODULE v2.0
 * =============================================================================
 * 
 * Módulo de networking revolucionário para Asteron.
 * 
 * FUNÇÕES EXPORTADAS:
 * 
 * TCP:
 *   tcp_connect(host, port)              -> handle
 *   tcp_connect(host, port, timeout_ms)  -> handle
 *   tcp_listen(port)                     -> handle
 *   tcp_listen(port, backlog)            -> handle
 *   tcp_accept(listener)                 -> handle
 *   tcp_send(sock, data)                 -> bytes_sent
 *   tcp_recv(sock)                       -> string
 *   tcp_recv(sock, max_bytes)            -> string
 *   tcp_recv(sock, max, timeout_ms)      -> string
 *   tcp_recv_all(sock)                   -> string (tudo até fechar)
 *   tcp_recv_all(sock, timeout_ms)       -> string
 *   tcp_close(sock)                      -> bool
 * 
 * DNS:
 *   hostname()                           -> string
 *   resolve(domain)                      -> string (primeiro IP)
 *   resolve_all(domain)                  -> array de strings
 *   reverse_lookup(ip)                   -> string (hostname)
 * 
 * CONFIGURAÇÃO:
 *   set_timeout(sock, timeout_ms)        -> bool
 *   is_connected(sock)                   -> bool
 *   socket_stats(sock)                   -> string (JSON)
 * 
 * HTTP (utilitários):
 *   http_get(url)                        -> string
 *   http_post(url, body)                 -> string (TODO)
 * 
 * NOTA: Handles são retornados como números (índices na tabela global).
 *       Handle 0 ou -1 indica erro.
 * 
 * =============================================================================
 */

#ifndef ASTERON_NET_MODULE_H
#define ASTERON_NET_MODULE_H

#include "../module.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * API DO MÓDULO
 * ============================================================================= */

extern NativeModuleDesc net_module;
void net_module_register(void);

/* =============================================================================
 * FUNÇÕES TCP
 * ============================================================================= */

AsteronValue net_tcp_connect(int argc, AsteronValue* args);
AsteronValue net_tcp_listen(int argc, AsteronValue* args);
AsteronValue net_tcp_accept(int argc, AsteronValue* args);
AsteronValue net_tcp_send(int argc, AsteronValue* args);
AsteronValue net_tcp_recv(int argc, AsteronValue* args);
AsteronValue net_tcp_recv_all(int argc, AsteronValue* args);
AsteronValue net_tcp_close(int argc, AsteronValue* args);

/* =============================================================================
 * FUNÇÕES DNS
 * ============================================================================= */

AsteronValue net_hostname(int argc, AsteronValue* args);
AsteronValue net_resolve(int argc, AsteronValue* args);
AsteronValue net_resolve_all(int argc, AsteronValue* args);
AsteronValue net_reverse_lookup(int argc, AsteronValue* args);

/* =============================================================================
 * CONFIGURAÇÃO E STATUS
 * ============================================================================= */

AsteronValue net_set_timeout(int argc, AsteronValue* args);
AsteronValue net_is_connected(int argc, AsteronValue* args);
AsteronValue net_socket_stats(int argc, AsteronValue* args);

/* =============================================================================
 * HTTP UTILITÁRIOS
 * ============================================================================= */

AsteronValue net_http_get(int argc, AsteronValue* args);
AsteronValue net_http_post(int argc, AsteronValue* args);

/* =============================================================================
 * UDP (Futuro)
 * ============================================================================= */

AsteronValue net_udp_bind(int argc, AsteronValue* args);
AsteronValue net_udp_send(int argc, AsteronValue* args);
AsteronValue net_udp_recv(int argc, AsteronValue* args);

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_NET_MODULE_H */

