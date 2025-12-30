/*
 * Asteron Runtime - (C) 2025 Josué Junior da Cruz de Freitas
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _POSIX_C_SOURCE 200809L
#include "visual_debugger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// =============================================================================
// UTILITÁRIOS
// =============================================================================

static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// =============================================================================
// CRIAÇÃO E DESTRUIÇÃO
// =============================================================================

VisualDebugger* visual_debugger_create(VM* vm, UnifiedGraph* graph) {
    if (!vm || !graph) return NULL;
    
    VisualDebugger* debugger = (VisualDebugger*)calloc(1, sizeof(VisualDebugger));
    if (!debugger) return NULL;
    
    debugger->vm = vm;
    debugger->graph = graph;
    
    // Inicializar timeline
    debugger->timeline.event_capacity = 1024;
    debugger->timeline.events = (TimelineEvent*)calloc(
        debugger->timeline.event_capacity, sizeof(TimelineEvent));
    
    // Inicializar breakpoints
    debugger->breakpoint_capacity = 32;
    debugger->breakpoints = (Breakpoint*)calloc(
        debugger->breakpoint_capacity, sizeof(Breakpoint));
    
    // Configuração padrão de visualização
    debugger->visualization.show_graph = true;
    debugger->visualization.show_timeline = true;
    debugger->visualization.show_variables = true;
    debugger->visualization.show_call_stack = true;
    debugger->visualization.highlight_hot_paths = true;
    
    debugger->replay_speed = 1;
    
    return debugger;
}

void visual_debugger_destroy(VisualDebugger* debugger) {
    if (!debugger) return;
    
    // Liberar eventos
    for (size_t i = 0; i < debugger->timeline.event_count; i++) {
        TimelineEvent* e = &debugger->timeline.events[i];
        if (e->data.function_call.function_name) {
            free((void*)e->data.function_call.function_name);
        }
        if (e->data.function_return.function_name) {
            free((void*)e->data.function_return.function_name);
        }
        if (e->data.function_return.return_value) {
            free((void*)e->data.function_return.return_value);
        }
        if (e->data.variable_change.variable_name) {
            free((void*)e->data.variable_change.variable_name);
        }
        if (e->data.variable_change.old_value) {
            free((void*)e->data.variable_change.old_value);
        }
        if (e->data.variable_change.new_value) {
            free((void*)e->data.variable_change.new_value);
        }
        if (e->data.breakpoint.file) {
            free((void*)e->data.breakpoint.file);
        }
        if (e->data.exception.message) {
            free((void*)e->data.exception.message);
        }
        if (e->file) {
            free((void*)e->file);
        }
    }
    if (debugger->timeline.events) free(debugger->timeline.events);
    
    // Liberar breakpoints
    for (size_t i = 0; i < debugger->breakpoint_count; i++) {
        if (debugger->breakpoints[i].file) {
            free((void*)debugger->breakpoints[i].file);
        }
    }
    if (debugger->breakpoints) free(debugger->breakpoints);
    
    free(debugger);
}

// =============================================================================
// TIMELINE
// =============================================================================

void visual_debugger_start_recording(VisualDebugger* debugger) {
    if (!debugger) return;
    
    debugger->timeline.is_recording = true;
    debugger->timeline.start_time_ns = get_timestamp_ns();
    debugger->timeline.event_count = 0;
}

void visual_debugger_stop_recording(VisualDebugger* debugger) {
    if (!debugger) return;
    
    debugger->timeline.is_recording = false;
    debugger->timeline.end_time_ns = get_timestamp_ns();
    debugger->timeline.total_duration_ns = 
        debugger->timeline.end_time_ns - debugger->timeline.start_time_ns;
}

void visual_debugger_record_event(VisualDebugger* debugger, TimelineEvent* event) {
    if (!debugger || !event || !debugger->timeline.is_recording) return;
    
    // Expandir array se necessário
    if (debugger->timeline.event_count >= debugger->timeline.event_capacity) {
        debugger->timeline.event_capacity *= 2;
        debugger->timeline.events = (TimelineEvent*)realloc(debugger->timeline.events, 
            debugger->timeline.event_capacity * sizeof(TimelineEvent));
    }
    
    TimelineEvent* e = &debugger->timeline.events[debugger->timeline.event_count++];
    *e = *event;
    
    // Ajustar timestamp relativo
    e->relative_ns = e->timestamp_ns - debugger->timeline.start_time_ns;
    
    // Callback
    if (debugger->on_event) {
        debugger->on_event(debugger, e);
    }
}

ExecutionTimeline* visual_debugger_get_timeline(VisualDebugger* debugger) {
    return debugger ? &debugger->timeline : NULL;
}

// =============================================================================
// BREAKPOINTS
// =============================================================================

Breakpoint* visual_debugger_add_breakpoint(VisualDebugger* debugger, 
                                           const char* file, 
                                           size_t line) {
    if (!debugger || !file) return NULL;
    
    // Expandir array se necessário
    if (debugger->breakpoint_count >= debugger->breakpoint_capacity) {
        debugger->breakpoint_capacity *= 2;
        debugger->breakpoints = (Breakpoint*)realloc(debugger->breakpoints, 
            debugger->breakpoint_capacity * sizeof(Breakpoint));
    }
    
    Breakpoint* bp = &debugger->breakpoints[debugger->breakpoint_count++];
    bp->file = strdup(file);
    bp->line = line;
    bp->column = 0;
    bp->enabled = true;
    bp->hit_count = 0;
    bp->condition = -1;
    
    return bp;
}

void visual_debugger_remove_breakpoint(VisualDebugger* debugger, Breakpoint* bp) {
    if (!debugger || !bp) return;
    
    // Encontrar e remover
    for (size_t i = 0; i < debugger->breakpoint_count; i++) {
        if (&debugger->breakpoints[i] == bp) {
            // Mover todos os breakpoints para frente
            if (debugger->breakpoints[i].file) {
                free((void*)debugger->breakpoints[i].file);
            }
            memmove(&debugger->breakpoints[i], 
                   &debugger->breakpoints[i + 1],
                   (debugger->breakpoint_count - i - 1) * sizeof(Breakpoint));
            debugger->breakpoint_count--;
            return;
        }
    }
}

void visual_debugger_enable_breakpoint(VisualDebugger* debugger, Breakpoint* bp) {
    if (debugger && bp) {
        bp->enabled = true;
    }
}

void visual_debugger_disable_breakpoint(VisualDebugger* debugger, Breakpoint* bp) {
    if (debugger && bp) {
        bp->enabled = false;
    }
}

// =============================================================================
// CONTROLE DE EXECUÇÃO
// =============================================================================

void visual_debugger_pause(VisualDebugger* debugger) {
    if (debugger) {
        debugger->is_paused = true;
    }
}

void visual_debugger_resume(VisualDebugger* debugger) {
    if (debugger) {
        debugger->is_paused = false;
    }
}

void visual_debugger_step_over(VisualDebugger* debugger) {
    if (!debugger) return;
    
    // TODO: Implementar step over (executar linha atual e parar na próxima)
    debugger->is_paused = true;
}

void visual_debugger_step_into(VisualDebugger* debugger) {
    if (!debugger) return;
    
    // TODO: Implementar step into (entrar em função se houver chamada)
    debugger->is_paused = true;
}

void visual_debugger_step_out(VisualDebugger* debugger) {
    if (!debugger) return;
    
    // TODO: Implementar step out (sair da função atual)
    debugger->is_paused = true;
}

// =============================================================================
// REPLAY
// =============================================================================

void visual_debugger_start_replay(VisualDebugger* debugger, ExecutionTimeline* timeline) {
    if (!debugger || !timeline) return;
    
    debugger->is_replaying = true;
    debugger->current_event_index = 0;
    // TODO: Implementar replay real
}

void visual_debugger_stop_replay(VisualDebugger* debugger) {
    if (debugger) {
        debugger->is_replaying = false;
    }
}

void visual_debugger_set_replay_speed(VisualDebugger* debugger, size_t speed) {
    if (debugger) {
        debugger->replay_speed = speed > 0 ? speed : 1;
    }
}

// =============================================================================
// VISUALIZAÇÃO
// =============================================================================

void visual_debugger_export_timeline_json(VisualDebugger* debugger, const char* filename) {
    if (!debugger || !filename) return;
    
    FILE* f = fopen(filename, "w");
    if (!f) return;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"start_time_ns\": %llu,\n", (unsigned long long)debugger->timeline.start_time_ns);
    fprintf(f, "  \"end_time_ns\": %llu,\n", (unsigned long long)debugger->timeline.end_time_ns);
    fprintf(f, "  \"total_duration_ns\": %llu,\n", (unsigned long long)debugger->timeline.total_duration_ns);
    fprintf(f, "  \"events\": [\n");
    
    for (size_t i = 0; i < debugger->timeline.event_count; i++) {
        TimelineEvent* e = &debugger->timeline.events[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"type\": %d,\n", e->type);
        fprintf(f, "      \"timestamp_ns\": %llu,\n", (unsigned long long)e->timestamp_ns);
        fprintf(f, "      \"relative_ns\": %llu,\n", (unsigned long long)e->relative_ns);
        
        switch (e->type) {
            case EVENT_FUNCTION_CALL:
                fprintf(f, "      \"function_name\": \"%s\",\n", 
                    e->data.function_call.function_name ? e->data.function_call.function_name : "");
                fprintf(f, "      \"arg_count\": %zu\n", e->data.function_call.arg_count);
                break;
            case EVENT_FUNCTION_RETURN:
                fprintf(f, "      \"function_name\": \"%s\",\n", 
                    e->data.function_return.function_name ? e->data.function_return.function_name : "");
                fprintf(f, "      \"return_value\": \"%s\"\n", 
                    e->data.function_return.return_value ? e->data.function_return.return_value : "");
                break;
            case EVENT_VARIABLE_CHANGE:
                fprintf(f, "      \"variable_name\": \"%s\",\n", 
                    e->data.variable_change.variable_name ? e->data.variable_change.variable_name : "");
                fprintf(f, "      \"old_value\": \"%s\",\n", 
                    e->data.variable_change.old_value ? e->data.variable_change.old_value : "");
                fprintf(f, "      \"new_value\": \"%s\"\n", 
                    e->data.variable_change.new_value ? e->data.variable_change.new_value : "");
                break;
            case EVENT_BREAKPOINT_HIT:
                fprintf(f, "      \"file\": \"%s\",\n", 
                    e->data.breakpoint.file ? e->data.breakpoint.file : "");
                fprintf(f, "      \"line\": %zu\n", e->data.breakpoint.line);
                break;
            case EVENT_EXCEPTION:
                fprintf(f, "      \"message\": \"%s\"\n", 
                    e->data.exception.message ? e->data.exception.message : "");
                break;
            default:
                break;
        }
        
        fprintf(f, "    }%s\n", i < debugger->timeline.event_count - 1 ? "," : "");
    }
    
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    
    fclose(f);
}

void visual_debugger_export_graph_json(VisualDebugger* debugger, const char* filename) {
    if (!debugger || !filename) return;
    
    // TODO: Integrar com unified_graph_export_json
    (void)debugger;
    (void)filename;
}

void visual_debugger_export_call_stack(VisualDebugger* debugger, const char* filename) {
    if (!debugger || !filename) return;
    
    FILE* f = fopen(filename, "w");
    if (!f) return;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"call_stack\": []\n"); // TODO: Implementar call stack real
    fprintf(f, "}\n");
    
    fclose(f);
}

// =============================================================================
// DEBUG
// =============================================================================

void visual_debugger_print_state(VisualDebugger* debugger) {
    if (!debugger) return;
    
    printf("=== Visual Debugger State ===\n");
    printf("Is Debugging: %s\n", debugger->is_debugging ? "yes" : "no");
    printf("Is Paused: %s\n", debugger->is_paused ? "yes" : "no");
    printf("Is Recording: %s\n", debugger->timeline.is_recording ? "yes" : "no");
    printf("Is Replaying: %s\n", debugger->is_replaying ? "yes" : "no");
    printf("Events Recorded: %zu\n", debugger->timeline.event_count);
    printf("Breakpoints: %zu\n", debugger->breakpoint_count);
    printf("============================\n");
}

void visual_debugger_print_timeline(VisualDebugger* debugger) {
    if (!debugger) return;
    
    printf("=== Execution Timeline ===\n");
    for (size_t i = 0; i < debugger->timeline.event_count; i++) {
        TimelineEvent* e = &debugger->timeline.events[i];
        printf("[%llu ns] ", (unsigned long long)e->relative_ns);
        
        switch (e->type) {
            case EVENT_FUNCTION_CALL:
                printf("CALL %s\n", e->data.function_call.function_name ? e->data.function_call.function_name : "?");
                break;
            case EVENT_FUNCTION_RETURN:
                printf("RETURN %s\n", e->data.function_return.function_name ? e->data.function_return.function_name : "?");
                break;
            case EVENT_VARIABLE_CHANGE:
                printf("VAR %s = %s\n", 
                    e->data.variable_change.variable_name ? e->data.variable_change.variable_name : "?",
                    e->data.variable_change.new_value ? e->data.variable_change.new_value : "?");
                break;
            case EVENT_BREAKPOINT_HIT:
                printf("BREAKPOINT %s:%zu\n", 
                    e->data.breakpoint.file ? e->data.breakpoint.file : "?",
                    e->data.breakpoint.line);
                break;
            case EVENT_EXCEPTION:
                printf("EXCEPTION %s\n", e->data.exception.message ? e->data.exception.message : "?");
                break;
            default:
                printf("EVENT %d\n", e->type);
                break;
        }
    }
    printf("==========================\n");
}


