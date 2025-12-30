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

/**
 * =============================================================================
 * VISUAL DEBUGGER - Developer Experience
 * =============================================================================
 * 
 * Ferramentas de desenvolvimento visual:
 * - Debug visual
 * - Timeline de execução
 * - Visualização do grafo vivo
 * - Replay determinístico
 * 
 * =============================================================================
 */

#ifndef VISUAL_DEBUGGER_H
#define VISUAL_DEBUGGER_H

#include "../../graph/unified_graph.h"
#include "../vm/vm.h"
#include <stdbool.h>
#include <stdint.h>

// Forward declarations
typedef struct VisualDebugger VisualDebugger;
typedef struct ExecutionTimeline ExecutionTimeline;
typedef struct TimelineEvent TimelineEvent;
typedef struct Breakpoint Breakpoint;

// =============================================================================
// EVENTO DA TIMELINE
// =============================================================================

typedef enum {
    EVENT_FUNCTION_CALL,
    EVENT_FUNCTION_RETURN,
    EVENT_VARIABLE_CHANGE,
    EVENT_BREAKPOINT_HIT,
    EVENT_EXCEPTION,
    EVENT_JIT_COMPILE,
    EVENT_DEOPTIMIZE,
    EVENT_MEMORY_ALLOC,
    EVENT_MEMORY_FREE
} TimelineEventType;

typedef struct {
    TimelineEventType type;
    uint64_t timestamp_ns;       // Timestamp absoluto
    uint64_t relative_ns;        // Timestamp relativo ao início
    
    // Dados do evento
    union {
        struct {
            const char* function_name;
            size_t arg_count;
        } function_call;
        
        struct {
            const char* function_name;
            const char* return_value;
        } function_return;
        
        struct {
            const char* variable_name;
            const char* old_value;
            const char* new_value;
        } variable_change;
        
        struct {
            const char* file;
            size_t line;
        } breakpoint;
        
        struct {
            const char* message;
        } exception;
    } data;
    
    // Contexto
    const char* file;
    size_t line;
    size_t column;
} TimelineEvent;

// =============================================================================
// TIMELINE DE EXECUÇÃO
// =============================================================================

typedef struct {
    TimelineEvent* events;
    size_t event_count;
    size_t event_capacity;
    
    uint64_t start_time_ns;
    uint64_t end_time_ns;
    uint64_t total_duration_ns;
    
    bool is_recording;
} ExecutionTimeline;

// =============================================================================
// BREAKPOINT
// =============================================================================

typedef struct {
    const char* file;
    size_t line;
    size_t column;
    bool enabled;
    int hit_count;
    int condition;  // ID de condição (se houver)
} Breakpoint;

// =============================================================================
// VISUAL DEBUGGER
// =============================================================================

typedef struct {
    VM* vm;                      // VM (referência)
    UnifiedGraph* graph;         // Grafo unificado (referência)
    
    // Timeline
    ExecutionTimeline timeline;
    
    // Breakpoints
    Breakpoint* breakpoints;
    size_t breakpoint_count;
    size_t breakpoint_capacity;
    
    // Estado
    bool is_debugging;
    bool is_paused;
    size_t current_event_index;
    
    // Replay
    bool is_replaying;
    size_t replay_speed;         // 1x, 2x, 4x, etc.
    
    // Visualização
    struct {
        bool show_graph;         // Mostrar grafo?
        bool show_timeline;      // Mostrar timeline?
        bool show_variables;     // Mostrar variáveis?
        bool show_call_stack;    // Mostrar call stack?
        bool highlight_hot_paths; // Destacar hot paths?
    } visualization;
    
    // Callbacks
    void (*on_breakpoint)(VisualDebugger* debugger, Breakpoint* bp);
    void (*on_event)(VisualDebugger* debugger, TimelineEvent* event);
} VisualDebugger;

// =============================================================================
// API
// =============================================================================

// Criação e destruição
VisualDebugger* visual_debugger_create(VM* vm, UnifiedGraph* graph);
void visual_debugger_destroy(VisualDebugger* debugger);

// Timeline
void visual_debugger_start_recording(VisualDebugger* debugger);
void visual_debugger_stop_recording(VisualDebugger* debugger);
void visual_debugger_record_event(VisualDebugger* debugger, TimelineEvent* event);
ExecutionTimeline* visual_debugger_get_timeline(VisualDebugger* debugger);

// Breakpoints
Breakpoint* visual_debugger_add_breakpoint(VisualDebugger* debugger, 
                                           const char* file, 
                                           size_t line);
void visual_debugger_remove_breakpoint(VisualDebugger* debugger, Breakpoint* bp);
void visual_debugger_enable_breakpoint(VisualDebugger* debugger, Breakpoint* bp);
void visual_debugger_disable_breakpoint(VisualDebugger* debugger, Breakpoint* bp);

// Controle de execução
void visual_debugger_pause(VisualDebugger* debugger);
void visual_debugger_resume(VisualDebugger* debugger);
void visual_debugger_step_over(VisualDebugger* debugger);
void visual_debugger_step_into(VisualDebugger* debugger);
void visual_debugger_step_out(VisualDebugger* debugger);

// Replay
void visual_debugger_start_replay(VisualDebugger* debugger, ExecutionTimeline* timeline);
void visual_debugger_stop_replay(VisualDebugger* debugger);
void visual_debugger_set_replay_speed(VisualDebugger* debugger, size_t speed);

// Visualização
void visual_debugger_export_timeline_json(VisualDebugger* debugger, const char* filename);
void visual_debugger_export_graph_json(VisualDebugger* debugger, const char* filename);
void visual_debugger_export_call_stack(VisualDebugger* debugger, const char* filename);

// Debug
void visual_debugger_print_state(VisualDebugger* debugger);
void visual_debugger_print_timeline(VisualDebugger* debugger);

#endif // VISUAL_DEBUGGER_H


