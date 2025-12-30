#!/bin/bash

# =============================================================================
# SCRIPT PARA RODAR SERVIDOR WASM
# =============================================================================
# 
# Compila e executa o servidor que fornece o sistema WebAssembly
# 
# USO:
#   ./run_wasm_server.sh [porta]
# 
# =============================================================================

PORT=${1:-8089}

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║     ASTERON WASM SERVER - SETUP                               ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# Verifica se diretórios existem
if [ ! -d "public" ]; then
    echo "Criando diretório public..."
    mkdir -p public
fi

# Verifica se arquivos Wasm existem
if [ ! -f "public/asteron.js" ] || [ ! -f "public/asteron.wasm" ]; then
    echo "⚠️  Arquivos Wasm não encontrados!"
    echo "Compilando para WebAssembly..."
    
    if [ -f "build_wasm.sh" ]; then
        chmod +x build_wasm.sh
        ./build_wasm.sh
    else
        echo "ERRO: build_wasm.sh não encontrado!"
        exit 1
    fi
fi

# Compila servidor
echo "Compilando servidor..."
gcc -Wall -Wextra -std=c11 -O2 -o wasm_server \
    src/server/wasm_server.c \
    -pthread

if [ $? -ne 0 ]; then
    echo "✗ Erro ao compilar servidor"
    exit 1
fi

echo "✓ Servidor compilado"
echo ""

# Executa servidor
echo "Iniciando servidor na porta $PORT..."
echo ""
./wasm_server $PORT

