/**
 * =============================================================================
 * ASTERON GRAPH-BASED DEBUGGER
 * =============================================================================
 * 
 * Sistema de diagnóstico que usa os grafos gerados para:
 * 
 * 1. TRACE DE EXECUÇÃO
 *    - Mostra caminho exato pelo CFG
 *    - Identifica onde a execução falhou
 * 
 * 2. ANÁLISE DE DEPENDÊNCIAS
 *    - Identifica variáveis que influenciam valor com erro
 *    - Sugere pontos de inspeção
 * 
 * 3. DIAGNÓSTICO AUTOMÁTICO
 *    - Detecta padrões de erro comuns
 *    - Sugere correções baseadas no contexto
 * 
 * =============================================================================
 */

#ifndef ASTERON_GRAPH_DEBUG_H
#define ASTERON_GRAPH_DEBUG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * ESTRUTURAS DE DEBUG
 * ============================================================================= */

/* Estado de um nó durante debug */
typedef enum {
    NODE_NOT_VISITED,
    NODE_EXECUTING,
    NODE_SUCCESS,
    NODE_FAILED,
    NODE_SKIPPED
} NodeDebugState;

/* Checkpoint de debug */
typedef struct {
    char name[64];
    int line;
    NodeDebugState state;
    char value[256];        /* Valor da variável/expressão */
    char error_msg[256];    /* Mensagem de erro se houver */
} DebugCheckpoint;

/* Trace de execução */
typedef struct {
    DebugCheckpoint* checkpoints;
    int count;
    int capacity;
    int failed_at;          /* Índice do checkpoint que falhou (-1 se sucesso) */
} ExecutionTrace;

/* Análise de dependência para debug */
typedef struct {
    char var_name[64];
    char* dependencies[16];
    int dep_count;
    char* suggested_checks[8];
    int check_count;
} DependencyAnalysis;

/* Diagnóstico completo */
typedef struct {
    ExecutionTrace trace;
    DependencyAnalysis* analyses;
    int analysis_count;
    char diagnosis[1024];
    char suggested_fix[512];
} GraphDiagnosis;

/* =============================================================================
 * API DE DEBUG
 * ============================================================================= */

/* Inicia rastreamento de execução */
ExecutionTrace* trace_create(void);

/* Adiciona checkpoint */
void trace_checkpoint(ExecutionTrace* trace, const char* name, int line, 
                      const char* value, NodeDebugState state);

/* Marca falha */
void trace_mark_failure(ExecutionTrace* trace, const char* error_msg);

/* Imprime trace */
void trace_print(ExecutionTrace* trace);

/* Libera trace */
void trace_free(ExecutionTrace* trace);

/* Analisa dependências de uma variável */
DependencyAnalysis* analyze_dependencies(const char* var_name);

/* Gera diagnóstico completo baseado no trace e dependências */
GraphDiagnosis* diagnose_failure(ExecutionTrace* trace, const char* failed_var);

/* Imprime diagnóstico formatado */
void diagnosis_print(GraphDiagnosis* diag);

/* Libera diagnóstico */
void diagnosis_free(GraphDiagnosis* diag);

/* =============================================================================
 * MACROS DE DEBUG (para inserir no código)
 * ============================================================================= */

/* Debug habilitado/desabilitado */
extern int g_debug_enabled;

/* Trace global */
extern ExecutionTrace* g_trace;

#define DEBUG_INIT() do { \
    g_debug_enabled = 1; \
    g_trace = trace_create(); \
} while(0)

#define DEBUG_CHECKPOINT(name, value) do { \
    if (g_debug_enabled && g_trace) { \
        trace_checkpoint(g_trace, name, __LINE__, value, NODE_SUCCESS); \
    } \
} while(0)

#define DEBUG_FAIL(name, error) do { \
    if (g_debug_enabled && g_trace) { \
        trace_checkpoint(g_trace, name, __LINE__, "", NODE_FAILED); \
        trace_mark_failure(g_trace, error); \
    } \
} while(0)

#define DEBUG_PRINT_TRACE() do { \
    if (g_debug_enabled && g_trace) { \
        trace_print(g_trace); \
    } \
} while(0)

#define DEBUG_CLEANUP() do { \
    if (g_trace) { \
        trace_free(g_trace); \
        g_trace = NULL; \
    } \
    g_debug_enabled = 0; \
} while(0)

#endif /* ASTERON_GRAPH_DEBUG_H */

