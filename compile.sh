#!/bin/bash

# Script de compilação simples para Linux
# Uso: ./compile.sh

echo "Compilando Asteron..."

# Cria diretório de objetos se não existir
mkdir -p obj/lexer obj/parser obj/ast obj/utils obj/typechecker obj/optimizer obj/interpreter obj/vm obj/jit obj/core/jit obj/core/runtime obj/core/scheduling obj/core/brain obj/devtools obj/server obj/scheduler obj/graph_declarative obj/graph obj/loader obj/modules obj/modules/native obj/memory obj/core/memory obj/sys obj/reactive obj/debug obj/core/lazy obj/core/sandbox obj/core/hotreload obj/core/analytics

# Flags de compilação
CFLAGS="-Wall -Wextra -std=c11 -g -I src -I src/core -I src/graph -I src/core/vm -I src/core/lazy -I src/core/sandbox -I src/core/hotreload -I src/core/analytics"

# Compila todos os arquivos objeto (módulos em src/core/)
echo "Compilando lexer..."
gcc $CFLAGS -c src/core/lexer/lexer.c -o obj/lexer/lexer.o

echo "Compilando parser..."
gcc $CFLAGS -c src/core/parser/parser.c -o obj/parser/parser.o

echo "Compilando AST..."
gcc $CFLAGS -c src/core/ast/ast.c -o obj/ast/ast.o

echo "Compilando typechecker..."
gcc $CFLAGS -c src/core/typechecker/typechecker.c -o obj/typechecker/typechecker.o

echo "Compilando ssa..."
gcc $CFLAGS -c src/core/optimizer/ssa.c -o obj/optimizer/ssa.o

echo "Compilando escape..."
gcc $CFLAGS -c src/core/optimizer/escape.c -o obj/optimizer/escape.o

echo "Compilando inline..."
gcc $CFLAGS -c src/core/optimizer/inline.c -o obj/optimizer/inline.o

echo "Compilando reg_alloc..."
gcc $CFLAGS -c src/core/optimizer/reg_alloc.c -o obj/optimizer/reg_alloc.o

echo "Compilando interpreter..."
gcc $CFLAGS -c src/core/interpreter/interpreter.c -o obj/interpreter/interpreter.o

echo "Compilando compiler..."
gcc $CFLAGS -c src/core/vm/compiler.c -o obj/vm/compiler.o

echo "Compilando vm (core)..."
gcc $CFLAGS -c src/core/vm/vm.c -o obj/vm/vm.o

echo "Compilando jit..."
gcc $CFLAGS -c src/core/jit/jit.c -o obj/jit/jit.o

echo "Compilando tiered_jit..."
gcc $CFLAGS -c src/core/jit/tiered_jit.c -o obj/core/jit/tiered_jit.o

echo "Compilando module_vm..."
gcc $CFLAGS -c src/vm/module_vm.c -o obj/vm/module_vm.o

# Compila módulos em src/ diretamente
echo "Compilando utils..."
gcc $CFLAGS -c src/utils/utils.c -o obj/utils/utils.o

echo "Compilando scheduler..."
gcc $CFLAGS -pthread -c src/scheduler/scheduler.c -o obj/scheduler/scheduler.o

echo "Compilando graph_declarative..."
gcc $CFLAGS -c src/graph_declarative/graph_declarative.c -o obj/graph_declarative/graph_declarative.o

echo "Compilando graph..."
gcc $CFLAGS -c src/graph/graph.c -o obj/graph/graph.o

echo "Compilando unified_graph..."
gcc $CFLAGS -c src/graph/unified_graph.c -o obj/graph/unified_graph.o

echo "Compilando loader..."
gcc $CFLAGS -c src/loader/loader.c -o obj/loader/loader.o

echo "Compilando modules..."
gcc $CFLAGS -c src/modules/module.c -o obj/modules/module.o

echo "Compilando math_module..."
gcc $CFLAGS -c src/modules/native/math_module.c -o obj/modules/native/math_module.o

echo "Compilando fs_module..."
gcc $CFLAGS -c src/modules/native/fs_module.c -o obj/modules/native/fs_module.o

echo "Compilando net_module..."
gcc $CFLAGS -c src/modules/native/net_module.c -o obj/modules/native/net_module.o

echo "Compilando time_module..."
gcc $CFLAGS -c src/modules/native/time_module.c -o obj/modules/native/time_module.o

echo "Compilando task_module..."
gcc $CFLAGS -c src/modules/native/task_module.c -o obj/modules/native/task_module.o

echo "Compilando os_module..."
gcc $CFLAGS -c src/modules/native/os_module.c -o obj/modules/native/os_module.o

echo "Compilando agent_module..."
gcc $CFLAGS -c src/modules/native/agent_module.c -o obj/modules/native/agent_module.o

echo "Compilando graph_module..."
gcc $CFLAGS -c src/modules/native/graph_module.c -o obj/modules/native/graph_module.o

echo "Compilando module_interface..."
gcc $CFLAGS -c src/modules/module_interface.c -o obj/modules/module_interface.o

echo "Compilando astm..."
gcc $CFLAGS -c src/modules/astm.c -o obj/modules/astm.o

echo "Compilando cache..."
gcc $CFLAGS -c src/modules/cache.c -o obj/modules/cache.o

echo "Compilando jit_module..."
gcc $CFLAGS -c src/modules/jit_module.c -o obj/modules/jit_module.o

# Sistema de baixo nível
echo "Compilando ownership..."
gcc $CFLAGS -c src/core/memory/ownership.c -o obj/memory/ownership.o

echo "Compilando region_memory..."
gcc $CFLAGS -c src/core/memory/region_memory.c -o obj/memory/region_memory.o

echo "Compilando holographic..."
gcc $CFLAGS -c src/core/memory/holographic.c -o obj/core/memory/holographic.o

echo "Compilando self_healing..."
gcc $CFLAGS -c src/core/runtime/self_healing.c -o obj/core/runtime/self_healing.o

echo "Compilando intent_based..."
gcc $CFLAGS -c src/core/scheduling/intent_based.c -o obj/core/scheduling/intent_based.o

echo "Compilando context_brain..."
gcc $CFLAGS -c src/core/brain/context_brain.c -o obj/core/brain/context_brain.o

echo "Compilando intent_engine..."
gcc $CFLAGS -c src/core/brain/intent_engine.c -o obj/core/brain/intent_engine.o

echo "Compilando self_tuning..."
gcc $CFLAGS -c src/core/brain/self_tuning.c -o obj/core/brain/self_tuning.o

echo "Compilando visual_debugger..."
gcc $CFLAGS -c src/devtools/visual_debugger.c -o obj/devtools/visual_debugger.o

echo "Compilando runtime..."
gcc $CFLAGS -c src/sys/runtime.c -o obj/sys/runtime.o

echo "Compilando cli..."
gcc $CFLAGS -c src/sys/cli.c -o obj/sys/cli.o

# State-Driven Runtime (Reactive)
echo "Compilando reactive..."
gcc $CFLAGS -c src/reactive/reactive.c -o obj/reactive/reactive.o

echo "Compilando ws_driver..."
gcc $CFLAGS -c src/reactive/ws_driver.c -o obj/reactive/ws_driver.o

echo "Compilando distributed..."
gcc $CFLAGS -c src/reactive/distributed.c -o obj/reactive/distributed.o

# TLS/HTTPS
echo "Compilando tls_module..."
gcc $CFLAGS -c src/modules/native/tls_module.c -o obj/modules/native/tls_module.o

# Debug baseado em grafos
echo "Compilando graph_debug..."
gcc $CFLAGS -c src/debug/graph_debug.c -o obj/debug/graph_debug.o

# Novos sistemas avançados
echo "Compilando lazy..."
gcc $CFLAGS -c src/core/lazy/lazy.c -o obj/core/lazy/lazy.o

echo "Compilando snapshot..."
gcc $CFLAGS -c src/core/vm/snapshot.c -o obj/vm/snapshot.o

echo "Compilando sandbox..."
gcc $CFLAGS -c src/core/sandbox/sandbox.c -o obj/core/sandbox/sandbox.o

echo "Compilando hotreload..."
gcc $CFLAGS -c src/core/hotreload/hotreload.c -o obj/core/hotreload/hotreload.o

echo "Compilando failure_analytics..."
gcc $CFLAGS -c src/core/analytics/failure_analytics.c -o obj/core/analytics/failure_analytics.o

echo "Compilando main..."
gcc $CFLAGS -c src/main.c -o obj/main.o

echo "Compilando wasm_server..."
gcc $CFLAGS -c src/server/wasm_server.c -o obj/server/wasm_server.o

# Linka tudo
echo "Linkando executável..."
gcc $CFLAGS -pthread -o asteron \
    obj/main.o \
    obj/lexer/lexer.o \
    obj/parser/parser.o \
    obj/ast/ast.o \
    obj/utils/utils.o \
    obj/typechecker/typechecker.o \
    obj/optimizer/ssa.o \
    obj/optimizer/escape.o \
    obj/optimizer/inline.o \
    obj/optimizer/reg_alloc.o \
    obj/interpreter/interpreter.o \
    obj/vm/compiler.o \
    obj/vm/vm.o \
    obj/vm/module_vm.o \
    obj/jit/jit.o \
    obj/core/jit/tiered_jit.o \
    obj/scheduler/scheduler.o \
    obj/graph_declarative/graph_declarative.o \
    obj/graph/graph.o \
    obj/graph/unified_graph.o \
    obj/core/lazy/lazy.o \
    obj/vm/snapshot.o \
    obj/core/sandbox/sandbox.o \
    obj/core/hotreload/hotreload.o \
    obj/core/analytics/failure_analytics.o \
    obj/loader/loader.o \
    obj/modules/module.o \
    obj/modules/native/math_module.o \
    obj/modules/native/fs_module.o \
    obj/modules/native/net_module.o \
    obj/modules/native/time_module.o \
    obj/modules/native/task_module.o \
    obj/modules/native/os_module.o \
    obj/modules/native/agent_module.o \
    obj/modules/native/tls_module.o \
    obj/modules/native/graph_module.o \
    obj/modules/module_interface.o \
    obj/modules/astm.o \
    obj/modules/cache.o \
    obj/modules/jit_module.o \
    obj/memory/ownership.o \
    obj/memory/region_memory.o \
    obj/core/memory/holographic.o \
    obj/core/runtime/self_healing.o \
    obj/core/scheduling/intent_based.o \
    obj/core/brain/context_brain.o \
    obj/core/brain/intent_engine.o \
    obj/core/brain/self_tuning.o \
    obj/devtools/visual_debugger.o \
    obj/sys/runtime.o \
    obj/sys/cli.o \
    obj/reactive/reactive.o \
    obj/reactive/ws_driver.o \
    obj/reactive/distributed.o \
    obj/debug/graph_debug.o \
    -lm

if [ $? -eq 0 ]; then
    echo "✓ Compilação concluída com sucesso!"
    echo "Execute: ./asteron tests/test1.ast"
else
    echo "✗ Erro na compilação"
    exit 1
fi
