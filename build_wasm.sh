#!/bin/bash

# =============================================================================
# BUILD SCRIPT PARA WEBASSEMBLY
# =============================================================================
# 
# Compila o Asteron para WebAssembly usando Emscripten
# 
# PRÉ-REQUISITOS:
#   - Emscripten SDK instalado
#   - emcc no PATH
# 
# USO:
#   ./build_wasm.sh
# 
# =============================================================================

echo "Compilando Asteron para WebAssembly..."

# Verifica se emcc está disponível
if ! command -v emcc &> /dev/null; then
    echo "ERRO: emcc não encontrado!"
    echo "Instale Emscripten SDK: https://emscripten.org/docs/getting_started/downloads.html"
    exit 1
fi

# Flags de compilação
CFLAGS="-Wall -Wextra -std=c11 -O2 -I src -I src/core -I src/graph"

# Funções exportadas
EXPORTED_FUNCTIONS='["_compile_to_ast","_compile_to_graph","_get_graph_json","_get_hot_paths","_get_node_metrics","_execute_with_metrics","_get_execution_stats","_wasm_free","_get_version","_malloc","_free"]'

# Compila para WebAssembly
emcc $CFLAGS \
    src/wasm/asteron_wasm.c \
    src/core/lexer/lexer.c \
    src/core/parser/parser.c \
    src/core/ast/ast.c \
    src/graph/unified_graph.c \
    src/graph/graph.c \
    -o public/asteron.js \
    -s EXPORTED_FUNCTIONS=$EXPORTED_FUNCTIONS \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","stringToUTF8"]' \
    -s MODULARIZE=1 \
    -s EXPORT_NAME="AsteronWasm" \
    -s USE_ES6_IMPORT_META=0

if [ $? -eq 0 ]; then
    echo "✓ Compilação concluída com sucesso!"
    echo "Arquivos gerados:"
    echo "  - public/asteron.js"
    echo "  - public/asteron.wasm"
    echo ""
    echo "Próximo passo: Integrar com frontend React/Next.js"
else
    echo "✗ Erro na compilação"
    exit 1
fi


