# 🚀 Quick Start - Asteron

Guia rápido para começar a usar o Asteron em 5 minutos.

## 1. Instalação Rápida

```bash
# Clone e compile
git clone https://github.com/seu-usuario/asteron.git
cd asteron
bash compile.sh
```

## 2. Seu Primeiro Programa

Crie um arquivo `hello.ast`:

```asteron
function main() {
    print("Hello, Asteron!")
    return 0
}
```

Execute:

```bash
./asteron hello.ast
```

## 3. Conceitos Básicos

### Variáveis

```asteron
let x = 10
let y = 20
let sum = x + y
```

### Funções

```asteron
function add(a, b) {
    return a + b
}

let result = add(5, 3)
print(result)  // 8
```

### Condicionais

```asteron
let x = 10

if (x > 5) {
    print("x é maior que 5")
} else {
    print("x é menor ou igual a 5")
}
```

### Loops

```asteron
let i = 0
while (i < 10) {
    print(i)
    i = i + 1
}
```

## 4. Exemplo Completo

```asteron
function factorial(n) {
    if (n <= 1) {
        return 1
    }
    return n * factorial(n - 1)
}

function main() {
    let n = 5
    let result = factorial(n)
    print("Factorial de " + n + " é " + result)
    return 0
}
```

## 5. Usando Módulos

```asteron
// Módulo net
let socket = tcp_connect("example.com", 80)
tcp_send(socket, "GET / HTTP/1.1\r\n\r\n")
let response = tcp_recv(socket, 1024)
print(response)
tcp_close(socket)

// Módulo fs
let content = read_file("arquivo.txt")
print(content)

// Módulo math
let result = sqrt(16)
print(result)  // 4.0
```

## 6. Sistema Reativo

```asteron
// Estado reativo
let count = state(0)

// Derivado (atualiza automaticamente)
let doubled = derived(() => count * 2)

// Efeito (executa quando count muda)
effect(() => {
    print("Count: " + count + ", Doubled: " + doubled)
})

count = 10  // Efeito executa automaticamente
```

## 7. Intent-Based Scheduling

```asteron
@intent optimize_latency
function process_data(data) {
    // Runtime otimiza automaticamente para baixa latência
    return heavy_computation(data)
}
```

## 8. Próximos Passos

- Veja [EXAMPLES.md](EXAMPLES.md) para mais exemplos
- Veja [CONCEPTS.md](CONCEPTS.md) para entender conceitos
- Veja [ARCHITECTURE.md](ARCHITECTURE.md) para arquitetura
- Veja [API.md](API.md) para referência de API

## Dicas

1. **Use o modo interativo** (quando disponível):
   ```bash
   ./asteron -i
   ```

2. **Compile com debug**:
   ```bash
   ./asteron --debug seu_arquivo.ast
   ```

3. **Veja o grafo unificado**:
   ```bash
   ./asteron --graph seu_arquivo.ast
   ```

4. **Use o servidor Wasm** para visualização:
   ```bash
   ./run_wasm_server.sh
   # Acesse http://localhost:8080
   ```

## Recursos

- **Documentação**: [docs/](../docs/)
- **Exemplos**: [examples/](../examples/)
- **Issues**: [GitHub Issues](https://github.com/seu-usuario/asteron/issues)
- **Discord/Forum**: [Link para comunidade]

## Ajuda

Precisa de ajuda?
1. Veja a [documentação completa](../README.md)
2. Procure em [Issues](https://github.com/seu-usuario/asteron/issues)
3. Abra uma nova issue
4. Entre em contato com a comunidade

Bem-vindo ao Asteron! 🚀

