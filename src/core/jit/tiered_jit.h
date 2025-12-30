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

/**
 * =============================================================================
 * ASTERON TIERED JIT COMPILER v2.0
 * =============================================================================
 * 
 * Sistema de compilação JIT de duas camadas:
 * 
 * 1. BASELINE JIT (Tier 1)
 *    - Compilação rápida sem otimizações pesadas
 *    - Ativado na primeira execução de funções hot
 *    - Gera código de máquina direto do bytecode
 * 
 * 2. OPTIMIZING JIT (Tier 2 - Ion)
 *    - Usa SSA e Graph Coloring apenas em hot functions
 *    - Ativado após Baseline ter coletado feedback
 *    - Otimizações pesadas: inlining, loop unrolling, etc.
 * 
 * 3. OSR (On-Stack Replacement)
 *    - Troca código interpretado por JIT no meio da execução
 *    - Permite otimizar loops em execução
 * 
 * =============================================================================
 */

#ifndef TIERED_JIT_H
#define TIERED_JIT_H

#include "../../scheduler/scheduler.h"
#include "../../core/vm/vm.h"
#include "../../core/interpreter/interpreter.h"
#include <stddef.h>
#include <stdint.h>

// Forward declarations
struct ASTNode;
typedef struct ASTNode ASTNode;

// =============================================================================
// CONFIGURAÇÕES
// =============================================================================

#define BASELINE_HOT_THRESHOLD 3        // Execuções para ativar Baseline
#define OPTIMIZING_HOT_THRESHOLD 50     // Execuções para ativar Optimizing
#define OSR_LOOP_ITERATIONS 10           // Iterações de loop para OSR

// =============================================================================
// ESTRUTURAS
// =============================================================================

// Estado de compilação de uma função
typedef enum {
    JIT_TIER_INTERPRETED = 0,   // Ainda interpretado
    JIT_TIER_BASELINE = 1,      // Compilado com Baseline JIT
    JIT_TIER_OPTIMIZING = 2     // Compilado com Optimizing JIT
} JitTier;

// Informações de feedback para decisão de otimização
typedef struct {
    size_t call_count;              // Quantas vezes foi chamada
    size_t loop_iterations;         // Total de iterações de loops
    ValueType* type_profile;        // Perfil de tipos observados
    size_t type_profile_size;
    int* branch_profile;            // Perfil de branches (taken/not taken)
    size_t branch_profile_size;
    int is_hot;                     // Flag: função é hot?
    int is_type_stable;             // Flag: tipos são estáveis?
} FunctionFeedback;

// Código compilado Baseline
typedef struct {
    void* code_buffer;              // Buffer de código nativo
    size_t code_size;               // Tamanho do código
    JitFunction entry_point;        // Ponto de entrada
    size_t start_pc;                // PC inicial no bytecode
    size_t end_pc;                  // PC final no bytecode
    int is_valid;                   // Flag de validade
} BaselineCode;

// Código compilado Optimizing (Ion)
typedef struct {
    void* code_buffer;              // Buffer de código nativo otimizado
    size_t code_size;               // Tamanho do código
    JitFunction entry_point;        // Ponto de entrada
    JitFunction osr_entry;          // Ponto de entrada OSR (para loops)
    size_t start_pc;                // PC inicial
    size_t end_pc;                  // PC final
    ASTNode* optimized_ast;         // AST otimizada (SSA)
    int is_valid;                   // Flag de validade
} OptimizingCode;

// Informações de compilação JIT de uma função
typedef struct {
    JitTier current_tier;           // Tier atual
    FunctionFeedback feedback;      // Feedback coletado
    BaselineCode baseline;          // Código Baseline (se compilado)
    OptimizingCode optimizing;      // Código Optimizing (se compilado)
    int needs_recompilation;        // Flag: precisa recompilar?
} JitFunctionInfo;

// OSR Frame - informações para On-Stack Replacement
typedef struct {
    size_t loop_pc;                 // PC do início do loop
    size_t loop_iterations;         // Iterações já executadas
    Value* locals_snapshot;         // Snapshot dos locals
    size_t locals_count;
    Value* stack_snapshot;          // Snapshot do stack
    size_t stack_size;
} OSRFrame;

// =============================================================================
// FUNÇÕES PÚBLICAS
// =============================================================================

// Inicializa o sistema Tiered JIT
void tiered_jit_init(void);

// Limpa recursos do Tiered JIT
void tiered_jit_cleanup(void);

// Registra uma chamada de função (para profiling)
void tiered_jit_record_call(const char* function_name, size_t pc);

// Registra uma iteração de loop (para OSR)
void tiered_jit_record_loop_iteration(size_t pc, size_t iterations);

// Registra feedback de tipo
void tiered_jit_record_type_feedback(size_t pc, ValueType type);

// Registra feedback de branch
void tiered_jit_record_branch(size_t pc, int taken);

// Compila uma função com Baseline JIT
int tiered_jit_compile_baseline(VM* vm, size_t start_pc, size_t end_pc, 
                                 const char* function_name, BaselineCode* out);

// Compila uma função com Optimizing JIT (Ion)
int tiered_jit_compile_optimizing(VM* vm, size_t start_pc, size_t end_pc,
                                   const char* function_name, FunctionFeedback* feedback,
                                   OptimizingCode* out);

// Executa código Baseline
int tiered_jit_execute_baseline(VM* vm, BaselineCode* code);

// Executa código Optimizing
int tiered_jit_execute_optimizing(VM* vm, OptimizingCode* code);

// Prepara OSR frame para transição
OSRFrame* tiered_jit_prepare_osr(VM* vm, size_t loop_pc);

// Executa OSR (troca interpretado -> JIT no meio da execução)
int tiered_jit_perform_osr(VM* vm, OSRFrame* frame, OptimizingCode* code);

// Libera frame OSR
void tiered_jit_free_osr_frame(OSRFrame* frame);

// Verifica se uma função deve ser promovida para próximo tier
JitTier tiered_jit_should_promote(const char* function_name, FunctionFeedback* feedback);

// Obtém informações JIT de uma função
JitFunctionInfo* tiered_jit_get_function_info(const char* function_name);

// Libera código Baseline
void tiered_jit_free_baseline(BaselineCode* code);

// Libera código Optimizing
void tiered_jit_free_optimizing(OptimizingCode* code);

// Invalida código JIT (para hot reload)
void tiered_jit_invalidate(const char* function_name);

#endif // TIERED_JIT_H

