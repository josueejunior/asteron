#!/bin/bash

# Script para compilar e executar testes do Asteron

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║     ASTERON TEST RUNNER                                       ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# Cria diretório de objetos de teste
mkdir -p obj/tests

# Flags de compilação
CFLAGS="-Wall -Wextra -std=c11 -g -I src -I src/core -I src/graph -I tests"

# Compila framework de testes
echo "Compilando test framework..."
gcc $CFLAGS -c tests/test_framework.c -o obj/tests/test_framework.o

# Compila testes
echo "Compilando testes..."
gcc $CFLAGS -c tests/test_brain.c -o obj/tests/test_brain.o

# Compila objetos necessários (simplificado - apenas o necessário para testes)
echo "Compilando dependências..."
gcc $CFLAGS -c src/core/brain/context_brain.c -o obj/tests/context_brain.o
gcc $CFLAGS -c src/core/brain/intent_engine.c -o obj/tests/intent_engine.o
gcc $CFLAGS -c src/core/brain/self_tuning.c -o obj/tests/self_tuning.o
gcc $CFLAGS -c src/devtools/visual_debugger.c -o obj/tests/visual_debugger.o

# Linka e executa (simplificado - precisa de mais objetos)
echo "Linkando test runner..."
# gcc $CFLAGS -o test_runner \
#     obj/tests/test_framework.o \
#     obj/tests/test_brain.o \
#     obj/tests/context_brain.o \
#     obj/tests/intent_engine.o \
#     obj/tests/self_tuning.o \
#     obj/tests/visual_debugger.o \
#     -lm

echo ""
echo "⚠️  Test runner precisa de mais objetos para linkar completamente"
echo "   Por enquanto, os testes podem ser executados manualmente"
echo ""

