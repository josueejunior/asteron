# 📚 API Reference

Referência completa da API do Asteron.

## Core Language

### Variáveis

```asteron
let x = 10              // Número
let y = "hello"        // String
let z = true           // Boolean
let w = nil            // Nil
```

### Funções

```asteron
function nome(param1, param2) {
    // Código
    return valor
}
```

### Condicionais

```asteron
if (condição) {
    // Código
} else {
    // Código
}
```

### Loops

```asteron
while (condição) {
    // Código
}
```

## Built-in Functions

### I/O

```asteron
print(value)           // Imprime valor
read_line()            // Lê linha do stdin
```

### Strings

```asteron
len(str)               // Tamanho da string
substr(str, start, len) // Substring
index_of(str, substr)  // Índice de substring
```

### Arrays

```asteron
array_push(arr, item)  // Adiciona item
array_pop(arr)        // Remove último item
array_len(arr)        // Tamanho do array
```

## Módulo Net

### TCP

```asteron
tcp_connect(host, port)           // Conecta
tcp_listen(port)                  // Escuta
tcp_accept(listener)              // Aceita conexão
tcp_send(socket, data)            // Envia dados
tcp_recv(socket, max_bytes)       // Recebe dados
tcp_close(socket)                 // Fecha socket
```

### HTTP

```asteron
http_get(url)          // GET request
http_post(url, body)   // POST request
```

## Módulo FS

```asteron
read_file(path)        // Lê arquivo
write_file(path, data) // Escreve arquivo
exists(path)          // Verifica existência
mkdir(path)           // Cria diretório
list_dir(path)        // Lista diretório
```

## Módulo Math

```asteron
sqrt(x)               // Raiz quadrada
pow(x, y)             // Potência
sin(x)                // Seno
cos(x)                // Cosseno
abs(x)                // Valor absoluto
```

## Módulo Time

```asteron
time_now()            // Timestamp atual
sleep(seconds)         // Dorme
format_time(timestamp, format) // Formata tempo
```

## Sistema Reativo

```asteron
state(initial_value)              // Estado reativo
derived(() => expression)         // Valor derivado
effect(() => { /* código */ })    // Efeito colateral
computed(() => expression)       // Valor computado (com cache)
```

## Intent-Based Scheduling

```asteron
@intent optimize_latency
function minha_funcao() {
    // Código otimizado para latência
}

@intent optimize_throughput
function outra_funcao() {
    // Código otimizado para throughput
}
```

## Ownership & Borrowing

```asteron
let x = create_object()  // Ownership
let y = borrow(x)        // Borrow (read-only)
let z = borrow_mut(x)    // Borrow mutável (exclusivo)
```

## Region Memory

```asteron
let region = create_region("nome")
let data = alloc_in_region(region, size)
destroy_region(region)  // Libera tudo
```

## Distributed Memory

```asteron
let obj = create_object()
let remote = borrow_remote(obj, "node_id")
// Usa como se fosse local
```

## Persistent Memory

```asteron
let config = persistent_state({
    key: "value"
})
// Sobrevive a reinicializações
```

## Task (Concorrência)

```asteron
let task = task_spawn(function() {
    // Código em paralelo
})
task_join(task)  // Espera terminar
```

## Graph

```asteron
graph_add_node(id, type, metadata)
graph_add_edge(from, to, type, weight)
graph_get_neighbors(node_id)
graph_get_context(node_id)
```

## Hot Reload

```asteron
// Modifica função
function minha_funcao() {
    // Nova implementação
}
// Runtime recarrega automaticamente
```

## Annotations

```asteron
@intent optimize_latency
@inline
@no_side_effects
function minha_funcao() {
    // Código
}
```

## Error Handling

```asteron
try {
    // Código que pode falhar
} catch (error) {
    // Tratamento de erro
}
```

## Type System

```asteron
let x: number = 10
let y: string = "hello"
let z: bool = true
```

## Generics (Futuro)

```asteron
function identity<T>(x: T): T {
    return x
}
```

## Traits/Interfaces (Futuro)

```asteron
trait Printable {
    function print()
}

impl Printable for MyType {
    function print() {
        // Implementação
    }
}
```

## Pattern Matching (Futuro)

```asteron
match value {
    case 1 => print("um")
    case 2 => print("dois")
    default => print("outro")
}
```

---

**Nota**: Esta é uma referência em desenvolvimento. Algumas funcionalidades podem estar em desenvolvimento ou planejadas.

