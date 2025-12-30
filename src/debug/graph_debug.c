/**
 * =============================================================================
 * ASTERON GRAPH-BASED DEBUGGER - Implementação
 * =============================================================================
 */

#define _GNU_SOURCE
#include "graph_debug.h"
#include <string.h>

/* strdup para compatibilidade */
#ifndef strdup
static char* my_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = (char*)malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}
#define strdup my_strdup
#endif

/* Variáveis globais */
int g_debug_enabled = 0;
ExecutionTrace* g_trace = NULL;

/* =============================================================================
 * TRACE DE EXECUÇÃO
 * ============================================================================= */

ExecutionTrace* trace_create(void) {
    ExecutionTrace* trace = (ExecutionTrace*)malloc(sizeof(ExecutionTrace));
    trace->capacity = 64;
    trace->count = 0;
    trace->failed_at = -1;
    trace->checkpoints = (DebugCheckpoint*)malloc(
        sizeof(DebugCheckpoint) * trace->capacity
    );
    return trace;
}

void trace_checkpoint(ExecutionTrace* trace, const char* name, int line,
                      const char* value, NodeDebugState state) {
    if (!trace) return;
    
    /* Expande se necessário */
    if (trace->count >= trace->capacity) {
        trace->capacity *= 2;
        trace->checkpoints = (DebugCheckpoint*)realloc(
            trace->checkpoints,
            sizeof(DebugCheckpoint) * trace->capacity
        );
    }
    
    DebugCheckpoint* cp = &trace->checkpoints[trace->count];
    strncpy(cp->name, name, sizeof(cp->name) - 1);
    cp->name[sizeof(cp->name) - 1] = '\0';
    cp->line = line;
    cp->state = state;
    
    if (value) {
        strncpy(cp->value, value, sizeof(cp->value) - 1);
        cp->value[sizeof(cp->value) - 1] = '\0';
    } else {
        cp->value[0] = '\0';
    }
    
    cp->error_msg[0] = '\0';
    
    trace->count++;
}

void trace_mark_failure(ExecutionTrace* trace, const char* error_msg) {
    if (!trace || trace->count == 0) return;
    
    trace->failed_at = trace->count - 1;
    
    DebugCheckpoint* cp = &trace->checkpoints[trace->failed_at];
    cp->state = NODE_FAILED;
    
    if (error_msg) {
        strncpy(cp->error_msg, error_msg, sizeof(cp->error_msg) - 1);
        cp->error_msg[sizeof(cp->error_msg) - 1] = '\0';
    }
}

void trace_print(ExecutionTrace* trace) {
    if (!trace) return;
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║              TRACE DE EXECUÇÃO (Control Flow Graph)             ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    for (int i = 0; i < trace->count; i++) {
        DebugCheckpoint* cp = &trace->checkpoints[i];
        
        /* Ícone de estado */
        const char* icon;
        const char* color_start = "";
        const char* color_end = "";
        
        switch (cp->state) {
            case NODE_SUCCESS:
                icon = "✓";
                break;
            case NODE_FAILED:
                icon = "✗";
                break;
            case NODE_EXECUTING:
                icon = "→";
                break;
            case NODE_SKIPPED:
                icon = "○";
                break;
            default:
                icon = "?";
        }
        
        /* Formato do checkpoint */
        if (i == trace->failed_at) {
            printf("  %s [FALHA] %s (linha %d)\n", icon, cp->name, cp->line);
            if (cp->error_msg[0]) {
                printf("      └─ Erro: %s\n", cp->error_msg);
            }
        } else {
            printf("  %s %s", icon, cp->name);
            if (cp->value[0]) {
                printf(" = %s", cp->value);
            }
            printf("\n");
        }
        
        /* Seta para próximo */
        if (i < trace->count - 1 && cp->state != NODE_FAILED) {
            printf("      │\n");
            printf("      ▼\n");
        }
    }
    
    printf("\n");
    
    /* Resumo */
    if (trace->failed_at >= 0) {
        printf("┌─────────────────────────────────────────────────────────────────┐\n");
        printf("│ ⚠️  FALHA DETECTADA no checkpoint: %-29s│\n", 
               trace->checkpoints[trace->failed_at].name);
        printf("└─────────────────────────────────────────────────────────────────┘\n");
    } else {
        printf("┌─────────────────────────────────────────────────────────────────┐\n");
        printf("│ ✓ Execução concluída com sucesso (%d checkpoints)               │\n", 
               trace->count);
        printf("└─────────────────────────────────────────────────────────────────┘\n");
    }
}

void trace_free(ExecutionTrace* trace) {
    if (!trace) return;
    if (trace->checkpoints) free(trace->checkpoints);
    free(trace);
}

/* =============================================================================
 * ANÁLISE DE DEPENDÊNCIAS
 * ============================================================================= */

/* Mapa de dependências conhecidas (hardcoded por simplicidade) */
static const char* tcp_dependencies[][4] = {
    {"bytes_sent", "tcp_send", "sock", "request"},
    {"response", "tcp_recv", "sock", NULL},
    {"sock", "tcp_connect", "host", "port"},
    {"html", "http_get", "url", NULL},
    {NULL}
};

DependencyAnalysis* analyze_dependencies(const char* var_name) {
    DependencyAnalysis* analysis = (DependencyAnalysis*)malloc(sizeof(DependencyAnalysis));
    memset(analysis, 0, sizeof(DependencyAnalysis));
    
    strncpy(analysis->var_name, var_name, sizeof(analysis->var_name) - 1);
    
    /* Procura dependências conhecidas */
    for (int i = 0; tcp_dependencies[i][0] != NULL; i++) {
        if (strcmp(tcp_dependencies[i][0], var_name) == 0) {
            for (int j = 1; j < 4 && tcp_dependencies[i][j] != NULL; j++) {
                analysis->dependencies[analysis->dep_count] = 
                    strdup(tcp_dependencies[i][j]);
                analysis->dep_count++;
            }
            break;
        }
    }
    
    /* Sugere verificações baseadas nas dependências */
    if (strcmp(var_name, "bytes_sent") == 0) {
        analysis->suggested_checks[analysis->check_count++] = 
            strdup("Verificar se sock > 0 (conexão válida)");
        analysis->suggested_checks[analysis->check_count++] = 
            strdup("Verificar se request não está vazio");
        analysis->suggested_checks[analysis->check_count++] = 
            strdup("Testar tcp_connect isoladamente");
    } else if (strcmp(var_name, "response") == 0) {
        analysis->suggested_checks[analysis->check_count++] = 
            strdup("Verificar se bytes_sent > 0 (envio bem-sucedido)");
        analysis->suggested_checks[analysis->check_count++] = 
            strdup("Verificar timeout de recv");
        analysis->suggested_checks[analysis->check_count++] = 
            strdup("Testar is_connected(sock)");
    } else if (strcmp(var_name, "sock") == 0) {
        analysis->suggested_checks[analysis->check_count++] = 
            strdup("Verificar resolução DNS com resolve()");
        analysis->suggested_checks[analysis->check_count++] = 
            strdup("Verificar porta acessível (firewall?)");
        analysis->suggested_checks[analysis->check_count++] = 
            strdup("Testar com timeout maior");
    }
    
    return analysis;
}

/* =============================================================================
 * DIAGNÓSTICO COMPLETO
 * ============================================================================= */

GraphDiagnosis* diagnose_failure(ExecutionTrace* trace, const char* failed_var) {
    GraphDiagnosis* diag = (GraphDiagnosis*)malloc(sizeof(GraphDiagnosis));
    memset(diag, 0, sizeof(GraphDiagnosis));
    
    diag->trace = *trace; /* Copia shallow do trace */
    
    /* Analisa dependências */
    diag->analysis_count = 1;
    diag->analyses = (DependencyAnalysis*)malloc(sizeof(DependencyAnalysis));
    *diag->analyses = *analyze_dependencies(failed_var);
    
    /* Gera diagnóstico baseado no padrão de erro */
    if (strcmp(failed_var, "bytes_sent") == 0) {
        snprintf(diag->diagnosis, sizeof(diag->diagnosis),
            "DIAGNÓSTICO: tcp_send retornou -1\n\n"
            "O socket foi conectado (handle > 0), mas o envio falhou.\n\n"
            "POSSÍVEIS CAUSAS:\n"
            "  1. Socket foi fechado pelo servidor antes do envio\n"
            "  2. Timeout de envio expirado\n"
            "  3. Erro de rede (firewall, conexão instável)\n"
            "  4. Buffer de envio cheio\n"
        );
        
        snprintf(diag->suggested_fix, sizeof(diag->suggested_fix),
            "SUGESTÃO:\n"
            "  1. Usar is_connected(sock) antes do envio\n"
            "  2. Aumentar timeout: set_timeout(sock, 60000)\n"
            "  3. Verificar se request está correto\n"
            "  4. Tentar reconectar e reenviar\n"
        );
    } else if (strcmp(failed_var, "response") == 0) {
        snprintf(diag->diagnosis, sizeof(diag->diagnosis),
            "DIAGNÓSTICO: tcp_recv retornou nil\n\n"
            "O socket pode estar válido, mas não há dados para receber.\n\n"
            "POSSÍVEIS CAUSAS:\n"
            "  1. Envio anterior falhou (bytes_sent = -1)\n"
            "  2. Timeout de recepção expirado\n"
            "  3. Servidor não respondeu\n"
            "  4. Conexão fechada pelo servidor\n"
        );
        
        snprintf(diag->suggested_fix, sizeof(diag->suggested_fix),
            "SUGESTÃO:\n"
            "  1. Verificar se bytes_sent > 0\n"
            "  2. Usar tcp_recv_all() para receber tudo\n"
            "  3. Aumentar timeout do recv\n"
            "  4. Verificar formato do request HTTP\n"
        );
    } else if (strcmp(failed_var, "sock") == 0) {
        snprintf(diag->diagnosis, sizeof(diag->diagnosis),
            "DIAGNÓSTICO: tcp_connect retornou -1\n\n"
            "Não foi possível estabelecer conexão TCP.\n\n"
            "POSSÍVEIS CAUSAS:\n"
            "  1. Host não encontrado (DNS falhou)\n"
            "  2. Porta fechada ou filtrada\n"
            "  3. Firewall bloqueando conexão\n"
            "  4. Timeout de conexão\n"
        );
        
        snprintf(diag->suggested_fix, sizeof(diag->suggested_fix),
            "SUGESTÃO:\n"
            "  1. Testar resolve() antes do connect\n"
            "  2. Verificar porta (80 para HTTP)\n"
            "  3. Tentar com timeout maior\n"
            "  4. Verificar conectividade de rede\n"
        );
    }
    
    return diag;
}

void diagnosis_print(GraphDiagnosis* diag) {
    if (!diag) return;
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                    DIAGNÓSTICO BASEADO EM GRAFOS                ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* Grafo de dependências */
    if (diag->analysis_count > 0) {
        printf("┌─────────────────────────────────────────────────────────────────┐\n");
        printf("│ GRAFO DE DEPENDÊNCIAS                                           │\n");
        printf("├─────────────────────────────────────────────────────────────────┤\n");
        
        DependencyAnalysis* a = &diag->analyses[0];
        printf("│ Variável: %s\n", a->var_name);
        printf("│\n");
        printf("│   %s\n", a->var_name);
        for (int i = 0; i < a->dep_count; i++) {
            printf("│     └─ depende de: %s\n", a->dependencies[i]);
        }
        printf("│\n");
        printf("│ Verificações sugeridas:\n");
        for (int i = 0; i < a->check_count; i++) {
            printf("│   %d. %s\n", i+1, a->suggested_checks[i]);
        }
        printf("└─────────────────────────────────────────────────────────────────┘\n");
    }
    
    printf("\n");
    
    /* Diagnóstico */
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("%s", diag->diagnosis);
    printf("└─────────────────────────────────────────────────────────────────┘\n");
    
    printf("\n");
    
    /* Sugestão de correção */
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("%s", diag->suggested_fix);
    printf("└─────────────────────────────────────────────────────────────────┘\n");
}

void diagnosis_free(GraphDiagnosis* diag) {
    if (!diag) return;
    
    /* Libera análises */
    for (int i = 0; i < diag->analysis_count; i++) {
        DependencyAnalysis* a = &diag->analyses[i];
        for (int j = 0; j < a->dep_count; j++) {
            if (a->dependencies[j]) free(a->dependencies[j]);
        }
        for (int j = 0; j < a->check_count; j++) {
            if (a->suggested_checks[j]) free(a->suggested_checks[j]);
        }
    }
    if (diag->analyses) free(diag->analyses);
    
    free(diag);
}

/* =============================================================================
 * FUNÇÕES AUXILIARES DE DEBUG PARA TCP
 * ============================================================================= */

void debug_tcp_connection(int handle, const char* host, int port) {
    printf("\n┌─ DEBUG TCP CONNECTION ─────────────────────────────────────────┐\n");
    printf("│ Host: %-54s│\n", host);
    printf("│ Port: %-54d│\n", port);
    printf("│ Handle: %-52d│\n", handle);
    
    if (handle > 0) {
        printf("│ Status: ✓ CONECTADO                                           │\n");
    } else {
        printf("│ Status: ✗ FALHA NA CONEXÃO                                    │\n");
    }
    printf("└─────────────────────────────────────────────────────────────────┘\n");
}

void debug_tcp_send(int handle, int bytes_sent, const char* data_preview) {
    printf("\n┌─ DEBUG TCP SEND ───────────────────────────────────────────────┐\n");
    printf("│ Socket Handle: %-49d│\n", handle);
    printf("│ Bytes Enviados: %-48d│\n", bytes_sent);
    
    if (data_preview) {
        char preview[50];
        strncpy(preview, data_preview, 45);
        preview[45] = '\0';
        if (strlen(data_preview) > 45) strcat(preview, "...");
        printf("│ Data: %-58s│\n", preview);
    }
    
    if (bytes_sent > 0) {
        printf("│ Status: ✓ ENVIADO                                             │\n");
    } else {
        printf("│ Status: ✗ FALHA NO ENVIO                                      │\n");
        printf("│                                                                 │\n");
        printf("│ ⚠️  DIAGNÓSTICO:                                               │\n");
        printf("│   - Socket pode ter sido fechado                               │\n");
        printf("│   - Verifique is_connected() antes de enviar                   │\n");
    }
    printf("└─────────────────────────────────────────────────────────────────┘\n");
}

void debug_tcp_recv(int handle, const char* data, int data_len) {
    printf("\n┌─ DEBUG TCP RECV ───────────────────────────────────────────────┐\n");
    printf("│ Socket Handle: %-49d│\n", handle);
    printf("│ Bytes Recebidos: %-47d│\n", data_len);
    
    if (data && data_len > 0) {
        char preview[50];
        strncpy(preview, data, 45);
        preview[45] = '\0';
        for (int i = 0; i < 45 && preview[i]; i++) {
            if (preview[i] == '\r' || preview[i] == '\n') preview[i] = ' ';
        }
        printf("│ Data: %-58s│\n", preview);
        printf("│ Status: ✓ RECEBIDO                                            │\n");
    } else {
        printf("│ Data: (nil)                                                    │\n");
        printf("│ Status: ✗ NENHUM DADO RECEBIDO                                │\n");
    }
    printf("└─────────────────────────────────────────────────────────────────┘\n");
}

