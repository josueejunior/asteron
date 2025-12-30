# 🔧 Troubleshooting

Guia para resolver problemas comuns no Asteron.

## Problemas de Compilação

### Erro: "gcc: command not found"

**Causa**: GCC não está instalado.

**Solução**:
```bash
# Ubuntu/Debian
sudo apt-get install build-essential

# Fedora
sudo dnf install gcc make

# macOS
xcode-select --install
```

### Erro: "undefined reference"

**Causa**: Arquivos não foram compilados ou linkados corretamente.

**Solução**:
```bash
# Limpe e recompile
rm -rf obj/
bash compile.sh
```

### Erro: "Permission denied"

**Causa**: Arquivos não têm permissão de execução.

**Solução**:
```bash
chmod +x compile.sh
chmod +x asteron
chmod +x run_wasm_server.sh
chmod +x build_wasm.sh
```

### Erro: "No such file or directory" (obj/)

**Causa**: Diretórios de objeto não foram criados.

**Solução**:
```bash
# Crie manualmente ou recompile
mkdir -p obj/core/jit obj/core/memory obj/core/runtime obj/core/scheduling obj/server
bash compile.sh
```

## Problemas de Execução

### Erro: "Erro durante execução na VM"

**Causa**: Erro em tempo de execução (ex: função não encontrada, tipo incorreto).

**Solução**:
1. Verifique se a função existe
2. Verifique os tipos dos argumentos
3. Verifique se a função foi registrada corretamente
4. Use `--debug` para mais informações:
   ```bash
   ./asteron --debug seu_arquivo.ast
   ```

### Erro: "Variável não definida"

**Causa**: Variável usada antes de ser definida.

**Solução**:
```asteron
// Ruim
print(x)  // x não definido
let x = 10

// Bom
let x = 10
print(x)
```

### Erro: "Função não encontrada"

**Causa**: Função não foi registrada ou não existe.

**Solução**:
1. Verifique se a função está definida
2. Verifique se está no escopo correto
3. Para funções built-in, verifique se o módulo está disponível

## Problemas do Servidor Wasm

### Erro: "Port already in use"

**Causa**: Porta já está em uso.

**Solução**:
```bash
# Use outra porta
./run_wasm_server.sh 3000

# Ou mate o processo na porta
lsof -ti:8080 | xargs kill
```

### Erro: "emcc não encontrado"

**Causa**: Emscripten não está instalado ou não está no PATH.

**Solução**:
```bash
# Instale Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# Volte ao diretório do projeto
cd ..
./build_wasm.sh
```

### Erro: "asteron.js não encontrado" (404)

**Causa**: Arquivo Wasm não foi compilado.

**Solução**:
```bash
# Compile para Wasm
./build_wasm.sh

# Verifique se os arquivos existem
ls -la public/asteron.*
```

### Erro: "compileCode is not defined"

**Causa**: JavaScript não carregou corretamente.

**Solução**:
1. Recarregue a página (Ctrl+F5)
2. Verifique o console do navegador
3. Verifique se o servidor está rodando

## Problemas de Memória

### Erro: "Memory leak detected"

**Causa**: Objetos não estão sendo liberados.

**Solução**:
1. Verifique se está usando `value_release` corretamente
2. Use Region-based Memory para alocações temporárias
3. Use Valgrind para detectar vazamentos:
   ```bash
   valgrind --leak-check=full ./asteron seu_arquivo.ast
   ```

### Erro: "Out of memory"

**Causa**: Muita memória sendo usada.

**Solução**:
1. Use Region-based Memory
2. Libere objetos quando não precisar mais
3. Reduza o tamanho dos dados processados

## Problemas de Performance

### Código muito lento

**Causa**: Código não está sendo otimizado pelo JIT.

**Solução**:
1. Execute o código várias vezes (JIT precisa de "warm-up")
2. Use `@intent optimize_latency` ou `optimize_throughput`
3. Verifique se o código está em um "hot path"

### JIT não está ativando

**Causa**: Código não é executado frequentemente o suficiente.

**Solução**:
1. Execute o código em um loop
2. Execute várias vezes
3. Verifique as métricas do grafo unificado

## Problemas de Rede

### Erro: "Connection refused"

**Causa**: Servidor não está rodando ou porta incorreta.

**Solução**:
```bash
# Verifique se o servidor está rodando
ps aux | grep asteron

# Verifique a porta
netstat -tuln | grep 8080
```

### Erro: "Timeout"

**Causa**: Conexão demorou muito.

**Solução**:
1. Aumente o timeout
2. Verifique a rede
3. Verifique se o servidor está respondendo

## Problemas de WebAssembly

### Erro: "Wasm module failed to load"

**Causa**: Módulo Wasm não foi compilado corretamente.

**Solução**:
1. Recompile o Wasm:
   ```bash
   ./build_wasm.sh
   ```
2. Verifique se os arquivos foram gerados
3. Verifique o console do navegador para mais detalhes

### Erro: "Function not exported"

**Causa**: Função não foi exportada no Wasm.

**Solução**:
1. Verifique `src/wasm/asteron_wasm.c`
2. Verifique se a função está em `EXPORTED_FUNCTIONS` no `build_wasm.sh`
3. Recompile

## Problemas de Debugging

### Debugger não funciona

**Causa**: Binário não foi compilado com debug.

**Solução**:
```bash
# Edite compile.sh e adicione -g
CFLAGS="-Wall -Wextra -std=c11 -g -O0 ..."

# Recompile
bash compile.sh

# Use GDB
gdb ./asteron
```

### Logs não aparecem

**Causa**: Logs podem estar desabilitados.

**Solução**:
1. Use `--debug` flag
2. Verifique se `fprintf(stderr)` está sendo usado
3. Verifique se `fflush` está sendo chamado

## Problemas Específicos do Sistema

### Linux

#### Erro: "clock_gettime not found"
**Solução**: Adicione `#define _POSIX_C_SOURCE 200809L`

#### Erro: "pthread not found"
**Solução**: Instale `libpthread-dev`

### macOS

#### Erro: "ld: library not found"
**Solução**: Instale Xcode Command Line Tools

### Windows (WSL)

#### Erro: "Address already in use"
**Solução**: WSL pode ter problemas com portas. Tente outra porta.

## Ainda com Problemas?

1. **Verifique os logs**: Procure por mensagens de erro
2. **Consulte a documentação**: Veja [docs/](docs/)
3. **Abra uma issue**: [GitHub Issues](https://github.com/seu-usuario/asteron/issues)
4. **Entre em contato**: Discord/Forum da comunidade

## Informações Úteis

### Coletar Informações para Debug

```bash
# Versão
./asteron --version

# Informações do sistema
uname -a
gcc --version

# Logs detalhados
./asteron --debug seu_arquivo.ast 2>&1 | tee debug.log
```

### Comandos Úteis

```bash
# Limpar build
rm -rf obj/ asteron

# Recompilar tudo
bash compile.sh

# Verificar arquivos
find src -name "*.c" -o -name "*.h" | wc -l

# Verificar tamanho
du -sh .
```

---

**Última atualização**: 2025-01-XX

