#define _POSIX_C_SOURCE 200809L
#include "failure_analytics.h"
#include "../../utils/utils.h"
#include "../vm/vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Implementação alternativa de strdup se não disponível
static char* safe_strdup(const char* s) {
    if (s == NULL) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = (char*)malloc(len);
    if (copy != NULL) {
        memcpy(copy, s, len);
    }
    return copy;
}

/* =============================================================================
 * CRIAÇÃO E DESTRUIÇÃO
 * ============================================================================= */

FailureAnalyzer* failure_analyzer_create(VM* vm, UnifiedGraph* graph, ASTNode* ast) {
    // Verificações defensivas antes de criar
    if (vm == NULL) {
        // VM pode ser NULL, mas vamos continuar
    } else {
        // Verifica se a VM tem estrutura válida
        if (vm->program != NULL) {
            // Se program existe mas instructions é NULL e count > 0, é suspeito
            if (vm->program->instructions == NULL && vm->program->instruction_count > 0) {
                // Instruções NULL mas count > 0 é suspeito - pode causar crash
                return NULL;
            }
            // Proteção contra instruction_count muito grande (possível corrupção)
            if (vm->program->instruction_count > 100000) {
                return NULL;
            }
        }
    }
    
    // Verifica se o grafo tem estrutura válida
    if (graph != NULL) {
        if (graph->nodes == NULL && graph->node_count > 0) {
            // Nodes NULL mas count > 0 é suspeito
            return NULL;
        }
        // Proteção contra node_count muito grande
        if (graph->node_count > 100000) {
            return NULL;
        }
    }
    
    FailureAnalyzer* analyzer = (FailureAnalyzer*)calloc(1, sizeof(FailureAnalyzer));
    if (analyzer == NULL) return NULL;
    
    analyzer->vm = vm;
    analyzer->graph = graph;
    analyzer->ast = ast;
    
    analyzer->issue_capacity = 64;
    analyzer->issue_count = 0;  // Inicializa explicitamente
    analyzer->issues = (FailureIssue*)calloc(
        analyzer->issue_capacity, sizeof(FailureIssue));
    if (analyzer->issues == NULL) {
        free(analyzer);
        return NULL;
    }
    
    // Inicializa todas as issues com zeros (calloc já faz isso, mas garantimos)
    memset(analyzer->issues, 0, sizeof(FailureIssue) * analyzer->issue_capacity);
    
    analyzer->static_analysis = 1;
    analyzer->dynamic_analysis = 1;
    analyzer->track_execution = 1;
    
    // Inicializa callbacks como NULL (segurança crítica)
    analyzer->on_issue_detected = NULL;
    analyzer->on_critical_issue = NULL;
    
    // Inicializa estatísticas
    memset(&analyzer->stats, 0, sizeof(analyzer->stats));
    
    return analyzer;
}

void failure_analyzer_destroy(FailureAnalyzer* analyzer) {
    if (analyzer == NULL) return;
    
    // Proteção: verifica se issues é válido antes de iterar
    if (analyzer->issues != NULL) {
        // Proteção: verifica se issue_count é válido
        size_t max_count = analyzer->issue_count;
        if (max_count > analyzer->issue_capacity) {
            max_count = analyzer->issue_capacity; // Limita ao máximo seguro
        }
        if (max_count > 10000) {
            max_count = 10000; // Limita adicional para segurança
        }
        
        for (size_t i = 0; i < max_count; i++) {
            // Verifica se o índice está dentro dos bounds
            if (i >= analyzer->issue_capacity) break;
            
            if (analyzer->issues[i].location) {
                free((void*)analyzer->issues[i].location);
            }
            if (analyzer->issues[i].description) {
                free((void*)analyzer->issues[i].description);
            }
            if (analyzer->issues[i].suggestion) {
                free((void*)analyzer->issues[i].suggestion);
            }
        }
        free(analyzer->issues);
    }
    free(analyzer);
}

/* =============================================================================
 * ANÁLISE
 * ============================================================================= */

void failure_analyze_static(FailureAnalyzer* analyzer) {
    if (analyzer == NULL || !analyzer->static_analysis) return;
    
    // Verifica se VM e programa são válidos antes de qualquer análise
    if (analyzer->vm == NULL || analyzer->vm->program == NULL) {
        printf("  [Info] Análise estática pulada - VM ou programa não disponível\n");
        return;
    }
    
    // Proteção adicional: verifica se instructions é válido
    if (analyzer->vm->program->instructions == NULL) {
        printf("  [Info] Análise estática pulada - Instruções não disponíveis\n");
        return;
    }
    
    // Executa todas as análises estáticas com tratamento de erros
    // Usa try-catch simulado com verificações defensivas
    failure_detect_null_pointers(analyzer);
    failure_detect_division_by_zero(analyzer);
    failure_detect_array_bounds(analyzer);
    failure_detect_memory_leaks(analyzer);
    failure_detect_deadlocks(analyzer);
    
    // Verifica se graph é válido antes de usar
    if (analyzer->graph != NULL) {
        failure_detect_infinite_loops(analyzer);
    }
    
    failure_detect_uninitialized(analyzer);
    failure_detect_race_conditions(analyzer);
}

void failure_analyze_dynamic(FailureAnalyzer* analyzer) {
    if (analyzer == NULL || !analyzer->dynamic_analysis) return;
    
    // Análise dinâmica durante execução
    // (Simplificado - em produção, instrumentaria a execução)
}

void failure_analyze_node(FailureAnalyzer* analyzer, UnifiedNode* node) {
    if (analyzer == NULL || node == NULL) return;
    
    // Analisa nó específico
    // (Simplificado - em produção, analisaria o nó em detalhes)
}

void failure_analyze_instruction(FailureAnalyzer* analyzer, size_t pc) {
    if (analyzer == NULL || analyzer->vm == NULL) return;
    if (analyzer->vm->program == NULL) return;
    if (analyzer->vm->program->instructions == NULL) return;
    
    if (pc >= analyzer->vm->program->instruction_count) {
        return;
    }
    
    // Analisa instrução específica
    // (Simplificado - em produção, analisaria a instrução)
}

/* =============================================================================
 * DETECÇÃO ESPECÍFICA
 * ============================================================================= */

static void add_issue(FailureAnalyzer* analyzer, FailureType type, 
                      FailureSeverity severity, const char* location,
                      const char* description, const char* suggestion) {
    if (analyzer == NULL) return;
    if (analyzer->issues == NULL) {
        // Tenta alocar se não foi alocado
        analyzer->issue_capacity = 64;
        analyzer->issues = (FailureIssue*)calloc(
            analyzer->issue_capacity, sizeof(FailureIssue));
        if (analyzer->issues == NULL) return;
    }
    
    if (analyzer->issue_count >= analyzer->issue_capacity) {
        size_t new_capacity = analyzer->issue_capacity * 2;
        if (new_capacity == 0) new_capacity = 8; // Evita multiplicação por zero
        FailureIssue* new_issues = (FailureIssue*)realloc(
            analyzer->issues, sizeof(FailureIssue) * new_capacity);
        if (new_issues == NULL) return;
        // Inicializa novas posições com zeros
        memset(&new_issues[analyzer->issue_count], 0, 
               sizeof(FailureIssue) * (new_capacity - analyzer->issue_count));
        analyzer->issues = new_issues;
        analyzer->issue_capacity = new_capacity;
    }
    
    FailureIssue* issue = &analyzer->issues[analyzer->issue_count++];
    memset(issue, 0, sizeof(FailureIssue)); // Inicializa com zeros
    
    issue->type = type;
    issue->severity = severity;
    
    // Usa safe_strdup que funciona mesmo sem _POSIX_C_SOURCE
    issue->location = location ? safe_strdup(location) : NULL;
    issue->description = description ? safe_strdup(description) : NULL;
    issue->suggestion = suggestion ? safe_strdup(suggestion) : NULL;
    
    issue->is_confirmed = 0;
    issue->occurrence_count = 1;
    time_t now = time(NULL);
    issue->first_seen = now;
    issue->last_seen = now;
    issue->node = NULL;
    issue->ast_node = NULL;
    issue->pc = 0;
    
    analyzer->stats.total_issues++;
    if (severity == SEVERITY_CRITICAL) {
        analyzer->stats.critical_issues++;
        // Proteção: verifica se callback é válido antes de chamar
        if (analyzer->on_critical_issue != NULL) {
            // Tenta chamar callback, mas pode falhar silenciosamente se inválido
            // (em produção, poderia usar signal handlers ou try-catch)
            analyzer->on_critical_issue(issue);
        }
    }
    
    // Proteção: verifica se callback é válido antes de chamar
    if (analyzer->on_issue_detected != NULL) {
        analyzer->on_issue_detected(issue);
    }
}

void failure_detect_null_pointers(FailureAnalyzer* analyzer) {
    if (analyzer == NULL) return;
    
    // (Simplificado - em produção, analisaria AST para detectar null pointers)
    // Exemplo:
    // add_issue(analyzer, FAILURE_NULL_POINTER, SEVERITY_HIGH,
    //           "file.ast:42", "Possible null pointer dereference",
    //           "Add null check before dereference");
}

void failure_detect_division_by_zero(FailureAnalyzer* analyzer) {
    if (analyzer == NULL) return;
    
    // Verifica se VM e programa são válidos antes de acessar
    if (analyzer->vm == NULL) return;
    if (analyzer->vm->program == NULL) return;
    if (analyzer->vm->program->instructions == NULL) return;
    
    // Verifica se instruction_count é válido
    if (analyzer->vm->program->instruction_count == 0) return;
    
    // Proteção adicional: verifica se instruction_count não é muito grande (proteção contra corrupção)
    if (analyzer->vm->program->instruction_count > 100000) {
        // Valor suspeito, provavelmente corrupção de memória
        return;
    }
    
    // Analisa instruções de divisão
    // Limita o loop para evitar acesso fora dos bounds
    size_t max_i = analyzer->vm->program->instruction_count;
    for (size_t i = 0; i < max_i; i++) {
        // Verifica se o índice é válido antes de acessar
        if (i >= analyzer->vm->program->instruction_count) break;
        
        // Proteção adicional: verifica se o ponteiro de instrução é válido
        if (analyzer->vm->program->instructions == NULL) break;
        
        // Acessa a instrução com cuidado
        // Nota: Não podemos verificar se o ponteiro é válido diretamente,
        // mas podemos limitar o acesso ao array conhecido
        if (i < max_i) {
            // Verifica se o opcode está em um range válido (0-54 baseado em OpCode enum)
            unsigned int op = (unsigned int)analyzer->vm->program->instructions[i].op;
            if (op <= 54 && op == OP_DIV) {
                // Verifica se divisor pode ser zero
                // (Simplificado - em produção, faria análise de fluxo de dados)
                add_issue(analyzer, FAILURE_DIVISION_BY_ZERO, SEVERITY_CRITICAL,
                          "bytecode", "Possible division by zero",
                          "Add check to ensure divisor is not zero");
                // Limita a uma issue por tipo para evitar spam
                break;
            }
        }
    }
}

void failure_detect_array_bounds(FailureAnalyzer* analyzer) {
    // (Simplificado)
}

void failure_detect_memory_leaks(FailureAnalyzer* analyzer) {
    // (Simplificado)
}

void failure_detect_deadlocks(FailureAnalyzer* analyzer) {
    // (Simplificado)
}

void failure_detect_infinite_loops(FailureAnalyzer* analyzer) {
    if (analyzer == NULL) return;
    if (analyzer->graph == NULL) return;
    
    // Proteção adicional: verifica se o grafo tem estrutura válida
    if (analyzer->graph->nodes == NULL) return;
    if (analyzer->graph->node_count == 0) return;
    
    // Proteção contra node_count muito grande (possível corrupção)
    if (analyzer->graph->node_count > 100000) return;
    
    // Detecta loops sem condições de saída
    // (Simplificado - em produção, analisaria CFG para ciclos)
    // Por enquanto, apenas verifica se o grafo é válido sem acessar nós
}

void failure_detect_uninitialized(FailureAnalyzer* analyzer) {
    // (Simplificado)
}

void failure_detect_race_conditions(FailureAnalyzer* analyzer) {
    // (Simplificado)
}

/* =============================================================================
 * RELATÓRIOS
 * ============================================================================= */

void failure_generate_report(FailureAnalyzer* analyzer, const char* filename) {
    if (analyzer == NULL || filename == NULL) return;
    if (analyzer->issues == NULL) return;
    
    // Proteção: verifica se issue_count é válido
    if (analyzer->issue_count == 0) {
        // Ainda gera o relatório, mas sem problemas
    } else if (analyzer->issue_count > 10000) {
        printf("  [Warning] Contagem de problemas suspeita, pulando geração de relatório.\n");
        return;
    }
    
    FILE* f = fopen(filename, "w");
    if (f == NULL) {
        printf("  [Warning] Não foi possível criar arquivo de relatório: %s\n", filename);
        return;
    }
    
    fprintf(f, "# Relatório de Análise de Falhas\n\n");
    fprintf(f, "Total de problemas: %zu\n", analyzer->stats.total_issues);
    fprintf(f, "Problemas críticos: %zu\n", analyzer->stats.critical_issues);
    fprintf(f, "Problemas confirmados: %zu\n", analyzer->stats.confirmed_issues);
    fprintf(f, "\n## Problemas Detectados\n\n");
    
    for (size_t i = 0; i < analyzer->issue_count; i++) {
        // Proteção adicional: verifica se o índice é válido
        if (i >= analyzer->issue_capacity) break;
        
        FailureIssue* issue = &analyzer->issues[i];
        if (issue == NULL) continue;
        
        const char* severity_str = "unknown";
        switch (issue->severity) {
            case SEVERITY_LOW: severity_str = "Baixa"; break;
            case SEVERITY_MEDIUM: severity_str = "Média"; break;
            case SEVERITY_HIGH: severity_str = "Alta"; break;
            case SEVERITY_CRITICAL: severity_str = "Crítica"; break;
        }
        
        fprintf(f, "### %zu. %s [%s]\n", i + 1, 
                issue->description ? issue->description : "Unknown",
                severity_str);
        if (issue->location) {
            fprintf(f, "**Localização**: %s\n", issue->location);
        }
        if (issue->suggestion) {
            fprintf(f, "**Sugestão**: %s\n", issue->suggestion);
        }
        fprintf(f, "\n");
    }
    
    fclose(f);
}

void failure_list_issues(FailureAnalyzer* analyzer) {
    if (analyzer == NULL) return;
    
    printf("\n=== Problemas Detectados ===\n");
    for (size_t i = 0; i < analyzer->issue_count; i++) {
        FailureIssue* issue = &analyzer->issues[i];
        const char* severity_str = "unknown";
        switch (issue->severity) {
            case SEVERITY_LOW: severity_str = "Baixa"; break;
            case SEVERITY_MEDIUM: severity_str = "Média"; break;
            case SEVERITY_HIGH: severity_str = "Alta"; break;
            case SEVERITY_CRITICAL: severity_str = "Crítica"; break;
        }
        
        printf("%zu. [%s] %s\n", i + 1, severity_str,
               issue->description ? issue->description : "Unknown");
        if (issue->location) {
            printf("   Localização: %s\n", issue->location);
        }
        if (issue->suggestion) {
            printf("   Sugestão: %s\n", issue->suggestion);
        }
    }
}

void failure_list_critical(FailureAnalyzer* analyzer) {
    if (analyzer == NULL) return;
    if (analyzer->issues == NULL) return;
    
    // Proteção: verifica se issue_count é válido
    if (analyzer->issue_count == 0) {
        printf("\n=== Problemas Críticos ===\n");
        printf("Nenhum problema crítico encontrado.\n");
        return;
    }
    
    // Proteção: verifica se issue_count não é muito grande (possível corrupção)
    if (analyzer->issue_count > 10000) {
        printf("\n=== Problemas Críticos ===\n");
        printf("  [Warning] Contagem de problemas suspeita, pulando listagem.\n");
        return;
    }
    
    printf("\n=== Problemas Críticos ===\n");
    int found = 0;
    for (size_t i = 0; i < analyzer->issue_count; i++) {
        // Proteção adicional: verifica se o índice é válido
        if (i >= analyzer->issue_capacity) break;
        
        FailureIssue* issue = &analyzer->issues[i];
        if (issue != NULL && issue->severity == SEVERITY_CRITICAL) {
            printf("%zu. %s\n", i + 1,
                   issue->description ? issue->description : "Unknown");
            if (issue->location) {
                printf("   Localização: %s\n", issue->location);
            }
            found = 1;
        }
    }
    if (!found) {
        printf("Nenhum problema crítico encontrado.\n");
    }
}

void failure_get_stats(FailureAnalyzer* analyzer, size_t* total, size_t* critical,
                       size_t* confirmed) {
    if (analyzer == NULL) return;
    
    if (total) *total = analyzer->stats.total_issues;
    if (critical) *critical = analyzer->stats.critical_issues;
    if (confirmed) *confirmed = analyzer->stats.confirmed_issues;
}

