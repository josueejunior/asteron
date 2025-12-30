/**
 * =============================================================================
 * ASTERON MODULE-VM INTEGRATION - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "module_vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* =============================================================================
 * HELPERS
 * ============================================================================= */

static uint64_t get_time_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/* =============================================================================
 * BINDING DE MÓDULO
 * ============================================================================= */

ModuleBinding* vm_bind_module(VM* vm, Module* mod) {
    if (!vm || !mod) return NULL;
    
    ModuleBinding* binding = (ModuleBinding*)calloc(1, sizeof(ModuleBinding));
    binding->module = mod;
    
    /* Cria índice de funções */
    binding->func_count = mod->function_count;
    binding->func_index = (ModuleFunction**)calloc(mod->function_count,
                                                     sizeof(ModuleFunction*));
    binding->func_stats = calloc(mod->function_count, sizeof(*binding->func_stats));
    
    ModuleFunction* func = mod->functions;
    uint32_t i = 0;
    while (func && i < mod->function_count) {
        binding->func_index[i] = func;
        func = func->next;
        i++;
    }
    
    /* Cria índice de variáveis */
    binding->var_count = mod->variable_count;
    binding->var_index = (ModuleVariable**)calloc(mod->variable_count,
                                                    sizeof(ModuleVariable*));
    binding->var_versions = (uint32_t*)calloc(mod->variable_count, sizeof(uint32_t));
    
    ModuleVariable* var = mod->variables;
    i = 0;
    while (var && i < mod->variable_count) {
        binding->var_index[i] = var;
        binding->var_versions[i] = 1;  /* Versão inicial */
        var = var->next;
        i++;
    }
    
    /* Adiciona à lista do VM */
    /* binding->next = vm->module_bindings; */
    /* vm->module_bindings = binding; */
    
    return binding;
}

void vm_unbind_module(VM* vm, ModuleBinding* binding) {
    if (!vm || !binding) return;
    
    /* Remove da lista */
    /* ... */
    
    free(binding->func_index);
    free(binding->func_stats);
    free(binding->var_index);
    free(binding->var_versions);
    free(binding);
}

ModuleBinding* vm_get_binding(VM* vm, const char* module_name) {
    (void)vm;
    (void)module_name;
    /* TODO: Buscar na lista de bindings */
    return NULL;
}

/* =============================================================================
 * EXECUÇÃO
 * ============================================================================= */

AsteronValue vm_execute_module_call(VM* vm, uint32_t module_id,
                                     uint32_t func_id, int argc,
                                     AsteronValue* args) {
    (void)vm;
    
    /* TODO: Buscar binding e executar */
    ModuleBinding* binding = NULL; /* vm_get_binding_by_id(vm, module_id); */
    (void)module_id;
    
    if (!binding || func_id >= binding->func_count) {
        return ASTERON_NIL();
    }
    
    ModuleFunction* func = binding->func_index[func_id];
    if (!func || !func->native_fn) {
        return ASTERON_NIL();
    }
    
    /* Atualiza estatísticas */
    binding->func_stats[func_id].call_count++;
    
    uint64_t start = get_time_ns();
    
    /* Executa */
    AsteronValue result = func->native_fn(argc, args);
    
    /* Tempo */
    binding->func_stats[func_id].total_time_ns += get_time_ns() - start;
    
    /* Detecta hot path */
    if (binding->func_stats[func_id].call_count > 100) {
        binding->func_stats[func_id].is_hot = true;
        func->is_hot = true;
    }
    
    return result;
}

AsteronValue vm_read_module_var(VM* vm, uint32_t module_id, uint32_t var_id) {
    (void)vm;
    
    ModuleBinding* binding = NULL; /* vm_get_binding_by_id(vm, module_id); */
    (void)module_id;
    
    if (!binding || var_id >= binding->var_count) {
        return ASTERON_NIL();
    }
    
    return binding->var_index[var_id]->value;
}

void vm_write_module_var(VM* vm, uint32_t module_id, uint32_t var_id,
                          AsteronValue value) {
    (void)vm;
    
    ModuleBinding* binding = NULL; /* vm_get_binding_by_id(vm, module_id); */
    (void)module_id;
    
    if (!binding || var_id >= binding->var_count) return;
    
    ModuleVariable* var = binding->var_index[var_id];
    
    /* Salva valor anterior */
    var->prev_value = var->value;
    var->value = value;
    
    /* Incrementa versão SSA */
    binding->var_versions[var_id]++;
    
    /* Dispara reatividade se necessário */
    if ((var->flags & VAR_FLAG_REACTIVE) && binding->module->reactive_rt) {
        reactive_set(binding->module->reactive_rt, var->reactive_node, value);
    }
}

/* =============================================================================
 * SSA VERSIONING
 * ============================================================================= */

uint32_t vm_get_var_version(VM* vm, uint32_t module_id, uint32_t var_id) {
    (void)vm;
    
    ModuleBinding* binding = NULL;
    (void)module_id;
    
    if (!binding || var_id >= binding->var_count) return 0;
    
    return binding->var_versions[var_id];
}

uint32_t vm_inc_var_version(VM* vm, uint32_t module_id, uint32_t var_id) {
    (void)vm;
    
    ModuleBinding* binding = NULL;
    (void)module_id;
    
    if (!binding || var_id >= binding->var_count) return 0;
    
    return ++binding->var_versions[var_id];
}

bool vm_var_changed(VM* vm, uint32_t module_id, uint32_t var_id,
                     uint32_t last_version) {
    uint32_t current = vm_get_var_version(vm, module_id, var_id);
    return current != last_version;
}

/* =============================================================================
 * PARALELIZAÇÃO
 * ============================================================================= */

bool vm_can_parallelize(ModuleFunctionNode* fn1, ModuleFunctionNode* fn2) {
    if (!fn1 || !fn2) return false;
    
    /* Verifica se há conflito de escrita */
    for (uint32_t i = 0; i < fn1->write_count; i++) {
        ModuleVariable* v1 = fn1->write_deps[i].var;
        
        /* fn2 lê o que fn1 escreve? */
        for (uint32_t j = 0; j < fn2->read_count; j++) {
            if (fn2->read_deps[j].var == v1) return false;
        }
        
        /* fn2 escreve no mesmo lugar? */
        for (uint32_t j = 0; j < fn2->write_count; j++) {
            if (fn2->write_deps[j].var == v1) return false;
        }
    }
    
    /* Verifica o inverso */
    for (uint32_t i = 0; i < fn2->write_count; i++) {
        ModuleVariable* v2 = fn2->write_deps[i].var;
        
        for (uint32_t j = 0; j < fn1->read_count; j++) {
            if (fn1->read_deps[j].var == v2) return false;
        }
    }
    
    return true;
}

void vm_schedule_parallel(VM* vm, ModuleFunctionNode** funcs, uint32_t count) {
    if (!vm || !funcs || count == 0) return;
    
    /* TODO: Enviar para scheduler de threads */
    /* Por enquanto, executa sequencialmente */
    for (uint32_t i = 0; i < count; i++) {
        ModuleFunction* func = funcs[i]->func;
        if (func && func->native_fn) {
            func->native_fn(0, NULL);
        }
    }
}

ModuleFunctionNode** vm_find_independent_calls(VM* vm, uint32_t start_ip,
                                                 uint32_t end_ip,
                                                 uint32_t* count) {
    (void)vm; (void)start_ip; (void)end_ip;
    *count = 0;
    return NULL;
}

/* =============================================================================
 * GRAFO DECLARATIVO
 * ============================================================================= */

ModuleFunctionNode* vm_add_function_to_graph(VM* vm, ModuleFunction* func) {
    if (!vm || !func) return NULL;
    
    ModuleFunctionNode* node = (ModuleFunctionNode*)calloc(1, sizeof(ModuleFunctionNode));
    node->func = func;
    node->can_parallelize = (func->flags & FUNC_FLAG_PURE) != 0;
    
    /* Cria nó reativo */
    if (func->owner && func->owner->reactive_rt) {
        char name[128];
        snprintf(name, sizeof(name), "%s.%s", func->owner->name, func->name);
        node->reactive_node = reactive_effect(func->owner->reactive_rt,
                                               name, NULL, func);
        func->graph_node = node->reactive_node;
    }
    
    return node;
}

void vm_analyze_function_deps(VM* vm, ModuleFunctionNode* node) {
    if (!vm || !node || !node->func) return;
    
    /* TODO: Análise estática das instruções para descobrir:
     * - Quais variáveis a função lê
     * - Quais variáveis a função escreve
     * 
     * Por enquanto, usamos as dependências declaradas
     */
    
    ModuleFunction* func = node->func;
    
    node->read_count = func->dep_count;
    node->read_deps = calloc(func->dep_count, sizeof(*node->read_deps));
    for (uint32_t i = 0; i < func->dep_count; i++) {
        node->read_deps[i].var = func->deps[i];
        node->read_deps[i].last_version = 0;
    }
    
    node->write_count = func->output_count;
    node->write_deps = calloc(func->output_count, sizeof(*node->write_deps));
    for (uint32_t i = 0; i < func->output_count; i++) {
        node->write_deps[i].var = func->outputs[i];
        node->write_deps[i].new_version = 0;
    }
}

void vm_propagate_module_change(VM* vm, ModuleVariable* var) {
    if (!vm || !var) return;
    
    /* Propaga no grafo reativo */
    if (var->reactive_node && var->owner && var->owner->reactive_rt) {
        reactive_propagate(var->owner->reactive_rt);
    }
}

/* =============================================================================
 * JIT
 * ============================================================================= */

void* vm_jit_module_function(VM* vm, ModuleFunction* func) {
    (void)vm; (void)func;
    /* TODO: Implementar compilação JIT */
    return NULL;
}

void vm_invalidate_module_jit(VM* vm, Module* mod) {
    if (!vm || !mod) return;
    
    /* Invalida todo código JIT do módulo */
    ModuleFunction* func = mod->functions;
    while (func) {
        func->jit_code = NULL;
        func = func->next;
    }
}

bool vm_inline_module_call(VM* vm, ModuleCallSite* site) {
    (void)vm; (void)site;
    /* TODO: Decidir se deve fazer inline */
    return false;
}

/* =============================================================================
 * COMPILAÇÃO
 * ============================================================================= */

void compile_module_call(Compiler* compiler, const char* module_name,
                          const char* func_name, int argc) {
    (void)compiler; (void)module_name; (void)func_name; (void)argc;
    /* TODO: Emitir bytecode OP_MODULE_CALL */
}

void compile_module_load(Compiler* compiler, const char* module_name,
                          const char* var_name) {
    (void)compiler; (void)module_name; (void)var_name;
    /* TODO: Emitir bytecode OP_MODULE_LOAD */
}

void compile_module_store(Compiler* compiler, const char* module_name,
                           const char* var_name) {
    (void)compiler; (void)module_name; (void)var_name;
    /* TODO: Emitir bytecode OP_MODULE_STORE */
}

void compile_when_block(Compiler* compiler, const char* module_name,
                         const char* var_name, uint32_t handler_addr) {
    (void)compiler; (void)module_name; (void)var_name; (void)handler_addr;
    /* TODO: Emitir bytecode OP_WHEN */
}

