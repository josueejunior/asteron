# 📦 Guia de Instalação

## Pré-requisitos

### Linux

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential git

# Fedora
sudo dnf install gcc make git

# Arch
sudo pacman -S base-devel git
```

### macOS

```bash
# Instale Xcode Command Line Tools
xcode-select --install

# Ou use Homebrew
brew install gcc make git
```

### Windows

1. Instale [WSL2](https://docs.microsoft.com/wsl/install) ou [MSYS2](https://www.msys2.org/)
2. Siga as instruções do Linux dentro do WSL/MSYS2

## Instalação

### 1. Clone o Repositório

```bash
git clone https://github.com/seu-usuario/asteron.git
cd asteron
```

### 2. Compile o Projeto

```bash
bash compile.sh
```

Isso irá:
- Compilar todos os arquivos fonte
- Linkar o executável
- Criar o binário `asteron`

### 3. Teste a Instalação

```bash
./asteron --version
```

Ou execute um arquivo de teste:

```bash
echo 'print("Hello, Asteron!")' > test.ast
./asteron test.ast
```

## Instalação Opcional: WebAssembly

Para compilar para WebAssembly:

### 1. Instale Emscripten

```bash
# Clone Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

# Instale e ative
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

### 2. Compile para Wasm

```bash
cd asteron
chmod +x build_wasm.sh
./build_wasm.sh
```

Isso gera:
- `public/asteron.js`
- `public/asteron.wasm`

### 3. Execute o Servidor Wasm

```bash
chmod +x run_wasm_server.sh
./run_wasm_server.sh
```

Acesse: `http://localhost:8080`

## Instalação de Desenvolvimento

Para desenvolvimento, você pode querer:

### 1. Compilar com Debug

```bash
# Edite compile.sh e adicione -g -O0
CFLAGS="-Wall -Wextra -std=c11 -g -O0 -I src ..."
bash compile.sh
```

### 2. Usar GDB/LLDB

```bash
# GDB
gdb ./asteron

# LLDB (macOS)
lldb ./asteron
```

### 3. Valgrind (Linux)

```bash
sudo apt-get install valgrind
valgrind --leak-check=full ./asteron test.ast
```

## Troubleshooting

### Erro: "gcc: command not found"

**Solução**: Instale build-essential:
```bash
sudo apt-get install build-essential
```

### Erro: "Permission denied"

**Solução**: Dê permissão de execução:
```bash
chmod +x compile.sh
chmod +x asteron
```

### Erro: "undefined reference"

**Solução**: Verifique se todos os arquivos foram compilados:
```bash
bash compile.sh 2>&1 | grep error
```

### Erro: "Port already in use" (Wasm Server)

**Solução**: Use outra porta:
```bash
./run_wasm_server.sh 3000
```

### Erro: "emcc not found" (Wasm)

**Solução**: Instale e ative Emscripten:
```bash
source emsdk/emsdk_env.sh
```

## Verificação da Instalação

Execute o script de verificação:

```bash
# Crie um arquivo de teste
cat > test_install.ast << 'EOF'
function main() {
    print("Asteron instalado com sucesso!")
    return 0
}
EOF

# Execute
./asteron test_install.ast
```

Se você ver "Asteron instalado com sucesso!", a instalação está correta!

## Próximos Passos

- Veja [QUICKSTART.md](QUICKSTART.md) para começar
- Veja [EXAMPLES.md](EXAMPLES.md) para exemplos
- Veja [ARCHITECTURE.md](ARCHITECTURE.md) para entender a arquitetura

## Suporte

Se tiver problemas:
1. Verifique [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
2. Abra uma [issue](https://github.com/seu-usuario/asteron/issues)
3. Entre em contato com os mantenedores

