# 📝 Exemplos de Código

Coleção de exemplos práticos usando o Asteron.

## Básico

### Hello World

```asteron
function main() {
    print("Hello, World!")
    return 0
}
```

### Variáveis e Tipos

```asteron
function main() {
    let number = 42
    let text = "Asteron"
    let flag = true
    let nothing = nil
    
    print(number)
    print(text)
    print(flag)
    print(nothing)
    
    return 0
}
```

### Funções

```asteron
function add(a, b) {
    return a + b
}

function multiply(a, b) {
    return a * b
}

function main() {
    let sum = add(5, 3)
    let product = multiply(4, 7)
    
    print("Sum: " + sum)
    print("Product: " + product)
    
    return 0
}
```

## Estruturas de Controle

### Condicionais

```asteron
function check_number(n) {
    if (n > 0) {
        print("Positive")
    } else if (n < 0) {
        print("Negative")
    } else {
        print("Zero")
    }
}

function main() {
    check_number(10)
    check_number(-5)
    check_number(0)
    return 0
}
```

### Loops

```asteron
function count_to_ten() {
    let i = 1
    while (i <= 10) {
        print(i)
        i = i + 1
    }
}

function factorial(n) {
    if (n <= 1) {
        return 1
    }
    
    let result = 1
    let i = 2
    while (i <= n) {
        result = result * i
        i = i + 1
    }
    
    return result
}

function main() {
    count_to_ten()
    print("Factorial of 5: " + factorial(5))
    return 0
}
```

## Recursão

### Fibonacci

```asteron
function fibonacci(n) {
    if (n <= 1) {
        return n
    }
    return fibonacci(n - 1) + fibonacci(n - 2)
}

function main() {
    let i = 0
    while (i < 10) {
        print("fib(" + i + ") = " + fibonacci(i))
        i = i + 1
    }
    return 0
}
```

## Módulo Net

### Cliente HTTP Simples

```asteron
function fetch_url(url) {
    // Simplificado - em produção, use http_get()
    let socket = tcp_connect("example.com", 80)
    if (socket <= 0) {
        return "Error: Could not connect"
    }
    
    let request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n"
    tcp_send(socket, request)
    
    let response = tcp_recv(socket, 4096)
    tcp_close(socket)
    
    return response
}

function main() {
    let content = fetch_url("http://example.com")
    print(content)
    return 0
}
```

## Módulo FS

### Leitura de Arquivo

```asteron
function read_config() {
    if (exists("config.txt")) {
        let content = read_file("config.txt")
        print("Config: " + content)
        return content
    } else {
        print("Config file not found")
        return nil
    }
}

function main() {
    read_config()
    return 0
}
```

## Sistema Reativo

### Contador Reativo

```asteron
function main() {
    // Estado reativo
    let count = state(0)
    
    // Derivado (atualiza automaticamente)
    let doubled = derived(() => count * 2)
    
    // Efeito (executa quando count muda)
    effect(() => {
        print("Count: " + count + ", Doubled: " + doubled)
    })
    
    // Modifica estado
    count = 5
    count = 10
    count = 15
    
    return 0
}
```

### Calculadora Reativa

```asteron
function main() {
    let a = state(10)
    let b = state(20)
    
    let sum = derived(() => a + b)
    let product = derived(() => a * b)
    
    effect(() => {
        print("a = " + a + ", b = " + b)
        print("Sum = " + sum + ", Product = " + product)
    })
    
    a = 15
    b = 25
    
    return 0
}
```

## Intent-Based Scheduling

### Otimização de Latência

```asteron
@intent optimize_latency
function process_request(data) {
    // Runtime otimiza automaticamente para baixa latência
    // Pode usar SIMD, pré-alocação, etc.
    return heavy_computation(data)
}

function main() {
    let result = process_request("data")
    print(result)
    return 0
}
```

### Otimização de Throughput

```asteron
@intent optimize_throughput
function batch_process(items) {
    // Runtime otimiza para alto throughput
    // Pode paralelizar, usar cache, etc.
    let i = 0
    while (i < len(items)) {
        process_item(items[i])
        i = i + 1
    }
    return 0
}
```

## Ownership & Borrowing

### Exemplo Básico

```asteron
function create_data() {
    let data = create_object()
    // Ownership de data
    return data
}

function process_data(data) {
    // Borrow de data (read-only)
    let borrowed = borrow(data)
    return process(borrowed)
}

function main() {
    let my_data = create_data()
    let result = process_data(my_data)
    // my_data ainda pode ser usado
    return 0
}
```

## Region Memory

### Processamento de Requisição

```asteron
function handle_request() {
    // Cria região para a requisição
    let region = create_region("http_request")
    
    // Todas as alocações vão para a região
    let buffer = alloc_in_region(region, 1024)
    let data = alloc_in_region(region, 512)
    
    // Processa requisição...
    process(buffer, data)
    
    // Quando termina, libera tudo de uma vez
    destroy_region(region)
    return 0
}
```

## Graph

### Construção de Grafo

```asteron
function build_graph() {
    // Adiciona nós
    graph_add_node("client_1", "client", "")
    graph_add_node("page_1", "page", "")
    
    // Adiciona aresta
    graph_add_edge("client_1", "page_1", "visited", 1.0)
    
    // Obtém vizinhos
    let neighbors = graph_get_neighbors("client_1")
    print("Neighbors: " + neighbors)
    
    return 0
}

function main() {
    build_graph()
    return 0
}
```

## Task (Concorrência)

### Processamento Paralelo

```asteron
function process_item(item) {
    // Processa item
    return item * 2
}

function main() {
    let items = [1, 2, 3, 4, 5]
    let tasks = []
    
    // Cria tasks
    let i = 0
    while (i < len(items)) {
        let task = task_spawn(() => process_item(items[i]))
        tasks = array_push(tasks, task)
        i = i + 1
    }
    
    // Espera todas terminarem
    i = 0
    while (i < len(tasks)) {
        task_join(tasks[i])
        i = i + 1
    }
    
    return 0
}
```

## Combinando Conceitos

### Sistema Completo

```asteron
@intent optimize_throughput
function process_data(data) {
    let region = create_region("processing")
    let buffer = alloc_in_region(region, 1024)
    
    // Processa
    let result = heavy_computation(data, buffer)
    
    destroy_region(region)
    return result
}

function main() {
    // Estado reativo
    let input = state("")
    let output = derived(() => process_data(input))
    
    effect(() => {
        print("Input: " + input)
        print("Output: " + output)
    })
    
    input = "data1"
    input = "data2"
    
    return 0
}
```

## Mais Exemplos

- Veja [framework/](../framework/) para exemplos do framework de IA
- Veja [test/](../test/) para testes
- Veja [examples/](../examples/) para mais exemplos (quando disponível)

---

**Contribua**: Adicione seus próprios exemplos!

