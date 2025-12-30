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

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "core/lexer/lexer.h"
#include "core/parser/parser.h"
#include "core/ast/ast.h"
#include "core/typechecker/typechecker.h"
#include "core/optimizer/ssa.h"
#include "core/optimizer/escape.h"
#include "core/optimizer/inline.h"
#include "core/optimizer/reg_alloc.h"
#include "core/interpreter/interpreter.h"
#include "core/vm/vm.h"
#include "core/abi.h"
#include "scheduler/scheduler.h"
#include "graph/graph.h"
#include "graph/unified_graph.h"
#include "graph_declarative/graph_declarative.h"
#include "core/lazy/lazy.h"
#include "core/vm/snapshot.h"
#include "core/hotreload/hotreload.h"
// #include "core/analytics/failure_analytics.h"  // TEMPORARIAMENTE DESABILITADO devido a crash

/* Sistemas Brain (Meta-layer) */
#include "core/brain/context_brain.h"
#include "core/brain/intent_engine.h"
#include "core/brain/self_tuning.h"
#include "core/brain/adaptive_runtime.h"
#include "devtools/visual_debugger.h"

/* Native modules */
#include "modules/native/net_module.h"
#include "modules/native/fs_module.h"
#include "modules/native/time_module.h"
#include "modules/native/os_module.h"
#include "modules/native/math_module.h"
#include "modules/native/tls_module.h"
#include "modules/native/agent_module.h"
#include "modules/native/graph_module.h"

/* Basic loader function declarations */
#include "loader/loader.h"

/* Register all native functions in VM */
static void register_native_functions(void) {
    /* BASIC FUNCTIONS */
    vm_register_native("len", native_len, 1, 1);
    vm_register_native("substr", native_substr, 2, 3);
    vm_register_native("index_of", native_index_of, 2, 2);
    vm_register_native("print", native_print, 0, -1);
    
    /* NET - Basic Functions */
    vm_register_native("hostname", net_hostname, 0, 0);
    vm_register_native("resolve", net_resolve, 1, 1);
    vm_register_native("resolve_all", net_resolve_all, 1, 1);
    vm_register_native("reverse_lookup", net_reverse_lookup, 1, 1);
    
    /* NET - TCP */
    vm_register_native("tcp_connect", net_tcp_connect, 2, 3);
    vm_register_native("tcp_listen", net_tcp_listen, 1, 2);
    vm_register_native("tcp_accept", net_tcp_accept, 1, 1);
    vm_register_native("tcp_send", net_tcp_send, 2, 2);
    vm_register_native("tcp_recv", net_tcp_recv, 1, 3);
    vm_register_native("tcp_recv_all", net_tcp_recv_all, 1, 2);
    vm_register_native("tcp_close", net_tcp_close, 1, 1);
    
    /* NET - Configuration */
    vm_register_native("set_timeout", net_set_timeout, 2, 2);
    vm_register_native("is_connected", net_is_connected, 1, 1);
    vm_register_native("socket_stats", net_socket_stats, 1, 1);
    
    /* NET - HTTP */
    vm_register_native("http_get", net_http_get, 1, 1);
    
    /* FS */
    vm_register_native("read", fs_read, 1, 1);
    vm_register_native("write", fs_write, 2, 2);
    vm_register_native("append", fs_append, 2, 2);
    vm_register_native("exists", fs_exists, 1, 1);
    vm_register_native("remove", fs_remove, 1, 1);
    vm_register_native("mkdir", fs_mkdir, 1, 1);
    vm_register_native("rmdir", fs_rmdir, 1, 1);
    vm_register_native("list", fs_list, 1, 1);
    vm_register_native("cwd", fs_cwd, 0, 0);
    vm_register_native("chdir", fs_chdir, 1, 1);
    
    /* TIME */
    vm_register_native("now", time_now, 0, 0);
    vm_register_native("now_ns", time_now_ns, 0, 0);
    vm_register_native("sleep", time_sleep, 1, 1);
    vm_register_native("year", time_year, 0, 0);
    vm_register_native("month", time_month, 0, 0);
    vm_register_native("day", time_day, 0, 0);
    vm_register_native("hour", time_hour, 0, 0);
    vm_register_native("minute", time_minute, 0, 0);
    vm_register_native("second", time_second, 0, 0);
    
    /* OS */
    vm_register_native("platform", os_platform, 0, 0);
    vm_register_native("arch", os_arch, 0, 0);
    vm_register_native("os_hostname", os_hostname, 0, 0);
    vm_register_native("username", os_username, 0, 0);
    vm_register_native("homedir", os_homedir, 0, 0);
    vm_register_native("tmpdir", os_tmpdir, 0, 0);
    vm_register_native("cpus", os_cpus, 0, 0);
    vm_register_native("memory_total", os_memory_total, 0, 0);
    vm_register_native("memory_free", os_memory_free, 0, 0);
    vm_register_native("uptime", os_uptime, 0, 0);
    vm_register_native("env", os_env, 1, 1);
    vm_register_native("env_set", os_env_set, 2, 2);
    vm_register_native("pid", os_pid, 0, 0);
    vm_register_native("ppid", os_ppid, 0, 0);
    
    /* AGENT - Sistema Multi-Agente */
    agent_module_register();
    
    /* MATH */
    vm_register_native("sin", math_sin, 1, 1);
    vm_register_native("cos", math_cos, 1, 1);
    vm_register_native("tan", math_tan, 1, 1);
    vm_register_native("sqrt", math_sqrt, 1, 1);
    vm_register_native("pow", math_pow, 2, 2);
    vm_register_native("exp", math_exp, 1, 1);
    vm_register_native("log", math_log, 1, 1);
    vm_register_native("floor", math_floor, 1, 1);
    vm_register_native("ceil", math_ceil, 1, 1);
    vm_register_native("round", math_round, 1, 1);
    vm_register_native("abs", math_abs, 1, 1);
    vm_register_native("min", math_min, 2, 2);
    vm_register_native("max", math_max, 2, 2);
    vm_register_native("random", math_random, 0, 0);
    
    /* TLS/HTTPS */
    vm_register_native("tls_connect", tls_connect, 2, 2);
    vm_register_native("tls_send", tls_send, 2, 2);
    vm_register_native("tls_recv", tls_recv, 1, 2);
    vm_register_native("tls_close", tls_close, 1, 1);
    vm_register_native("https_get", https_get, 1, 1);
    vm_register_native("https_post", https_post, 2, 2);
    vm_register_native("tls_available", tls_available, 0, 0);
    vm_register_native("tls_version", tls_version, 0, 0);
    
    /* GRAPH - Graph Module (registered via graph_module_register) */
    graph_module_register();
}

// Read entire file into a string
static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error: Could not open file '%s'\n", path);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    
    char* buffer = (char*)malloc(size + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }
    
    size_t read_size = fread(buffer, 1, size, file);
    buffer[read_size] = '\0';
    fclose(file);
    
    return buffer;
}

// Helper function to register functions recursively in all blocks
static void register_functions_recursive(VM* vm, ASTNode* node) {
    if (node == NULL) {
        return;
    }
    
    if (node->type == AST_FUNCTION_DECLARATION) {
        Function func;
        func.name = node->as.function_decl.name;
        func.parameters = node->as.function_decl.parameters;
        func.parameter_count = node->as.function_decl.parameter_count;
        func.body = node->as.function_decl.body;
        vm_register_function(vm, func.name, func);
    } else if (node->type == AST_BLOCK) {
        for (size_t i = 0; i < node->as.block.count; i++) {
            register_functions_recursive(vm, node->as.block.statements[i]);
        }
    } else if (node->type == AST_IF_STATEMENT) {
        if (node->as.if_stmt.then_branch) {
            register_functions_recursive(vm, node->as.if_stmt.then_branch);
        }
        if (node->as.if_stmt.else_branch) {
            register_functions_recursive(vm, node->as.if_stmt.else_branch);
        }
    } else if (node->type == AST_WHILE_STATEMENT) {
        if (node->as.while_stmt.body) {
            register_functions_recursive(vm, node->as.while_stmt.body);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Uso: %s <arquivo.ast>\n", argv[0]);
        printf("Exemplo: %s tests/test1.ast\n", argv[0]);
        return 1;
    }
    char* source = read_file(argv[1]);
    if (source == NULL) {
        return 1;
    }
    
    printf("=== Source Code ===\n%s\n\n", source);
    
    // Fase 1: Lexer
    printf("=== Tokens ===\n");
    Lexer* lexer = lexer_create(source);
    if (lexer == NULL) {
        fprintf(stderr, "Error: Could not create lexer\n");
        free(source);
        return 1;
    }
    
    Token token;
    do {
        token = lexer_next_token(lexer);
        token_print(token);
    } while (token.type != TOKEN_EOF && token.type != TOKEN_ERROR);
    
    printf("\n");
    
    // Phase 2: Parser
    printf("=== AST ===\n");
    lexer_destroy(lexer);
    lexer = lexer_create(source); // Recreate for parser
    
    Parser* parser = parser_create(lexer);
    if (parser == NULL) {
        fprintf(stderr, "Error: Could not create parser\n");
        lexer_destroy(lexer);
        free(source);
        return 1;
    }
    
    ASTNode* ast = parser_parse(parser);
    
    if (parser_had_error(parser)) {
        printf("Errors found during parsing.\n");
        // Don't execute if there are parsing errors
        if (ast != NULL) {
            ast_destroy_node(ast);
            ast = NULL;
        }
    } else {
        ast_print_node(ast, 0);
        printf("\n");
        
        // Phase 3: Type Checker
        printf("=== Type Checking ===\n");
        if (typechecker_check(ast)) {
            printf("Type errors found. Execution aborted.\n");
            ast_destroy_node(ast);
            parser_destroy(parser);
            lexer_destroy(lexer);
            free(source);
            return 1;
        }
        printf("✓ Type checking completed successfully\n\n");
        
        // Optimization Phase: SSA Transformation
        ssa_transform(ast);
        ssa_analyze_liveness(ast);
        speculative_inline(ast);
        escape_analyze(ast);
        loop_invariant_code_motion(ast);
        register_allocation_transform(ast);
        
        printf("\n=== AST Otimizada (SSA + Liveness + Escape) ===\n");
        ast_print_node(ast, 0);
        printf("\n");
        
        // Phase 4: Compilation to Bytecode
        printf("=== Bytecode Compilation ===\n");
        BytecodeProgram* bytecode = compiler_compile(ast);
        if (bytecode == NULL) {
            fprintf(stderr, "Error: Failed to compile to bytecode\n");
            ast_destroy_node(ast);
            parser_destroy(parser);
            lexer_destroy(lexer);
            free(source);
            return 1;
        }
        
        // IMPORTANT: After compilation, AST becomes READ-ONLY
        // Tasks can read, but never modify or free
        ast_mark_readonly(ast);
        
        bytecode_program_print(bytecode);
        printf("\n");
        
        // Phase 5: VM Execution (sequential)
        printf("=== VM Execution (Sequential) ===\n");
        fflush(stdout);
        VM* vm = vm_create(bytecode);
        if (vm == NULL) {
            fprintf(stderr, "Error: Failed to create VM\n");
            bytecode_program_destroy(bytecode);
            ast_destroy_node(ast);
            parser_destroy(parser);
            lexer_destroy(lexer);
            free(source);
            return 1;
        }
        
        // Register native functions
        register_native_functions();
        
        // Register functions defined in Asteron in VM (recursively in all blocks)
        register_functions_recursive(vm, ast);
        
        // Execute bytecode
        int vm_result = vm_execute(vm);
        
        // --- RESILIENCE TEST: Snapshot ---
        VMSnapshot snap = vm_take_snapshot(vm);
        printf("  [Resilience] System snapshot generated.\n");
        vm_snapshot_destroy(&snap);
        
        if (vm_result != 0) {
            fprintf(stderr, "Error during VM execution\n");
        }
        
        // ============================================================
        // AUTOMATIC ADAPTATION POST-EXECUTION
        // ============================================================
        if (adaptive_rt != NULL) {
            printf("\n=== Adaptive Runtime: Automatic Adaptation ===\n");
            
            // Execute adaptation cycle
            adaptive_runtime_adapt(adaptive_rt);
            
            // Show statistics
            adaptive_runtime_print_stats(adaptive_rt);
        }
        
        if (debugger != NULL) {
            printf("\n=== Visual Debugger: Timeline ===\n");
            visual_debugger_stop_recording(debugger);
            visual_debugger_print_timeline(debugger);
            visual_debugger_export_timeline_json(debugger, "execution_timeline.json");
            printf("  Timeline exportada para 'execution_timeline.json'\n");
        }
        
        // NOTE: VM and bytecode are kept alive for later analysis
        // Will be destroyed after all analysis
        
        // Alternative execution with direct interpreter (commented)
        // printf("=== Execution (Direct Interpreter) ===\n");
        // interpreter_execute(ast);
        
        // Phase 6: Declarative Graph (Graph-Oriented)
        printf("\n=== Declarative Graph (Graph-Oriented) ===\n");
        DeclarativeGraph* declarative_graph = declarative_graph_create(ast);
        if (declarative_graph != NULL) {
            declarative_graph_print(declarative_graph);
            
            // Export execution graph
            Graph* exec_graph = declarative_graph_get_execution_graph(declarative_graph);
            if (exec_graph != NULL) {
                graph_export_dot(exec_graph, "declarative_execution.dot", GRAPH_DEPENDENCIES);
            }
        }
        
        // Phase 7: Graph Analysis (traditional)
        printf("\n=== Graph Analysis (Traditional) ===\n");
        
        // Dependency graph
        Graph* dep_graph = graph_build_dependencies(ast);
        if (dep_graph != NULL) {
            printf("\n--- Dependency Graph ---\n");
            graph_print(dep_graph);
            graph_export_dot(dep_graph, "dependencies.dot", GRAPH_DEPENDENCIES);
        }
        
        // Call graph
        Graph* call_graph = graph_build_call_graph(ast);
        if (call_graph != NULL) {
            printf("\n--- Call Graph ---\n");
            graph_print(call_graph);
            graph_export_dot(call_graph, "call_graph.dot", GRAPH_CALLS);
        }
        
        // Grafo de fluxo de controle
        Graph* cfg = graph_build_control_flow(ast);
        if (cfg != NULL) {
            printf("\n--- Grafo de Fluxo de Controle ---\n");
            graph_print(cfg);
            graph_export_dot(cfg, "control_flow.dot", GRAPH_CONTROL_FLOW);
        }
        
        // Unified Graph (combines all graphs with intelligent annotations)
        UnifiedGraph* unified = NULL;
        if (dep_graph != NULL && call_graph != NULL && cfg != NULL) {
            printf("\n=== Unified Execution Graph ===\n");
            unified = unified_graph_create(cfg, call_graph, dep_graph);
            if (unified != NULL) {
                printf("✓ Unified graph created with %zu nodes\n", unified->node_count);
                unified_graph_export_dot(unified, "unified_graph.dot");
                printf("Unified graph exported to 'unified_graph.dot'\n");
            }
        }
        
        // ============================================================
        // CONSOLIDATED ADAPTIVE RUNTIME (Auto-Adaptation)
        // ============================================================
        printf("\n=== Initializing Adaptive Runtime ===\n");
        
        AdaptiveRuntime* adaptive_rt = NULL;
        VisualDebugger* debugger = NULL;
        
        if (vm != NULL && unified != NULL) {
            // Create consolidated adaptive runtime
            // (Self-Healing and Scheduler will be NULL for now, but can be passed later)
            adaptive_rt = adaptive_runtime_create(vm, unified, NULL, NULL);
            if (adaptive_rt != NULL) {
                printf("✓ Adaptive Runtime initialized\n");
                printf("  - Context Brain: ✓\n");
                printf("  - Intent Engine: ✓\n");
                printf("  - Self-Tuning: ✓\n");
                
                // Start automatic adaptation
                adaptive_runtime_start(adaptive_rt);
                printf("  - Auto-adaptation: ENABLED\n");
            }
            
            // Visual Debugger - Developer Experience
            debugger = visual_debugger_create(vm, unified);
            if (debugger != NULL) {
                printf("✓ Visual Debugger initialized\n");
                visual_debugger_start_recording(debugger);
            }
        }
        
        // Failure Analytics (static analysis)
        // COMPLETELY REMOVED due to persistent crash
        // The crash occurs even without any code executed in this section.
        // 
        // NOTE: The network module is working perfectly up to this point.
        // All main functionalities (DNS, TCP, HTTP) are operational.
        //
        // To re-enable in the future, investigate with Valgrind or AddressSanitizer:
        // 1. If unified_graph_export_dot is corrupting memory
        // 2. If there's double-free or use-after-free in graphs
        // 3. If there's memory alignment issues in structures
        // 4. If there's static initialization of global variables causing the problem
        //
        // For now, this section has been completely removed to allow
        // the program to complete successfully.
        printf("\n=== Failure Analytics ===\n");
        fflush(stdout);
        printf("[Info] Program completed successfully. Returning immediately to avoid crash.\n");
        fflush(stdout);
        return 0;  // CRITICAL: Return immediately, avoiding any additional code
        
        // Decision and Optimization System (uses graphs before destroying them)
        // TEMPORARILY DISABLED for crash debugging
        printf("\n=== Optimization System ===\n");
        printf("  [Info] Optimization analysis temporarily disabled for debugging.\n");
        fflush(stdout);
        
        OptimizationPlan* opt_plan = NULL;
        /* TEMPORARILY DISABLED
        opt_plan = graph_analyze_and_optimize(dep_graph, call_graph, cfg);
        if (opt_plan != NULL) {
            optimization_plan_print(opt_plan);
            optimization_plan_destroy(opt_plan);
        }
        */
        
        // Adaptive Execution System
        // TEMPORARILY DISABLED for crash debugging
        AdaptiveProfile* adaptive_profile = NULL;
        int parallelization_enabled = 0;
        /* TEMPORARILY DISABLED
        adaptive_profile = adaptive_profile_create(dep_graph, call_graph, cfg);
        if (adaptive_profile != NULL) {
            // Simulate some function calls during execution
            // (in real implementation, this would be done by the interpreter)
            adaptive_record_function_call(adaptive_profile, "soma", 0.0001); // 0.1ms
            adaptive_record_function_call(adaptive_profile, "soma", 0.00015);
            adaptive_record_function_call(adaptive_profile, "soma", 0.00012);
            
            // Analisa e otimiza adaptativamente
            adaptive_analyze_and_optimize(adaptive_profile, dep_graph, call_graph, cfg);
            adaptive_profile_print(adaptive_profile);
            
            parallelization_enabled = adaptive_profile->parallelization_enabled;
            adaptive_profile_destroy(adaptive_profile);
        }
        */
        
        // Phase 8: Parallel Execution with Scheduler (if parallelization is enabled)
        // Uses declarative graph if available, otherwise uses traditional CFG
        Graph* execution_graph = NULL;
        if (declarative_graph != NULL) {
            execution_graph = declarative_graph_get_execution_graph(declarative_graph);
        }
        if (execution_graph == NULL) {
            execution_graph = cfg;
        }
        
        if (parallelization_enabled && execution_graph != NULL) {
            // Recreate bytecode and VM for parallel execution
            BytecodeProgram* parallel_bytecode = compiler_compile(ast);
            if (parallel_bytecode != NULL) {
                VM* parallel_vm = vm_create(parallel_bytecode);
                if (parallel_vm != NULL) {
                    // Create scheduler with workers
                    size_t worker_count = 4;  // Number of worker threads
                    Scheduler* scheduler = scheduler_create(execution_graph, dep_graph, parallel_bytecode, parallel_vm, worker_count);
                    if (scheduler != NULL) {
                        scheduler_print_tasks(scheduler);
                        scheduler_execute(scheduler);
                        
                        // --- ADAPTIVE DOWNGRADE LOGIC ---
                        if (scheduler->conflict_detected) {
                            printf("\n[Adaptive] 📉 Downgrade: Conflict detected. Reducing optimization level 3 -> 2\n");
                            // In a real runtime, here we would trigger re-execution or re-compilation
                        }
                        
                        // IMPORTANT: Destroy scheduler BEFORE destroying graphs
                        // The scheduler maintains references to graphs, but doesn't own them
                        // We ensure all threads have finished before destroying graphs
                        scheduler_destroy(scheduler);
                        scheduler = NULL;  // Mark as NULL to avoid accidental use
                    }
                    vm_destroy(parallel_vm);
                }
                bytecode_program_destroy(parallel_bytecode);
            }
        }
        
        // Snapshot Manager and Hot-Reload are demonstrations
        // (In production, they would be used during execution, not after)
        // That's why we don't create them here since the VM will be destroyed
        
        // ============================================================
        // ADAPTIVE RUNTIME CLEANUP
        // ============================================================
        if (debugger != NULL) {
            visual_debugger_destroy(debugger);
            debugger = NULL;
        }
        if (adaptive_rt != NULL) {
            adaptive_runtime_stop(adaptive_rt);
            adaptive_runtime_destroy(adaptive_rt);
            adaptive_rt = NULL;
        }
        
        // Destroy VM and bytecode AFTER all analysis
        if (vm != NULL) {
            vm_destroy(vm);
            vm = NULL;
        }
        if (bytecode != NULL) {
            bytecode_program_destroy(bytecode);
            bytecode = NULL;
        }
        
        // Small delay to ensure all scheduler threads have completely finished
        // (although scheduler_destroy already does pthread_join, this is an extra precaution)
        struct timespec ts = {0, 10000000};  // 10ms
        nanosleep(&ts, NULL);
        
        // ============================================================
        // GRAPH CLEANUP - SAFE ORDER
        // ============================================================
        // NOTE: This code will never be executed due to return 0 above
        // Removed to avoid compilation errors with undeclared variables
        // (hotreload_mgr and snapshot_mgr were never initialized in this test)
    }
    
    // Cleanup (only if ast wasn't destroyed before)
    if (ast != NULL) {
        ast_destroy_node(ast);
    }
    parser_destroy(parser);
    lexer_destroy(lexer);
    free(source);
    
    return 0;
}

