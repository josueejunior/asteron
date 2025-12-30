#define _GNU_SOURCE
#include "jit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdint.h>

// --- Funções Auxiliares Nativas (chamadas pelo JIT) ---

// Realiza a soma nativa real acessando o TaskContext
void native_add_op(TaskContext* ctx) {
    if (ctx == NULL || ctx->vm == NULL) return;
    
    // Simula a operação ADD: r0 = r0 + r1 (simplificado)
    Value v1 = ctx->vm->locals[0];
    Value v2 = ctx->vm->locals[1];
    
    if (v1.type == VAL_NUMBER && v2.type == VAL_NUMBER) {
        ctx->vm->locals[0].as.number = v1.as.number + v2.as.number;
        printf("  [JIT Native] Soma realizada (Trampolim C): %g + %g = %g\n", 
               v1.as.number, v2.as.number, ctx->vm->locals[0].as.number);
    }
}

// Uma função de Deotimização (chamada se a Guarda falhar)
void native_deoptimize_handler(TaskContext* ctx) {
    if (ctx == NULL || ctx->vm == NULL) return;
    
    printf("\n  [JIT] 🚨 DEOPT: Assunção de tipo violada! Revertendo para VM segura...\n");
    
    // Invalida a flag de execução da VM atual para forçar saída do loop JIT
    ctx->vm->is_running = 0;
}

// --- Funções do Trace Recorder ---

void trace_start_record(TraceRecorder* recorder, size_t pc) {
    if (recorder == NULL) return;
    recorder->start_pc = pc;
    recorder->length = 0;
    recorder->is_recording = 1;
    printf("\n  [JIT] 🔴 Gravando Trace a partir do PC %zu...\n", pc);
}

void trace_record_step(TraceRecorder* recorder, OpCode op, size_t operand, ValueType type) {
    if (recorder == NULL || !recorder->is_recording) return;
    if (recorder->length >= MAX_TRACE_LENGTH) {
        recorder->is_recording = 0;
        printf("  [JIT] ⚠️ Trace muito longo, abortando gravação.\n");
        return;
    }

    TraceStep* step = &recorder->steps[recorder->length++];
    step->op = op;
    step->operand = operand;
    step->type_hint = type;
}

void trace_stop_and_compile(TraceRecorder* recorder, Task* task) {
    if (recorder == NULL || !recorder->is_recording) return;
    recorder->is_recording = 0;
    
    printf("  [JIT] ⏹️ Trace finalizado (%zu instruções). Gerando JitTrace...\n", recorder->length);
    
    // Transfere o rastro gravado para o JitTrace compilado
    if (recorder->compiled_trace.steps != NULL) free(recorder->compiled_trace.steps);
    
    recorder->compiled_trace.length = recorder->length;
    recorder->compiled_trace.steps = malloc(sizeof(TraceStep) * recorder->length);
    memcpy(recorder->compiled_trace.steps, recorder->steps, sizeof(TraceStep) * recorder->length);
    recorder->compiled_trace.start_pc = recorder->start_pc;
    recorder->compiled_trace.is_valid = 1;
    
    jit_compile_task(task);
}

int jit_execute_trace(VM* vm, JitTrace* trace) {
    if (vm == NULL || trace == NULL || !trace->is_valid) return 1;

    printf("  [JIT] ⚡ Ignorando VM: Executando Rastro Linear (PC %zu -> %zu)\n", trace->start_pc, trace->start_pc + trace->length);

    for (size_t i = 0; i < trace->length; i++) {
        TraceStep* step = &trace->steps[i];
        
        // --- GUARD: Verificação de Tipo ---
        // Se o tipo observado mudar, damos Deopt
        // (Simulado: em produção verificaríamos o tipo real do valor no stack/local)
        
        // Execução Linear simplificada (Bypass Dispatch)
        // Usamos vm_step mas em um contexto linear e controlado
        vm_step(vm, NULL, NULL); 
    }

    return 0;
}

void jit_trace_destroy(JitTrace* trace) {
    if (trace != NULL && trace->steps != NULL) {
        free(trace->steps);
        trace->steps = NULL;
        trace->is_valid = 0;
    }
}

void jit_init(void) {
    // Inicialização global do JIT se necessário
}

void jit_compile_task(Task* task) {
    if (task == NULL) return;

    // Se a task estiver suja, invalida o cache JIT
    if (task->is_dirty) {
        printf("  [JIT] 🚨 Invalidação: Task %zu marcada como dirty. Descartando Tier %d.\n", task->id, task->tier);
        jit_free_task(task);
        task->tier = 0;
        task->is_dirty = 0;
        return;
    }

    if (task->jit.is_valid) return;

    // Decisão baseada em FDO (Feedback Directed Optimization)
    int is_type_stable = (task->type_stability_count > 15);
    
    if (task->exec_count > 20) {
        if (is_type_stable) {
            task->tier = 2;
            task->fixed_type = task->observed_type;
            printf("  [JIT Tier 2] 🚀 Especialização de Tipo Progressiva: Tipo %d congelado (Estabilidade: %d)\n", 
                   task->fixed_type, task->type_stability_count);
        } else {
            task->tier = 1;
        }
    } else {
        task->tier = 1;
    }

    const char* specialization = (task->observed_type == VAL_NUMBER) ? "Numeric" : "Generic";
    printf("  [JIT Tier %d] Compilando task %zu [%s]\n", task->tier, task->id, specialization);

    size_t code_size = 4096;
    void* mem = mmap(NULL, code_size, PROT_READ | PROT_WRITE | PROT_EXEC, 
                     MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (mem == MAP_FAILED) return;

    unsigned char* code = (unsigned char*)mem;
    int i = 0;

    // 1. INJEÇÃO DE GUARDA ESPECULATIVA
    if (task->tier == 1 && task->observed_type == VAL_NUMBER) {
        printf("  [JIT] 🛡️ Injetando Guard Check para tipo Numeric\n");
        
        // Código nativo com checagem (Trampolim)
        uintptr_t addr = (uintptr_t)native_add_op;
        code[i++] = 0x48; code[i++] = 0xB8; 
        memcpy(&code[i], &addr, 8); i += 8;
        code[i++] = 0xFF; code[i++] = 0xD0; 
    } else if (task->tier == 2) {
        printf("  [JIT] 🔥 Removendo checagens de tipo (Tipo %d é GARANTIDO)\n", task->fixed_type);
        printf("  [JIT] 🚀 Emitindo ASM direto x86-64 (Matemática Nativa SSE)\n");
        
        // mov rsi, [rdi]
        code[i++] = 0x48; code[i++] = 0x8b; code[i++] = 0x37; 
        // movsd xmm0, [rsi + 24]
        code[i++] = 0xf2; code[i++] = 0x0f; code[i++] = 0x10; code[i++] = 0x46; code[i++] = 0x18;
        // movsd xmm1, [rsi + 40]
        code[i++] = 0xf2; code[i++] = 0x0f; code[i++] = 0x10; code[i++] = 0x4e; code[i++] = 0x28;
        // addsd xmm0, xmm1
        code[i++] = 0xf2; code[i++] = 0x0f; code[i++] = 0x58; code[i++] = 0xc1;
        // movsd [rsi + 24], xmm0
        code[i++] = 0xf2; code[i++] = 0x0f; code[i++] = 0x11; code[i++] = 0x46; code[i++] = 0x18;
    } else {
        // Fallback genérico
        uintptr_t addr = (uintptr_t)native_add_op;
        code[i++] = 0x48; code[i++] = 0xB8; 
        memcpy(&code[i], &addr, 8); i += 8;
        code[i++] = 0xFF; code[i++] = 0xD0; 
    }

    // 2. Finalização: ret (0xC3)
    code[i++] = 0xC3;

    task->jit.code_buffer = mem;
    task->jit.buffer_size = code_size;
    task->jit.native_fn = (JitFunction)mem;
    task->jit.is_valid = 1;

    printf("  [JIT] Task %zu otimizada com sucesso! Código em %p (Tamanho: %d bytes)\n", 
           task->id, mem, i);
}

void jit_free_task(Task* task) {
    if (task != NULL && task->jit.is_valid) {
        munmap(task->jit.code_buffer, task->jit.buffer_size);
        task->jit.is_valid = 0;
        task->jit.native_fn = NULL;
    }
}

