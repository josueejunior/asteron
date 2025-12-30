# Asteron Standard Library v1.0.0

## Módulos Oficiais

### 📁 `fs` - Sistema de Arquivos

```asteron
import fs

// Leitura/Escrita
let content = fs.read("arquivo.txt")
fs.write("saida.txt", content)
fs.append("log.txt", "nova linha\n")

// Verificações
if fs.exists("config.json") {
    if fs.is_file("config.json") {
        let size = fs.size("config.json")
    }
}

// Diretórios
fs.mkdir("nova_pasta")
let files = fs.list(".")
fs.chdir("/home/user")
let cwd = fs.cwd()

// Operações
fs.copy("origem.txt", "destino.txt")
fs.move("antigo.txt", "novo.txt")
fs.remove("temp.txt")
fs.rmdir("pasta_vazia")
```

**Funções:**
| Função | Assinatura | Descrição |
|--------|------------|-----------|
| `read` | `(path: string) -> string` | Lê arquivo como texto |
| `write` | `(path: string, data: string) -> bool` | Escreve arquivo |
| `append` | `(path: string, data: string) -> bool` | Adiciona ao arquivo |
| `exists` | `(path: string) -> bool` | Verifica se existe |
| `remove` | `(path: string) -> bool` | Remove arquivo |
| `mkdir` | `(path: string) -> bool` | Cria diretório |
| `rmdir` | `(path: string) -> bool` | Remove diretório vazio |
| `list` | `(path: string) -> array` | Lista conteúdo |
| `is_file` | `(path: string) -> bool` | É arquivo? |
| `is_dir` | `(path: string) -> bool` | É diretório? |
| `size` | `(path: string) -> number` | Tamanho em bytes |
| `cwd` | `() -> string` | Diretório atual |
| `chdir` | `(path: string) -> bool` | Muda diretório |
| `copy` | `(src: string, dst: string) -> bool` | Copia arquivo |
| `move` | `(src: string, dst: string) -> bool` | Move arquivo |

---

### 🌐 `net` - Networking

```asteron
import net

// TCP Client
let sock = net.tcp_connect("example.com", 80)
net.tcp_send(sock, "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")
let response = net.tcp_recv(sock, 4096)
net.tcp_close(sock)

// TCP Server
let listener = net.tcp_listen(8080)
while true {
    let client = net.tcp_accept(listener)
    let request = net.tcp_recv(client)
    net.tcp_send(client, "HTTP/1.1 200 OK\r\n\r\nHello!")
    net.tcp_close(client)
}

// DNS
let ip = net.resolve("google.com")
let host = net.hostname()
```

**Funções TCP:**
| Função | Assinatura | Descrição |
|--------|------------|-----------|
| `tcp_connect` | `(host: string, port: number) -> handle` | Conecta a servidor |
| `tcp_listen` | `(port: number) -> handle` | Inicia servidor |
| `tcp_accept` | `(listener: handle) -> handle` | Aceita conexão |
| `tcp_send` | `(sock: handle, data: string) -> number` | Envia dados |
| `tcp_recv` | `(sock: handle, max?: number) -> string` | Recebe dados |
| `tcp_close` | `(sock: handle) -> bool` | Fecha conexão |

**DNS:**
| Função | Assinatura | Descrição |
|--------|------------|-----------|
| `resolve` | `(hostname: string) -> string` | Resolve para IP |
| `hostname` | `() -> string` | Nome do host local |

---

### ⏰ `time` - Tempo e Datas

```asteron
import time

// Timestamp
let now = time.now()          // ms desde epoch
let ns = time.now_ns()        // ns desde epoch
let mono = time.monotonic()   // clock monotônico

// Sleep
time.sleep(1000)              // 1 segundo
time.sleep_ns(500000000)      // 500ms

// Medição de performance
let start = time.instant()
// ... código a medir ...
let elapsed = time.elapsed(start)
print("Levou " + elapsed + "ms")

// Data/Hora atual
let year = time.year()
let month = time.month()
let day = time.day()
let hour = time.hour()
let min = time.minute()
let sec = time.second()
let dow = time.weekday()      // 0 = Domingo

// Formatação
let formatted = time.format(time.now(), "%Y-%m-%d %H:%M:%S")

// Timezone
let tz = time.timezone()
let dst = time.is_dst()
```

**Constantes:**
- `time.SECOND` = 1000
- `time.MINUTE` = 60000
- `time.HOUR` = 3600000
- `time.DAY` = 86400000

---

### 🔢 `math` - Matemática

```asteron
import math

// Trigonometria
let s = math.sin(math.PI / 2)
let c = math.cos(0)
let t = math.tan(math.PI / 4)
let a = math.atan2(1, 1)

// Hiperbólicas
let sh = math.sinh(1)
let ch = math.cosh(1)
let th = math.tanh(1)

// Exponenciais
let sq = math.sqrt(16)        // 4
let cb = math.cbrt(27)        // 3
let pw = math.pow(2, 10)      // 1024
let ex = math.exp(1)          // e
let lg = math.log(math.E)     // 1
let l2 = math.log2(8)         // 3
let l10 = math.log10(100)     // 2
let hy = math.hypot(3, 4)     // 5

// Arredondamento
let f = math.floor(3.7)       // 3
let cl = math.ceil(3.2)       // 4
let r = math.round(3.5)       // 4
let tr = math.trunc(-3.7)     // -3

// Utilitárias
let ab = math.abs(-5)         // 5
let mn = math.min(3, 7)       // 3
let mx = math.max(3, 7)       // 7
let sg = math.sign(-5)        // -1
let cl = math.clamp(15, 0, 10) // 10
let lr = math.lerp(0, 100, 0.5) // 50
let fm = math.fmod(5.5, 2)    // 1.5
let rnd = math.random()       // [0, 1)

// Conversões
let deg = math.deg(math.PI)   // 180
let rad = math.rad(180)       // PI

// Verificações
let nan = math.is_nan(0/0)    // true
let inf = math.is_inf(1/0)    // true
let fin = math.is_finite(42)  // true
```

**Constantes:**
- `math.PI` = 3.14159265358979323846
- `math.E` = 2.71828182845904523536
- `math.TAU` = 6.28318530717958647692
- `math.SQRT2` = 1.41421356237309504880
- `math.LN2` = 0.69314718055994530942
- `math.LN10` = 2.30258509299404568402
- `math.INF` = Infinito
- `math.NAN` = Not a Number

---

### ⚡ `task` - Concorrência

```asteron
import task

// Spawn tasks
let t1 = task.spawn(fn () {
    return compute_heavy()
})

let t2 = task.spawn(fn () {
    return fetch_data()
})

// Aguarda resultados
let result1 = task.join(t1)
let result2 = task.join(t2)

// Verificações
if !task.is_done(t1) {
    task.cancel(t1)
}

// Channels (Go-style)
let ch = task.channel(10)     // Buffer de 10

task.spawn(fn () {
    for i in 0..100 {
        task.send(ch, i)
    }
    task.close(ch)
})

while true {
    let value = task.recv(ch)
    if value == nil { break }
    print(value)
}

// Non-blocking
if task.try_send(ch, 42) {
    print("Enviado!")
}

let maybe = task.try_recv(ch)
if maybe != nil {
    print("Recebido: " + maybe)
}

// Mutex
let m = task.mutex()
task.lock(m)
// seção crítica
task.unlock(m)

// Ou com try_lock
if task.try_lock(m) {
    // ...
    task.unlock(m)
}

// Utilitários
task.yield()                  // Cede CPU
let cpus = task.num_cpus()    // Número de CPUs
task.sleep(100)               // Sleep 100ms
```

---

## Códigos de Erro

```asteron
ASTERON_OK              = 0   // Sucesso
ASTERON_ERROR_GENERIC   = 1   // Erro genérico
ASTERON_ERROR_COMPILE   = 2   // Erro de compilação
ASTERON_ERROR_RUNTIME   = 3   // Erro de runtime
ASTERON_ERROR_TYPE      = 4   // Erro de tipo
ASTERON_ERROR_MEMORY    = 5   // Sem memória
ASTERON_ERROR_IO        = 6   // Erro de I/O
ASTERON_ERROR_INVALID   = 7   // Argumento inválido
ASTERON_ERROR_NOT_FOUND = 8   // Não encontrado
ASTERON_ERROR_EXISTS    = 9   // Já existe
ASTERON_ERROR_PERM      = 10  // Permissão negada
ASTERON_ERROR_BUSY      = 11  // Recurso ocupado
ASTERON_ERROR_TIMEOUT   = 12  // Timeout
ASTERON_ERROR_NET       = 13  // Erro de rede
ASTERON_ERROR_EOF       = 14  // Fim de arquivo
```

---

## Versão

- **ABI Version:** 1.0.0 (FROZEN)
- **Stdlib Version:** 1.0.0

