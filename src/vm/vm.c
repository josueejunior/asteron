#include "vm.h"
#include "../core/jit/jit.h"
#include "../utils/utils.h"
#include "../core/interpreter/interpreter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ==================== VM (Virtual Machine) ====================

VM* vm_create(BytecodeProgram* program) {
    VM* vm = (VM*)malloc(sizeof(VM));
    if (vm == NULL) return NULL;
    
    vm->program = program;
    vm->pc = 0;
    
    // Inicializa stack isolada
    vm->stack = stack_create(256);
    if (vm->stack == NULL) {
        free(vm);
        return NULL;
    }
    
    // Cria environment global isolado
    vm->env = environment_create(NULL);
    if (vm->env == NULL) {
        stack_destroy(vm->stack);
        free(vm);
        return NULL;
    }
    
    // Cria tabela de funções
    vm->function_table = function_table_create();
    if (vm->function_table == NULL) {
        environment_destroy(vm->env);
        stack_destroy(vm->stack);
        free(vm);
        return NULL;
    }
    
    vm->is_running = 1;
    
    // Inicializa tabela de locais e heat map
    for (int i = 0; i < 32; i++) {
        vm->locals[i] = value_nil();
    }
    for (int i = 0; i < 1024; i++) {
        vm->heat_map[i] = 0;
    }
    
    return vm;
}

void vm_destroy(VM* vm) {
    if (vm == NULL) return;
    
    // Libera valores nos registradores locais
    for (int i = 0; i < 32; i++) {
        value_release(vm->locals[i]);
    }
    
    if (vm->stack != NULL) stack_destroy(vm->stack);
    if (vm->env != NULL) environment_destroy(vm->env);
    if (vm->function_table != NULL) function_table_destroy(vm->function_table);
    free(vm);
}

static void push(VM* vm, Value value) {
    stack_push(vm->stack, value);
}

static Value pop(VM* vm) {
    return stack_pop(vm->stack);
}

static Value peek(VM* vm, size_t offset) {
    return stack_peek(vm->stack, offset);
}

// Variável global de gravação (simplificado para o nível atual)
static TraceRecorder g_recorder = { 
    .is_recording = 0, 
    .length = 0, 
    .start_pc = 0,
    .compiled_trace = { .steps = NULL, .is_valid = 0 } 
};

// Motor de execução de instrução única
int vm_step(VM* vm, ValueType* observed_type, int* stability_count) {
    if (vm->pc >= vm->program->instruction_count) return 0;
    
    Instruction* instr = &vm->program->instructions[vm->pc];
    BytecodeProgram* program = vm->program;
    
    // Se estamos gravando, salva este passo no trace
    if (g_recorder.is_recording) {
        ValueType type = (observed_type != NULL) ? *observed_type : VAL_NIL;
        trace_record_step(&g_recorder, instr->op, instr->operand.constant_index, type);
    }
    
    switch (instr->op) {
        case OP_LOAD_CONST: {
            if (instr->operand.constant_index >= program->constant_count) return 1;
            Constant* c = &program->constants[instr->operand.constant_index];
            Value value;
            if (c->type == CONST_NUMBER) value = value_number(c->value.number);
            else if (c->type == CONST_STRING) value = value_string(c->value.string);
            else if (c->type == CONST_BOOL) value = value_bool(c->value.boolean);
            else value = value_nil();
            push(vm, value);
            vm->pc++;
            break;
        }
        case OP_LOAD_VAR: {
            if (instr->operand.var_index >= program->variable_count) return 1;
            const char* var_name = program->variable_names[instr->operand.var_index];
            Value value = environment_get(vm->env, var_name);
            
            // --- PROGRESSIVE TYPING: Estabilização ---
            if (observed_type != NULL) {
                if (*observed_type == value.type) {
                    if (stability_count != NULL) (*stability_count)++;
                } else {
                    if (stability_count != NULL) *stability_count = 0;
                }
                *observed_type = value.type;
            }
            
            push(vm, value);
            vm->pc++;
            break;
        }
        case OP_LOAD_LOCAL: {
            if (instr->operand.var_index >= 32) return 1;
            Value value = vm->locals[instr->operand.var_index];
            
            // --- PROGRESSIVE TYPING: Estabilização ---
            if (observed_type != NULL) {
                if (*observed_type == value.type) {
                    if (stability_count != NULL) (*stability_count)++;
                } else {
                    if (stability_count != NULL) *stability_count = 0;
                }
                *observed_type = value.type;
            }
            
            value_retain(value);
            push(vm, value);
            vm->pc++;
            break;
        }
        case OP_STORE_VAR: {
            if (instr->operand.var_index >= program->variable_count) return 1;
            Value value = pop(vm);
            const char* var_name = program->variable_names[instr->operand.var_index];
            
            // --- WATCHPOINT REATIVO ---
            if (observed_type != NULL && value.type != *observed_type) {
                printf("  [Watchpoint] ⚠️ Mudança de tipo detectada em '%s'! Invalidando grafos...\n", var_name);
            }
            
            environment_set(vm->env, var_name, value);
            value_destroy(value);
            vm->pc++;
            break;
        }
        case OP_STORE_LOCAL: {
            if (instr->operand.var_index >= 32) return 1;
            Value value = pop(vm);
            value_release(vm->locals[instr->operand.var_index]);
            vm->locals[instr->operand.var_index] = value;
            vm->pc++;
            break;
        }
        case OP_ADD: {
            Value right = pop(vm);
            Value left = pop(vm);
            
            // Soma de números
            if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
                push(vm, value_number(left.as.number + right.as.number));
                value_destroy(left); value_destroy(right);
                vm->pc++;
                break;
            }
            
            // Concatenação de strings (suporta string + string, string + number, number + string)
            if (value_is_string(left) || value_is_string(right)) {
                char left_buf[128];
                char right_buf[128];
                const char* left_str = NULL;
                const char* right_str = NULL;
                
                // Converte left para string
                if (value_is_string(left)) {
                    left_str = value_to_chars(left);
                } else if (left.type == VAL_NUMBER) {
                    snprintf(left_buf, sizeof(left_buf), "%g", left.as.number);
                    left_str = left_buf;
                } else {
                    value_destroy(left); value_destroy(right);
                    return 1;
                }
                
                // Converte right para string
                if (value_is_string(right)) {
                    right_str = value_to_chars(right);
                } else if (right.type == VAL_NUMBER) {
                    snprintf(right_buf, sizeof(right_buf), "%g", right.as.number);
                    right_str = right_buf;
                } else {
                    value_destroy(left); value_destroy(right);
                    return 1;
                }
                
                // Concatena
                size_t len = strlen(left_str) + strlen(right_str);
                char* combined = (char*)malloc(len + 1);
                if (combined == NULL) {
                    value_destroy(left); value_destroy(right);
                    return 1;
                }
                strcpy(combined, left_str);
                strcat(combined, right_str);
                
                push(vm, value_string(combined));
                free(combined);
                value_destroy(left); value_destroy(right);
                vm->pc++;
                break;
            }
            
            // Tipo não suportado
            value_destroy(left); value_destroy(right);
            return 1;
        }
        case OP_SUB:
        case OP_MUL:
        case OP_DIV: {
            Value right = pop(vm);
            Value left = pop(vm);
            if (left.type != VAL_NUMBER || right.type != VAL_NUMBER) {
                value_destroy(left); value_destroy(right); return 1;
            }
            double res = 0;
            if (instr->op == OP_SUB) res = left.as.number - right.as.number;
            else if (instr->op == OP_MUL) res = left.as.number * right.as.number;
            else if (instr->op == OP_DIV) {
                if (right.as.number == 0) { value_destroy(left); value_destroy(right); return 1; }
                res = left.as.number / right.as.number;
            }
            push(vm, value_number(res));
            value_destroy(left); value_destroy(right);
            vm->pc++;
            break;
        }
        case OP_EQ:
        case OP_NE: {
            Value right = pop(vm);
            Value left = pop(vm);
            int equal = 0;
            if (left.type == right.type) {
                if (left.type == VAL_NUMBER) equal = (left.as.number == right.as.number);
                else if (value_is_string(left)) equal = (strcmp(value_to_chars(left), value_to_chars(right)) == 0);
                else if (left.type == VAL_BOOL) equal = (left.as.boolean == right.as.boolean);
                else if (left.type == VAL_NIL) equal = 1;
            }
            if (instr->op == OP_NE) equal = !equal;
            push(vm, value_bool(equal));
            value_destroy(left); value_destroy(right);
            vm->pc++;
            break;
        }
        case OP_GT:
        case OP_LT:
        case OP_GTE:
        case OP_LTE: {
            Value right = pop(vm);
            Value left = pop(vm);
            
            // Tenta converter para número se necessário
            if (left.type != VAL_NUMBER) {
                // Se for nil, trata como 0
                if (left.type == VAL_NIL) {
                    value_destroy(left);
                    left = value_number(0.0);
                }
                // Se for string, tenta converter para número (comprimento)
                else if (value_is_string(left)) {
                    double num = (double)strlen(value_to_chars(left));
                    value_destroy(left);
                    left = value_number(num);
                } else if (left.type == VAL_BOOL) {
                    double num = left.as.boolean ? 1.0 : 0.0;
                    value_destroy(left);
                    left = value_number(num);
                } else {
                    value_destroy(left); value_destroy(right);
                    return 1;
                }
            }
            
            if (right.type != VAL_NUMBER) {
                // Se for nil, trata como 0
                if (right.type == VAL_NIL) {
                    value_destroy(right);
                    right = value_number(0.0);
                }
                // Se for string, tenta converter para número (comprimento)
                else if (value_is_string(right)) {
                    double num = (double)strlen(value_to_chars(right));
                    value_destroy(right);
                    right = value_number(num);
                } else if (right.type == VAL_BOOL) {
                    double num = right.as.boolean ? 1.0 : 0.0;
                    value_destroy(right);
                    right = value_number(num);
                } else {
                    value_destroy(left); value_destroy(right);
                    return 1;
                }
            }
            
            int res = 0;
            if (instr->op == OP_GT) res = (left.as.number > right.as.number);
            else if (instr->op == OP_LT) res = (left.as.number < right.as.number);
            else if (instr->op == OP_GTE) res = (left.as.number >= right.as.number);
            else if (instr->op == OP_LTE) res = (left.as.number <= right.as.number);
            push(vm, value_bool(res));
            value_destroy(left); value_destroy(right);
            vm->pc++;
            break;
        }
        case OP_AND:
        case OP_OR: {
            Value right = pop(vm);
            Value left = pop(vm);
            int l_b = (left.type == VAL_BOOL && left.as.boolean) || (left.type == VAL_NUMBER && left.as.number != 0) || (value_is_string(left) && strlen(value_to_chars(left)) > 0);
            int r_b = (right.type == VAL_BOOL && right.as.boolean) || (right.type == VAL_NUMBER && right.as.number != 0) || (value_is_string(right) && strlen(value_to_chars(right)) > 0);
            push(vm, value_bool(instr->op == OP_AND ? (l_b && r_b) : (l_b || r_b)));
            value_destroy(left); value_destroy(right);
            vm->pc++;
            break;
        }
        case OP_NOT: {
            Value op = pop(vm);
            int is_t = (op.type == VAL_BOOL && op.as.boolean) || (op.type == VAL_NUMBER && op.as.number != 0) || (value_is_string(op) && strlen(value_to_chars(op)) > 0);
            push(vm, value_bool(!is_t));
            value_destroy(op);
            vm->pc++;
            break;
        }
        case OP_NEG: {
            Value op = pop(vm);
            if (op.type != VAL_NUMBER) { value_destroy(op); return 1; }
            push(vm, value_number(-op.as.number));
            value_destroy(op);
            vm->pc++;
            break;
        }
        case OP_PRINT: {
            Value val = peek(vm, 0);
            value_print(val); printf("\n");
            value_release(val); // Release da posse do peek
            vm->pc++;
            break;
        }
        case OP_POP: {
            Value val = pop(vm); 
            value_release(val);
            vm->pc++;
            break;
        }
        case OP_RELEASE: {
            if (instr->operand.var_index >= program->variable_count) return 1;
            const char* name = program->variable_names[instr->operand.var_index];
            
            // 1. Tenta remover do environment (se for global/escape)
            environment_remove(vm->env, name);
            
            // 2. Se for local (var_index < 32), libera o slot
            if (instr->operand.var_index < 32) {
                value_release(vm->locals[instr->operand.var_index]);
                vm->locals[instr->operand.var_index] = value_nil();
            }
            
            printf("  [Ownership] Liberando variável: %s\n", name);
            vm->pc++;
            break;
        }
        case OP_JUMP: {
            if (instr->operand.jump_target < vm->pc) {
                // Salto para trás (Loop Backedge) - Aumenta o calor
                if (instr->operand.jump_target < 1024) {
                    vm->heat_map[instr->operand.jump_target]++;
                    
                    // GATILHO JIT: Se o loop é quente e não estamos gravando, começa!
                    if (vm->heat_map[instr->operand.jump_target] == JIT_HOT_THRESHOLD && !g_recorder.is_recording) {
                        trace_start_record(&g_recorder, instr->operand.jump_target);
                    } 
                    // Se voltamos ao início do rastro durante a gravação, fecha e compila
                    else if (g_recorder.is_recording && instr->operand.jump_target == g_recorder.start_pc) {
                        printf("  [JIT] ⏹️ Ciclo de loop detectado. Finalizando rastro...\n");
                        g_recorder.is_recording = 0;
                    }
                }
            }
            vm->pc = instr->operand.jump_target; 
            break;
        }
        case OP_JUMP_IF_FALSE: {
            Value cond = pop(vm);
            int is_t = (cond.type == VAL_BOOL && cond.as.boolean) || 
                       (cond.type == VAL_NUMBER && cond.as.number != 0) || 
                       (value_is_string(cond) && strlen(value_to_chars(cond)) > 0);
            value_release(cond);
            
            // --- FEEDBACK LOOP: Branch Profiling ---
            // (Simulação de coleta de métricas de branch)

            if (!is_t) vm->pc = instr->operand.jump_target;
            else vm->pc++;
            break;
        }
        case OP_CALL: {
            vm->pc++;
            break;
        }
        case OP_RETURN: vm->pc++; break;
        case OP_HALT: 
            vm->is_running = 0;
            return -1;
        default: vm->pc++; break;
    }
    return 0;
}

int vm_execute(VM* vm) {
    if (vm == NULL || vm->program == NULL) return 1;
    while (vm->is_running && vm->pc < vm->program->instruction_count) {
        // --- JIT BYPASS: Verifica se há um rastro para este PC ---
        if (!g_recorder.is_recording && g_recorder.compiled_trace.is_valid && 
            vm->pc == g_recorder.compiled_trace.start_pc) {
            
            int res = jit_execute_trace(vm, &g_recorder.compiled_trace);
            if (res == 0) continue; // Continua após o rastro
        }

        int res = vm_step(vm, NULL, NULL);
        if (res == -1) return 0;
        if (res != 0) return res;
    }
    return 0;
}

int vm_execute_range(VM* vm, size_t start, size_t end) {
    if (vm == NULL || vm->program == NULL) return 1;
    vm->pc = start;
    vm->is_running = 1;
    while (vm->is_running && vm->pc < end && vm->pc < vm->program->instruction_count) {
        int res = vm_step(vm, NULL, NULL);
        if (res == -1) break;
        if (res != 0) return res;
    }
    return 0;
}

void vm_print_stack(VM* vm) {
    if (vm == NULL || vm->stack == NULL) {
        printf("Stack: NULL\n");
        return;
    }
    printf("=== Stack (size: %zu) ===\n", vm->stack->size);
    for (size_t i = 0; i < vm->stack->size; i++) {
        printf("[%zu] ", i);
        value_print(vm->stack->items[i]);
        printf("\n");
    }
}

// --- Implementação de Snapshot ---

VMSnapshot vm_take_snapshot(VM* vm) {
    VMSnapshot snap;
    snap.is_valid = 0;
    if (vm == NULL) return snap;

    snap.pc = vm->pc;
    for (int i = 0; i < 32; i++) {
        snap.locals[i] = vm->locals[i];
        value_retain(snap.locals[i]); // Snapshot agora possui uma referência
    }
    snap.is_valid = 1;
    printf("  [Snapshot] Estado capturado no PC: %zu\n", snap.pc);
    return snap;
}

void vm_restore_snapshot(VM* vm, VMSnapshot snap) {
    if (vm == NULL || !snap.is_valid) return;

    vm->pc = snap.pc;
    for (int i = 0; i < 32; i++) {
        value_release(vm->locals[i]); // Libera o que estava lá
        vm->locals[i] = snap.locals[i];
        value_retain(vm->locals[i]); // VM assume a posse da referência do snapshot
    }
    printf("  [Snapshot] Estado restaurado com sucesso! Retomando do PC: %zu\n", vm->pc);
}

void vm_snapshot_destroy(VMSnapshot* snap) {
    if (snap == NULL || !snap->is_valid) return;
    for (int i = 0; i < 32; i++) {
        value_release(snap->locals[i]);
    }
    snap->is_valid = 0;
}
