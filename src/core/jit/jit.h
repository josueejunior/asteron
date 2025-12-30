/*
 * Asteron Runtime - (C) 2024 Asteron Contributors
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

#ifndef JIT_H
#define JIT_H

#include "../../scheduler/scheduler.h"

// Configurações do JIT
#define JIT_HOT_THRESHOLD 5
#define MAX_TRACE_LENGTH 128

// Uma instrução especializada no Trace
typedef struct {
    OpCode op;
    size_t operand;
    ValueType type_hint; // Especialização: que tipo de dado passou aqui?
} TraceStep;

// --- Execução de Rastro (JitTrace) ---

typedef struct {
    TraceStep* steps;
    size_t length;
    size_t start_pc;
    size_t end_pc;
    int is_valid;
} JitTrace;

// O rastro gravado de um loop quente
typedef struct {
    TraceStep steps[MAX_TRACE_LENGTH];
    size_t length;
    size_t start_pc;
    int is_recording;
    JitTrace compiled_trace; // Trace pronto para execução
} TraceRecorder;

// Inicializa o sistema JIT
void jit_init(void);

// Tenta compilar uma task para código nativo
void jit_compile_task(Task* task);

// Libera recursos JIT de uma task
void jit_free_task(Task* task);

// Motor do Trace JIT
void trace_start_record(TraceRecorder* recorder, size_t pc);
void trace_record_step(TraceRecorder* recorder, OpCode op, size_t operand, ValueType type);
void trace_stop_and_compile(TraceRecorder* recorder, Task* task);

// Executor de Rastro
int jit_execute_trace(VM* vm, JitTrace* trace);
void jit_trace_destroy(JitTrace* trace);

#endif // JIT_H
