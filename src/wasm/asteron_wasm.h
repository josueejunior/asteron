/**
 * =============================================================================
 * ASTERON WEBASSEMBLY EXPORT v1.0
 * =============================================================================
 * 
 * Interface para exportar funcionalidades do Asteron para WebAssembly.
 * Permite que o compilador rode no navegador e gere grafos em tempo real.
 * 
 * USO COM EMSCRIPTEN:
 * 
 *   emcc src/wasm/asteron_wasm.c \
 *        src/core/lexer/lexer.c \
 *        src/core/parser/parser.c \
 *        src/core/ast/ast.c \
 *        src/graph/unified_graph.c \
 *        -o asteron.js \
 *        -s EXPORTED_FUNCTIONS='["_compile_to_ast","_get_graph_json","_get_hot_paths"]' \
 *        -s WASM=1 \
 *        -s ALLOW_MEMORY_GROWTH=1 \
 *        -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString"]'
 * 
 * =============================================================================
 */

#ifndef ASTERON_WASM_H
#define ASTERON_WASM_H

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// COMPILAÇÃO
// =============================================================================

/**
 * Compila código Asteron e retorna AST como JSON
 * 
 * @param code Código fonte (string C)
 * @return Ponteiro para string JSON (deve ser liberado com free)
 */
EMSCRIPTEN_KEEPALIVE
char* compile_to_ast(const char* code);

/**
 * Compila código e retorna grafo unificado como JSON
 * 
 * @param code Código fonte
 * @return Ponteiro para string JSON (deve ser liberado com free)
 */
EMSCRIPTEN_KEEPALIVE
char* compile_to_graph(const char* code);

// =============================================================================
// GRAFO UNIFICADO
// =============================================================================

/**
 * Obtém grafo unificado como JSON (com métricas)
 * 
 * @return Ponteiro para string JSON (deve ser liberado com free)
 */
EMSCRIPTEN_KEEPALIVE
char* get_graph_json(void);

/**
 * Obtém nós "hot" (frequentemente executados) como JSON
 * 
 * @return Ponteiro para string JSON (deve ser liberado com free)
 */
EMSCRIPTEN_KEEPALIVE
char* get_hot_paths(void);

/**
 * Obtém métricas de um nó específico
 * 
 * @param node_name Nome do nó
 * @return Ponteiro para string JSON (deve ser liberado com free)
 */
EMSCRIPTEN_KEEPALIVE
char* get_node_metrics(const char* node_name);

// =============================================================================
// EXECUÇÃO E PROFILING
// =============================================================================

/**
 * Executa código e coleta métricas
 * 
 * @param code Código fonte
 * @param collect_metrics true para coletar métricas
 * @return Ponteiro para string JSON com resultados (deve ser liberado com free)
 */
EMSCRIPTEN_KEEPALIVE
char* execute_with_metrics(const char* code, bool collect_metrics);

/**
 * Obtém estatísticas de execução
 * 
 * @return Ponteiro para string JSON (deve ser liberado com free)
 */
EMSCRIPTEN_KEEPALIVE
char* get_execution_stats(void);

// =============================================================================
// UTILITÁRIOS
// =============================================================================

/**
 * Libera memória alocada pelo Wasm
 * 
 * @param ptr Ponteiro retornado por outras funções
 */
EMSCRIPTEN_KEEPALIVE
void wasm_free(void* ptr);

/**
 * Obtém versão do compilador
 * 
 * @return String de versão (não precisa liberar)
 */
EMSCRIPTEN_KEEPALIVE
const char* get_version(void);

#ifdef __cplusplus
}
#endif

#endif // ASTERON_WASM_H


