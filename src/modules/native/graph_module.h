/**
 * =============================================================================
 * ASTERON GRAPH MODULE
 * =============================================================================
 * 
 * Módulo nativo para operações com grafos.
 * Fornece funções para criar, manipular e analisar grafos.
 * 
 * =============================================================================
 */

#ifndef ASTERON_GRAPH_MODULE_H
#define ASTERON_GRAPH_MODULE_H

#include "../../core/abi.h"
#include "../module.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Declaração do módulo */
ASTERON_MODULE_DECLARE(graph);

/* Função de registro */
void graph_module_register(void);

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_GRAPH_MODULE_H */

