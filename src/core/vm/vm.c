/*
 * Asteron Runtime - (C) 2025 Asteron Contributors
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

#include "vm.h"
#include "../jit/jit.h"
#include "../../utils/utils.h"
#include "../interpreter/interpreter.h"
#include "../abi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * DEBUG CONTROL - Para produção, comente a linha abaixo
 * ============================================================================= */
/* #define ASTERON_VM_DEBUG 1 */

#ifdef ASTERON_VM_DEBUG
    #define VM_DEBUG(fmt, ...) fprintf(stderr, "[VM] " fmt "\n", ##__VA_ARGS__)
#else
    #define VM_DEBUG(fmt, ...) ((void)0)
#endif

// ==================== REGISTRO GLOBAL DE FUNÇÕES NATIVAS ====================

typedef struct {
    const char* name;
    AsteronNativeFn fn;
    int min_args;
    int max_args;
} NativeFuncEntry;

#define MAX_NATIVE_FUNCS 256
static NativeFuncEntry g_native_funcs[MAX_NATIVE_FUNCS];
static size_t g_native_count = 0;

void vm_register_native(const char* name, AsteronNativeFn fn, int min_args, int max_args) {
    if (g_native_count < MAX_NATIVE_FUNCS) {
        g_native_funcs[g_native_count].name = name;
        g_native_funcs[g_native_count].fn = fn;
        g_native_funcs[g_native_count].min_args = min_args;
        g_native_funcs[g_native_count].max_args = max_args;
        g_native_count++;
    }
}

static NativeFuncEntry* vm_find_native(const char* name) {
    for (size_t i = 0; i < g_native_count; i++) {
        if (strcmp(g_native_funcs[i].name, name) == 0) {
            return &g_native_funcs[i];
        }
    }
    return NULL;
}

/* Busca função nativa por nome (para uso pelo interpretador) */
AsteronNativeFn vm_find_native_function(const char* name) {
    NativeFuncEntry* entry = vm_find_native(name);
    return entry ? entry->fn : NULL;
}

// Registra função definida em Asteron na tabela de funções da VM
void vm_register_function(VM* vm, const char* name, Function func) {
    if (vm == NULL || vm->function_table == NULL) return;
    function_table_set(vm->function_table, name, func);
}

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
            
            const char* var_name = "?";
            if (instr->operand.var_index < program->variable_count) {
                var_name = program->variable_names[instr->operand.var_index];
            }
            
            /* DEBUG: Mostra valor carregado */
            #ifdef ASTERON_VM_DEBUG
            if (value.type == VAL_NUMBER) {
                VM_DEBUG("LOAD_LOCAL[%zu] (%s) type=NUMBER value=%.0f", 
                        instr->operand.var_index, var_name, value.as.number);
            } else if (value.type == VAL_NIL) {
                VM_DEBUG("LOAD_LOCAL[%zu] (%s) type=NIL", 
                        instr->operand.var_index, var_name);
            } else if (value.type == VAL_OBJ) {
                VM_DEBUG("LOAD_LOCAL[%zu] (%s) type=OBJ", 
                        instr->operand.var_index, var_name);
            }
            #endif
            
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
            // IMPORTANTE: Retém referência do novo valor antes de armazenar
            if (value.type == VAL_OBJ && value.as.obj != NULL) {
                value_retain(value);
            }
            vm->locals[instr->operand.var_index] = value;
            vm->pc++;
            break;
        }
        case OP_ADD: {
            if (vm->stack->size < 2) {
                return 1;
            }
            
            Value right = pop(vm);
            Value left = pop(vm);
            
            // Verifica se os valores são válidos
            if (left.type == VAL_OBJ && left.as.obj == NULL) {
                value_destroy(left); value_destroy(right);
                return 1;
            }
            if (right.type == VAL_OBJ && right.as.obj == NULL) {
                value_destroy(left); value_destroy(right);
                return 1;
            }
            
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
                    if (left_str == NULL) {
                        value_destroy(left); value_destroy(right);
                        return 1;
                    }
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
                    if (right_str == NULL) {
                        value_destroy(left); value_destroy(right);
                        return 1;
                    }
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
            
            // CORREÇÃO: Apenas libera objetos alocados, NÃO zera o valor!
            // Isso permite que números/handles continuem válidos após RELEASE.
            // A análise de liveness pode estar incorreta, então mantemos o valor.
            
            // 1. Tenta remover do environment (se for global/escape)
            environment_remove(vm->env, name);
            
            // 2. Se for local com objeto alocado, decrementa refcount mas NÃO zera
            if (instr->operand.var_index < 32) {
                Value val = vm->locals[instr->operand.var_index];
                // Apenas libera objetos complexos (strings, arrays), não números
                if (val.type == VAL_OBJ && val.as.obj != NULL) {
                    value_release(val);
                    // NÃO zera o valor - pode haver usos futuros
                    // vm->locals[instr->operand.var_index] = value_nil();
                }
                // Números e handles permanecem intactos
            }
            
            // Debug silencioso (comentar para produção)
            // printf("  [Ownership] Liberando variável: %s\n", name);
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
            size_t func_idx = instr->operand.function_index;
            const char* func_name = NULL;
            
            if (func_idx < vm->program->variable_count) {
                func_name = vm->program->variable_names[func_idx];
            }
            
            if (func_name) {
                NativeFuncEntry* native = vm_find_native(func_name);
                if (native && native->fn) {
                    /* Prepara argumentos do stack */
                    int argc = 0;
                    AsteronValue args[16];
                    
                    VM_DEBUG("CALL %s (min_args=%d, stack=%zu)", func_name, native->min_args, vm->stack->size);
                    
                    /* Pop argumentos (eles estão na ordem reversa no stack) */
                    /* CORREÇÃO v2: Pegar o máximo possível de argumentos do stack */
                    /* até max_args, desde que seja >= min_args */
                    int target_argc = (int)vm->stack->size;
                    if (target_argc > native->max_args) {
                        target_argc = native->max_args;
                    }
                    if (target_argc >= native->min_args) {
                        argc = target_argc;
                        VM_DEBUG("  args=%d (min=%d, max=%d)", argc, native->min_args, native->max_args);
                        for (int i = argc - 1; i >= 0; i--) {
                            Value v = stack_pop(vm->stack);
                            
                            /* Converte Value para AsteronValue */
                            if (v.type == VAL_NUMBER) {
                                args[i] = ASTERON_NUMBER(v.as.number);
                                value_destroy(v);
                            } else if (v.type == VAL_BOOL) {
                                args[i] = ASTERON_BOOL(v.as.boolean);
                                value_destroy(v);
                            } else if (v.type == VAL_NIL) {
                                args[i] = ASTERON_NIL();
                                value_destroy(v);
                            } else if (v.type == VAL_OBJ && v.as.obj) {
                                ObjString* str = (ObjString*)v.as.obj;
                                if (str && str->obj.type == OBJ_STRING) {
                                    /* Cria AsteronString - copia dados antes de destruir Value */
                                    size_t str_len = str->length;
                                    const char* str_chars = str->chars;
                                    
                                    // Se chars é NULL, tenta usar length ou cria string vazia
                                    if (!str_chars) {
                                        if (str_len > 0) {
                                            // Length > 0 mas chars é NULL - situação inválida
                                            args[i] = ASTERON_NIL();
                                            value_destroy(v);
                                            continue;
                                        }
                                        str_len = 0;
                                        str_chars = "";
                                    }
                                    
                                    // Copia os dados da string antes de destruir o Value
                                    char* str_copy = NULL;
                                    if (str_len > 0 && str_chars) {
                                        str_copy = (char*)malloc(str_len + 1);
                                        if (str_copy) {
                                            memcpy(str_copy, str_chars, str_len);
                                            str_copy[str_len] = '\0';
                                        }
                                    }
                                    
                                    // Agora pode destruir o Value original
                                    value_destroy(v);
                                    
                                    // Cria AsteronString com a cópia
                                    size_t total_size = sizeof(AsteronString) + str_len + 1;
                                    AsteronString* astr = (AsteronString*)malloc(total_size);
                                    if (astr) {
                                        // Inicializa header completamente
                                        memset(astr, 0, total_size);
                                        astr->header.obj_type = ASTERON_OBJ_STRING;
                                        astr->header.flags = 0;
                                        astr->header.reserved = 0;
                                        astr->header.ref_count = 1;
                                        astr->header.next = NULL;
                                        
                                        astr->length = str_len;
                                        astr->hash = 0;
                                        astr->capacity = str_len + 1;
                                        if (str_len > 0 && str_copy) {
                                            memcpy(astr->chars, str_copy, str_len);
                                            free(str_copy);
                                        } else {
                                            astr->chars[0] = '\0';
                                        }
                                        astr->chars[str_len] = '\0';
                                        args[i] = ASTERON_PTR(astr, ASTERON_VAL_STRING);
                                    } else {
                                        if (str_copy) free(str_copy);
                                        args[i] = ASTERON_NIL();
                                    }
                                } else {
                                    args[i] = ASTERON_NIL();
                                    value_destroy(v);
                                }
                            } else {
                                args[i] = ASTERON_NIL();
                                value_destroy(v);
                            }
                        }
                    } else {
                        VM_DEBUG("  AVISO: stack insuficiente");
                    }
                    
                    /* Chama função nativa */
                    AsteronValue result = native->fn(argc, args);
                    
                    /* Converte resultado para Value e empilha */
                    Value res_val;
                    if (result.type == ASTERON_VAL_NUMBER) {
                        res_val = value_number(ASTERON_AS_NUMBER(result));
                    } else if (result.type == ASTERON_VAL_BOOL) {
                        res_val = value_bool(ASTERON_AS_BOOL(result));
                    } else if (result.type == ASTERON_VAL_STRING) {
                        AsteronString* astr = ASTERON_AS_STRING(result);
                        if (astr) {
                            // Verifica se chars é válido e length é válido
                            if (astr->chars && astr->length > 0) {
                                // Cria cópia da string para o interpretador
                                char* str_copy = (char*)malloc(astr->length + 1);
                                if (str_copy) {
                                    memcpy(str_copy, astr->chars, astr->length);
                                    str_copy[astr->length] = '\0';
                                    res_val = value_string(str_copy);
                                    free(str_copy); // value_string já copiou
                                } else {
                                    res_val = value_nil();
                                }
                            } else if (astr->length == 0 || !astr->chars) {
                                // String vazia ou chars NULL
                                res_val = value_string("");
                            } else {
                                res_val = value_nil();
                            }
                        } else {
                            res_val = value_nil();
                        }
                    } else {
                        res_val = value_nil();
                    }
                    
                    stack_push(vm->stack, res_val);
                    
                    // NOTA: NÃO liberar argumentos aqui - eles são gerenciados pelo sistema de valores
                    // A liberação prematura causa double-free e corrupção de heap
                } else {
                    /* Tenta buscar função definida em Asteron */
                    Function* func = NULL;
                    if (vm->function_table) {
                        func = function_table_get(vm->function_table, func_name);
                    }
                    
                    if (func) {
                        /* Executa função definida em Asteron - avalia o corpo como expressão */
                        // Para funções simples que retornam uma expressão, podemos avaliar diretamente
                        // Para funções com múltiplos statements, precisamos executar o bloco
                        
                        VM_DEBUG("  [CALL %s] Executando função Asteron (params=%zu)", func_name, func->parameter_count);
                        
                        // Pop argumentos do stack
                        size_t arg_count = func->parameter_count;
                        Value* arg_values = NULL;
                        
                        if (arg_count > 0 && vm->stack->size >= arg_count) {
                            arg_values = (Value*)malloc(sizeof(Value) * arg_count);
                            if (arg_values) {
                                // Pop argumentos (ordem reversa)
                                for (int i = (int)arg_count - 1; i >= 0; i--) {
                                    arg_values[i] = stack_pop(vm->stack);
                                }
                            }
                        }
                        
                        if (arg_values || arg_count == 0) {
                            // Salva estado do interpretador
                            Environment* saved_interp_env = interpreter_get_environment();
                            FunctionTable* saved_interp_table = interpreter_get_function_table();
                            
                            // Salva environment atual da VM
                            Environment* saved_vm_env = vm->env;
                            
                            // Cria novo frame
                            vm->env = environment_create(vm->env);
                            
                            // Configura interpretador para usar environment e function_table da VM
                            interpreter_set_environment(vm->env);
                            interpreter_set_function_table(vm->function_table);
                            
                            // Define parâmetros no novo frame
                            for (size_t i = 0; i < arg_count; i++) {
                                environment_set(vm->env, func->parameters[i], arg_values[i]);
                            }
                            
                            if (arg_values) free(arg_values);
                            
                            // Executa corpo da função - procura por return statement
                            Value return_value = value_nil();
                            
                            if (func->body->type == AST_BLOCK) {
                                // Executa statements até encontrar return
                                int found_return = 0;
                                for (size_t i = 0; i < func->body->as.block.count && !found_return; i++) {
                                    ASTNode* stmt = func->body->as.block.statements[i];
                                    
                                    if (stmt->type == AST_RETURN) {
                                        if (stmt->as.return_stmt.value) {
                                            return_value = interpreter_evaluate_expression(stmt->as.return_stmt.value);
                                            // Retém referência antes de destruir environment
                                            if (return_value.type == VAL_OBJ && return_value.as.obj != NULL) {
                                                value_retain(return_value);
                                            }
                                        }
                                        found_return = 1;
                                    } else if (stmt->type == AST_IF_STATEMENT) {
                                        // Para IF statements, usamos o interpretador
                                        // e verificamos se houve retorno
                                        interpreter_execute_statement(stmt);
                                        if (interpreter_has_returned()) {
                                            return_value = interpreter_get_return_value();
                                            // IMPORTANTE: Retém referência ANTES de limpar o flag
                                            // O valor já foi retido no interpretador, mas precisamos
                                            // retê-lo novamente aqui porque estamos obtendo uma cópia
                                            if (return_value.type == VAL_OBJ && return_value.as.obj != NULL) {
                                                value_retain(return_value);
                                            }
                                            found_return = 1;
                                            interpreter_clear_return();
                                        }
                                    } else if (stmt->type == AST_ASSIGNMENT) {
                                        Value val = interpreter_evaluate_expression(stmt->as.assignment.value);
                                        environment_set(vm->env, stmt->as.assignment.name, val);
                                        value_destroy(val);
                                    } else if (stmt->type == AST_VARIABLE_DECLARATION) {
                                        Value val = interpreter_evaluate_expression(stmt->as.variable_decl.value);
                                        environment_set(vm->env, stmt->as.variable_decl.name, val);
                                        value_destroy(val);
                                    } else {
                                        // Outros statements - executa via interpretador
                                        interpreter_execute_statement(stmt);
                                        if (interpreter_has_returned()) {
                                            return_value = interpreter_get_return_value();
                                            // IMPORTANTE: Retém referência ANTES de limpar o flag
                                            if (return_value.type == VAL_OBJ && return_value.as.obj != NULL) {
                                                value_retain(return_value);
                                            }
                                            found_return = 1;
                                            interpreter_clear_return();
                                        }
                                    }
                                }
                                
                                // Verifica se houve retorno mesmo após o loop (pode ter sido setado por um RETURN dentro de um IF)
                                if (!found_return && interpreter_has_returned()) {
                                    return_value = interpreter_get_return_value();
                                    // IMPORTANTE: Retém referência ANTES de limpar o flag
                                    if (return_value.type == VAL_OBJ && return_value.as.obj != NULL) {
                                        value_retain(return_value);
                                    }
                                    interpreter_clear_return();
                                }
                            } else if (func->body->type == AST_RETURN) {
                                if (func->body->as.return_stmt.value) {
                                    return_value = interpreter_evaluate_expression(func->body->as.return_stmt.value);
                                    // Retém referência antes de destruir environment
                                    if (return_value.type == VAL_OBJ && return_value.as.obj != NULL) {
                                        value_retain(return_value);
                                    }
                                }
                            } else {
                                // Body é uma expressão única - avalia
                                return_value = interpreter_evaluate_expression(func->body);
                                // Retém referência antes de destruir environment
                                if (return_value.type == VAL_OBJ && return_value.as.obj != NULL) {
                                    value_retain(return_value);
                                }
                            }
                            
                            // Restaura estado do interpretador
                            interpreter_set_environment(saved_interp_env);
                            interpreter_set_function_table(saved_interp_table);
                            
                            // Restaura environment da VM
                            Environment* old_env = vm->env;
                            vm->env = saved_vm_env;
                            environment_destroy(old_env);
                            
                            // Empilha resultado
                            // Garante que o valor está válido antes de empilhar
                            if (return_value.type == VAL_OBJ) {
                                if (return_value.as.obj == NULL) {
                                    return_value = value_nil();
                                } else if (return_value.as.obj->type == OBJ_STRING) {
                                    ObjString* str = (ObjString*)return_value.as.obj;
                                    if (str->chars == NULL) {
                                        value_release(return_value);
                                        return_value = value_nil();
                                    }
                                }
                            }
                            
                            stack_push(vm->stack, return_value);
                        } else {
                            /* Erro ao alocar argumentos - empilha nil */
                            stack_push(vm->stack, value_nil());
                        }
                    } else {
                        /* Função não encontrada - empilha nil */
                        stack_push(vm->stack, value_nil());
                    }
                }
            } else {
                stack_push(vm->stack, value_nil());
            }
            
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
