#ifndef FAILURE_ANALYTICS_H
#define FAILURE_ANALYTICS_H

#include "../vm/vm.h"
#include "../../graph/unified_graph.h"
#include "../ast/ast.h"
#include <stddef.h>

/* =============================================================================
 * ANALÍTICA DE FALHAS
 * =============================================================================
 * Detecta onde e porque algo pode travar ou gerar exceção antes de rodar,
 * através de análise estática e dinâmica.
 * ============================================================================= */

// Tipo de problema detectado
typedef enum {
    FAILURE_NULL_POINTER,          // Possível null pointer dereference
    FAILURE_DIVISION_BY_ZERO,      // Divisão por zero
    FAILURE_ARRAY_BOUNDS,          // Array out of bounds
    FAILURE_TYPE_MISMATCH,         // Type mismatch
    FAILURE_MEMORY_LEAK,           // Memory leak
    FAILURE_DEADLOCK,              // Possível deadlock
    FAILURE_INFINITE_LOOP,         // Possível loop infinito
    FAILURE_UNINITIALIZED,         // Variável não inicializada
    FAILURE_RESOURCE_LEAK,         // Resource leak (file, socket, etc)
    FAILURE_RACE_CONDITION         // Race condition
} FailureType;

// Severidade do problema
typedef enum {
    SEVERITY_LOW,                  // Baixa - pode não causar problema
    SEVERITY_MEDIUM,               // Média - pode causar problema
    SEVERITY_HIGH,                 // Alta - provavelmente causará problema
    SEVERITY_CRITICAL              // Crítica - certamente causará problema
} FailureSeverity;

// Problema detectado
typedef struct FailureIssue {
    FailureType type;              // Tipo do problema
    FailureSeverity severity;      // Severidade
    const char* location;          // Localização (arquivo:linha)
    const char* description;       // Descrição do problema
    const char* suggestion;        // Sugestão de correção
    
    // Contexto
    UnifiedNode* node;              // Nó do grafo relacionado (se houver)
    ASTNode* ast_node;             // Nó AST relacionado (se houver)
    size_t pc;                     // Program counter (se durante execução)
    
    // Metadados
    int is_confirmed;              // Foi confirmado durante execução?
    int occurrence_count;          // Quantas vezes foi detectado
    time_t first_seen;             // Quando foi detectado pela primeira vez
    time_t last_seen;              // Quando foi detectado pela última vez
} FailureIssue;

// Analisador de falhas
typedef struct FailureAnalyzer {
    VM* vm;                        // VM sendo analisada
    UnifiedGraph* graph;           // Grafo sendo analisado
    ASTNode* ast;                  // AST sendo analisada
    
    // Problemas detectados
    FailureIssue* issues;          // Lista de problemas
    size_t issue_count;
    size_t issue_capacity;
    
    // Configuração
    int static_analysis;           // Realiza análise estática?
    int dynamic_analysis;          // Realiza análise dinâmica?
    int track_execution;           // Rastreia execução?
    
    // Estatísticas
    struct {
        size_t total_issues;
        size_t critical_issues;
        size_t confirmed_issues;
        size_t false_positives;
    } stats;
    
    // Callbacks
    void (*on_issue_detected)(FailureIssue* issue);
    void (*on_critical_issue)(FailureIssue* issue);
} FailureAnalyzer;

/* =============================================================================
 * API - CRIAÇÃO E DESTRUIÇÃO
 * ============================================================================= */

/**
 * Cria analisador de falhas
 */
FailureAnalyzer* failure_analyzer_create(VM* vm, UnifiedGraph* graph, ASTNode* ast);

/**
 * Destrói analisador de falhas
 */
void failure_analyzer_destroy(FailureAnalyzer* analyzer);

/* =============================================================================
 * API - ANÁLISE
 * ============================================================================= */

/**
 * Executa análise estática completa
 */
void failure_analyze_static(FailureAnalyzer* analyzer);

/**
 * Executa análise dinâmica (durante execução)
 */
void failure_analyze_dynamic(FailureAnalyzer* analyzer);

/**
 * Analisa nó específico do grafo
 */
void failure_analyze_node(FailureAnalyzer* analyzer, UnifiedNode* node);

/**
 * Analisa instrução específica
 */
void failure_analyze_instruction(FailureAnalyzer* analyzer, size_t pc);

/* =============================================================================
 * API - DETECÇÃO ESPECÍFICA
 * ============================================================================= */

/**
 * Detecta possíveis null pointer dereferences
 */
void failure_detect_null_pointers(FailureAnalyzer* analyzer);

/**
 * Detecta possíveis divisões por zero
 */
void failure_detect_division_by_zero(FailureAnalyzer* analyzer);

/**
 * Detecta possíveis array out of bounds
 */
void failure_detect_array_bounds(FailureAnalyzer* analyzer);

/**
 * Detecta possíveis memory leaks
 */
void failure_detect_memory_leaks(FailureAnalyzer* analyzer);

/**
 * Detecta possíveis deadlocks
 */
void failure_detect_deadlocks(FailureAnalyzer* analyzer);

/**
 * Detecta possíveis loops infinitos
 */
void failure_detect_infinite_loops(FailureAnalyzer* analyzer);

/**
 * Detecta variáveis não inicializadas
 */
void failure_detect_uninitialized(FailureAnalyzer* analyzer);

/**
 * Detecta possíveis race conditions
 */
void failure_detect_race_conditions(FailureAnalyzer* analyzer);

/* =============================================================================
 * API - RELATÓRIOS
 * ============================================================================= */

/**
 * Gera relatório de problemas
 */
void failure_generate_report(FailureAnalyzer* analyzer, const char* filename);

/**
 * Lista todos os problemas detectados
 */
void failure_list_issues(FailureAnalyzer* analyzer);

/**
 * Lista problemas críticos
 */
void failure_list_critical(FailureAnalyzer* analyzer);

/**
 * Obtém estatísticas
 */
void failure_get_stats(FailureAnalyzer* analyzer, size_t* total, size_t* critical,
                       size_t* confirmed);

#endif // FAILURE_ANALYTICS_H

