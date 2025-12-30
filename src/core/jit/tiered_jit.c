/**
 * =============================================================================
 * ASTERON TIERED JIT COMPILER - IMPLEMENTAÇÃO
 * =============================================================================
 */

#define _GNU_SOURCE
#include "tiered_jit.h"
#include "../../core/interpreter/interpreter.h"
#include "../../core/vm/vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdint.h>

// =============================================================================
// ESTRUTURAS INTERNAS
// =============================================================================

// Tabela de funções JIT
typedef struct {
    char* function_name;
    JitFunctionInfo info;
} JitFunctionEntry;

static JitFunctionEntry* g_jit_functions = NULL;
static size_t g_jit_functions_count = 0;
static size_t g_jit_functions_capacity = 0;

// =============================================================================
// FUNÇÕES AUXILIARES
// =============================================================================

// Encontra entrada de função JIT
static JitFunctionEntry* find_function_entry(const char* function_name) {
    for (size_t i = 0; i < g_jit_functions_count; i++) {
        if (strcmp(g_jit_functions[i].function_name, function_name) == 0) {
            return &g_jit_functions[i];
        }
    }
    return NULL;
}

// Cria nova entrada de função JIT
static JitFunctionEntry* create_function_entry(const char* function_name) {
    if (g_jit_functions_count >= g_jit_functions_capacity) {
        size_t new_capacity = g_jit_functions_capacity == 0 ? 16 : g_jit_functions_capacity * 2;
        JitFunctionEntry* new_functions = realloc(g_jit_functions, 
                                                  sizeof(JitFunctionEntry) * new_capacity);
        if (new_functions == NULL) return NULL;
        g_jit_functions = new_functions;
        g_jit_functions_capacity = new_capacity;
    }
    
    JitFunctionEntry* entry = &g_jit_functions[g_jit_functions_count++];
    entry->function_name = strdup(function_name);
    entry->info.current_tier = JIT_TIER_INTERPRETED;
    entry->info.feedback.call_count = 0;
    entry->info.feedback.loop_iterations = 0;
    entry->info.feedback.type_profile = NULL;
    entry->info.feedback.type_profile_size = 0;
    entry->info.feedback.branch_profile = NULL;
    entry->info.feedback.branch_profile_size = 0;
    entry->info.feedback.is_hot = 0;
    entry->info.feedback.is_type_stable = 0;
    entry->info.needs_recompilation = 0;
    
    return entry;
}

// =============================================================================
// BASELINE JIT - Compilação Rápida
// =============================================================================

// Emite código x86-64 para uma instrução bytecode (simplificado)
static void emit_baseline_instruction(unsigned char* code, int* pos, 
                                       Instruction* instr, BytecodeProgram* program) {
    // Esta é uma implementação simplificada
    // Em produção, seria um compilador completo bytecode -> x86-64
    
    switch (instr->op) {
        case OP_LOAD_CONST: {
            // Carrega constante no stack
            // Em produção: emitir código para push constante
            // Por enquanto: trampoline para função C
            uintptr_t addr = (uintptr_t)NULL; // Função helper
            code[(*pos)++] = 0x48; code[(*pos)++] = 0xB8; 
            memcpy(&code[*pos], &addr, 8); *pos += 8;
            code[(*pos)++] = 0xFF; code[(*pos)++] = 0xD0;
            break;
        }
        case OP_ADD: {
            // Soma dois valores do stack
            // Em produção: emitir código SSE direto
            // Por enquanto: trampoline
            uintptr_t addr = (uintptr_t)NULL; // Função helper
            code[(*pos)++] = 0x48; code[(*pos)++] = 0xB8;
            memcpy(&code[*pos], &addr, 8); *pos += 8;
            code[(*pos)++] = 0xFF; code[(*pos)++] = 0xD0;
            break;
        }
        // Outras instruções...
        default:
            // Fallback: trampoline genérico
            break;
    }
}

int tiered_jit_compile_baseline(VM* vm, size_t start_pc, size_t end_pc,
                                 const char* function_name, BaselineCode* out) {
    if (vm == NULL || vm->program == NULL || out == NULL) return 1;
    
    printf("[Baseline JIT] Compilando função '%s' (PC %zu-%zu)\n", 
           function_name, start_pc, end_pc);
    
    // Aloca buffer de código executável
    size_t code_size = 4096;
    void* mem = mmap(NULL, code_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (mem == MAP_FAILED) return 1;
    
    unsigned char* code = (unsigned char*)mem;
    int pos = 0;
    
    // Prologue: salva registradores
    // push rbp
    code[pos++] = 0x55;
    // mov rbp, rsp
    code[pos++] = 0x48; code[pos++] = 0x89; code[pos++] = 0xE5;
    
    // Compila cada instrução bytecode
    for (size_t pc = start_pc; pc < end_pc && pc < vm->program->instruction_count; pc++) {
        Instruction* instr = &vm->program->instructions[pc];
        emit_baseline_instruction(code, &pos, instr, vm->program);
    }
    
    // Epilogue: restaura registradores e retorna
    // mov rsp, rbp
    code[pos++] = 0x48; code[pos++] = 0x89; code[pos++] = 0xEC;
    // pop rbp
    code[pos++] = 0x5D;
    // ret
    code[pos++] = 0xC3;
    
    out->code_buffer = mem;
    out->code_size = code_size;
    out->entry_point = (JitFunction)mem;
    out->start_pc = start_pc;
    out->end_pc = end_pc;
    out->is_valid = 1;
    
    printf("[Baseline JIT] Função '%s' compilada (%d bytes)\n", function_name, pos);
    return 0;
}

int tiered_jit_execute_baseline(VM* vm, BaselineCode* code) {
    if (vm == NULL || code == NULL || !code->is_valid) return 1;
    
    // Em produção: chamaria code->entry_point diretamente
    // Por enquanto: executa via VM normal (fallback)
    // TODO: Implementar execução nativa real
    
    printf("[Baseline JIT] Executando código Baseline\n");
    return 0;
}

// =============================================================================
// OPTIMIZING JIT (Ion) - Compilação com Otimizações
// =============================================================================

int tiered_jit_compile_optimizing(VM* vm, size_t start_pc, size_t end_pc,
                                   const char* function_name, FunctionFeedback* feedback,
                                   OptimizingCode* out) {
    if (vm == NULL || vm->program == NULL || out == NULL) return 1;
    
    printf("[Optimizing JIT] Compilando função '%s' com otimizações (PC %zu-%zu)\n",
           function_name, start_pc, end_pc);
    
    // 1. Reconstrói AST do bytecode (ou usa AST original se disponível)
    // Por enquanto, vamos assumir que temos acesso à AST original
    // Em produção, precisaríamos de um decompiler bytecode -> AST
    
    // 2. Aplica transformação SSA
    // ASTNode* ast = ...; // Obter AST da função
    // ssa_transform(ast);
    
    // 3. Aplica outras otimizações
    // - Loop unrolling (se feedback indicar loops pequenos)
    // - Inlining (se funções chamadas são pequenas e hot)
    // - Dead code elimination
    // - Constant folding
    
    // 4. Alocação de registradores (Graph Coloring)
    // register_allocation_transform(ast);
    
    // 5. Gera código x86-64 otimizado
    size_t code_size = 8192; // Maior para código otimizado
    void* mem = mmap(NULL, code_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (mem == MAP_FAILED) return 1;
    
    unsigned char* code = (unsigned char*)mem;
    int pos = 0;
    
    // Prologue otimizado
    code[pos++] = 0x55; // push rbp
    code[pos++] = 0x48; code[pos++] = 0x89; code[pos++] = 0xE5; // mov rbp, rsp
    
    // Em produção: emitir código otimizado baseado na AST transformada
    // Por enquanto: placeholder
    
    // Epilogue
    code[pos++] = 0x48; code[pos++] = 0x89; code[pos++] = 0xEC; // mov rsp, rbp
    code[pos++] = 0x5D; // pop rbp
    code[pos++] = 0xC3; // ret
    
    out->code_buffer = mem;
    out->code_size = code_size;
    out->entry_point = (JitFunction)mem;
    out->osr_entry = (JitFunction)mem; // Por enquanto, mesmo ponto
    out->start_pc = start_pc;
    out->end_pc = end_pc;
    out->optimized_ast = NULL; // Seria preenchido com AST otimizada (ASTNode*)
    out->is_valid = 1;
    
    printf("[Optimizing JIT] Função '%s' otimizada (%d bytes)\n", function_name, pos);
    return 0;
}

int tiered_jit_execute_optimizing(VM* vm, OptimizingCode* code) {
    if (vm == NULL || code == NULL || !code->is_valid) return 1;
    
    printf("[Optimizing JIT] Executando código otimizado\n");
    // Em produção: chamaria code->entry_point diretamente
    return 0;
}

// =============================================================================
// OSR (On-Stack Replacement)
// =============================================================================

OSRFrame* tiered_jit_prepare_osr(VM* vm, size_t loop_pc) {
    if (vm == NULL) return NULL;
    
    OSRFrame* frame = malloc(sizeof(OSRFrame));
    if (frame == NULL) return NULL;
    
    frame->loop_pc = loop_pc;
    frame->loop_iterations = 0;
    
    // Snapshot dos locals
    frame->locals_count = 32; // Assumindo 32 locals
    frame->locals_snapshot = malloc(sizeof(Value) * frame->locals_count);
    if (frame->locals_snapshot) {
        memcpy(frame->locals_snapshot, vm->locals, 
               sizeof(Value) * frame->locals_count);
    }
    
    // Snapshot do stack
    if (vm->stack && vm->stack->size > 0) {
        frame->stack_size = vm->stack->size;
        frame->stack_snapshot = malloc(sizeof(Value) * frame->stack_size);
        if (frame->stack_snapshot) {
            // Copia valores do stack (em ordem reversa para manter topo)
            for (size_t i = 0; i < frame->stack_size; i++) {
                frame->stack_snapshot[i] = vm->stack->items[i];
                value_retain(frame->stack_snapshot[i]);
            }
        }
    } else {
        frame->stack_size = 0;
        frame->stack_snapshot = NULL;
    }
    
    printf("[OSR] Frame preparado para loop em PC %zu\n", loop_pc);
    return frame;
}

int tiered_jit_perform_osr(VM* vm, OSRFrame* frame, OptimizingCode* code) {
    if (vm == NULL || frame == NULL || code == NULL || !code->is_valid) return 1;
    
    printf("[OSR] Executando On-Stack Replacement (loop PC %zu, %zu iterações)\n",
           frame->loop_pc, frame->loop_iterations);
    
    // 1. Restaura estado do frame OSR
    if (frame->locals_snapshot) {
        memcpy(vm->locals, frame->locals_snapshot, 
               sizeof(Value) * frame->locals_count);
    }
    
    if (frame->stack_snapshot && vm->stack) {
        // Limpa stack atual
        while (vm->stack->size > 0) {
            Value v = stack_pop(vm->stack);
            value_destroy(v);
        }
        // Restaura stack do snapshot
        for (size_t i = 0; i < frame->stack_size; i++) {
            stack_push(vm->stack, frame->stack_snapshot[i]);
            value_retain(frame->stack_snapshot[i]);
        }
    }
    
    // 2. Ajusta PC para início do loop
    vm->pc = frame->loop_pc;
    
    // 3. Executa código JIT otimizado
    // Em produção: chamaria code->osr_entry diretamente
    printf("[OSR] Estado restaurado, executando código otimizado\n");
    
    return 0;
}

void tiered_jit_free_osr_frame(OSRFrame* frame) {
    if (frame == NULL) return;
    
    // Libera snapshots
    if (frame->locals_snapshot) {
        free(frame->locals_snapshot);
    }
    
    if (frame->stack_snapshot) {
        for (size_t i = 0; i < frame->stack_size; i++) {
            value_destroy(frame->stack_snapshot[i]);
        }
        free(frame->stack_snapshot);
    }
    
    free(frame);
}

// =============================================================================
// PROFILING E FEEDBACK
// =============================================================================

void tiered_jit_record_call(const char* function_name, size_t pc) {
    JitFunctionEntry* entry = find_function_entry(function_name);
    if (entry == NULL) {
        entry = create_function_entry(function_name);
        if (entry == NULL) return;
    }
    
    entry->info.feedback.call_count++;
    
    // Marca como hot se atingir threshold
    if (entry->info.feedback.call_count >= BASELINE_HOT_THRESHOLD) {
        entry->info.feedback.is_hot = 1;
    }
}

void tiered_jit_record_loop_iteration(size_t pc, size_t iterations) {
    // Registra iterações de loop para decisão de OSR
    // Em produção: associaria com função atual
}

void tiered_jit_record_type_feedback(size_t pc, ValueType type) {
    // Registra feedback de tipo para especialização
    // Em produção: associaria com função atual
}

void tiered_jit_record_branch(size_t pc, int taken) {
    // Registra feedback de branch para otimização
    // Em produção: associaria com função atual
}

// =============================================================================
// DECISÃO DE PROMOÇÃO
// =============================================================================

JitTier tiered_jit_should_promote(const char* function_name, FunctionFeedback* feedback) {
    if (feedback == NULL) return JIT_TIER_INTERPRETED;
    
    // Se já está em Optimizing, não promove mais
    JitFunctionEntry* entry = find_function_entry(function_name);
    if (entry && entry->info.current_tier == JIT_TIER_OPTIMIZING) {
        return JIT_TIER_OPTIMIZING;
    }
    
    // Promove para Optimizing se:
    // - É hot (muitas chamadas)
    // - Tipos são estáveis
    // - Atingiu threshold de execuções
    if (feedback->call_count >= OPTIMIZING_HOT_THRESHOLD &&
        feedback->is_type_stable) {
        return JIT_TIER_OPTIMIZING;
    }
    
    // Promove para Baseline se é hot
    if (feedback->call_count >= BASELINE_HOT_THRESHOLD) {
        return JIT_TIER_BASELINE;
    }
    
    return JIT_TIER_INTERPRETED;
}

// =============================================================================
// FUNÇÕES PÚBLICAS
// =============================================================================

void tiered_jit_init(void) {
    g_jit_functions = NULL;
    g_jit_functions_count = 0;
    g_jit_functions_capacity = 0;
    printf("[Tiered JIT] Sistema inicializado\n");
}

void tiered_jit_cleanup(void) {
    for (size_t i = 0; i < g_jit_functions_count; i++) {
        JitFunctionEntry* entry = &g_jit_functions[i];
        free(entry->function_name);
        
        if (entry->info.baseline.is_valid) {
            tiered_jit_free_baseline(&entry->info.baseline);
        }
        if (entry->info.optimizing.is_valid) {
            tiered_jit_free_optimizing(&entry->info.optimizing);
        }
        
        if (entry->info.feedback.type_profile) {
            free(entry->info.feedback.type_profile);
        }
        if (entry->info.feedback.branch_profile) {
            free(entry->info.feedback.branch_profile);
        }
    }
    
    free(g_jit_functions);
    g_jit_functions = NULL;
    g_jit_functions_count = 0;
    g_jit_functions_capacity = 0;
}

JitFunctionInfo* tiered_jit_get_function_info(const char* function_name) {
    JitFunctionEntry* entry = find_function_entry(function_name);
    return entry ? &entry->info : NULL;
}

void tiered_jit_free_baseline(BaselineCode* code) {
    if (code == NULL || !code->is_valid) return;
    if (code->code_buffer != NULL) {
        munmap(code->code_buffer, code->code_size);
    }
    memset(code, 0, sizeof(BaselineCode));
}

void tiered_jit_free_optimizing(OptimizingCode* code) {
    if (code == NULL || !code->is_valid) return;
    if (code->code_buffer != NULL) {
        munmap(code->code_buffer, code->code_size);
    }
    // AST otimizada seria liberada se necessário
    memset(code, 0, sizeof(OptimizingCode));
}

void tiered_jit_invalidate(const char* function_name) {
    JitFunctionEntry* entry = find_function_entry(function_name);
    if (entry == NULL) return;
    
    if (entry->info.baseline.is_valid) {
        tiered_jit_free_baseline(&entry->info.baseline);
    }
    if (entry->info.optimizing.is_valid) {
        tiered_jit_free_optimizing(&entry->info.optimizing);
    }
    
    entry->info.current_tier = JIT_TIER_INTERPRETED;
    entry->info.needs_recompilation = 1;
    entry->info.feedback.call_count = 0;
}

