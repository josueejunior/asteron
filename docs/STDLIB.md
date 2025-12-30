# Asteron Standard Library v1.0.0

## Official Modules

### 📁 `fs` - File System

```asteron
import fs

// Read/Write
let content = fs.read("file.txt")
fs.write("output.txt", content)
fs.append("log.txt", "new line\n")

// Checks
if fs.exists("config.json") {
    if fs.is_file("config.json") {
        let size = fs.size("config.json")
    }
}

// Directories
fs.mkdir("new_folder")
let files = fs.list(".")
fs.chdir("/home/user")
let cwd = fs.cwd()

// Operations
fs.copy("source.txt", "destination.txt")
fs.move("old.txt", "new.txt")
fs.remove("temp.txt")
fs.rmdir("empty_folder")
```

**Functions:**
| Function | Signature | Description |
|----------|-----------|-------------|
| `read` | `(path: string) -> string` | Reads file as text |
| `write` | `(path: string, data: string) -> bool` | Writes file |
| `append` | `(path: string, data: string) -> bool` | Appends to file |
| `exists` | `(path: string) -> bool` | Checks if exists |
| `remove` | `(path: string) -> bool` | Removes file |
| `mkdir` | `(path: string) -> bool` | Creates directory |
| `rmdir` | `(path: string) -> bool` | Removes empty directory |
| `list` | `(path: string) -> array` | Lists contents |
| `is_file` | `(path: string) -> bool` | Is file? |
| `is_dir` | `(path: string) -> bool` | Is directory? |
| `size` | `(path: string) -> number` | Size in bytes |
| `cwd` | `() -> string` | Current directory |
| `chdir` | `(path: string) -> bool` | Changes directory |
| `copy` | `(src: string, dst: string) -> bool` | Copies file |
| `move` | `(src: string, dst: string) -> bool` | Moves file |

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

**TCP Functions:**
| Function | Signature | Description |
|----------|-----------|-------------|
| `tcp_connect` | `(host: string, port: number) -> handle` | Connects to server |
| `tcp_listen` | `(port: number) -> handle` | Starts server |
| `tcp_accept` | `(listener: handle) -> handle` | Accepts connection |
| `tcp_send` | `(sock: handle, data: string) -> number` | Sends data |
| `tcp_recv` | `(sock: handle, max?: number) -> string` | Receives data |
| `tcp_close` | `(sock: handle) -> bool` | Closes connection |

**DNS:**
| Function | Signature | Description |
|----------|-----------|-------------|
| `resolve` | `(hostname: string) -> string` | Resolves to IP |
| `hostname` | `() -> string` | Local hostname |

---

### ⏰ `time` - Time and Dates

```asteron
import time

// Timestamp
let now = time.now()          // ms since epoch
let ns = time.now_ns()        // ns since epoch
let mono = time.monotonic()   // monotonic clock

// Sleep
time.sleep(1000)              // 1 second
time.sleep_ns(500000000)      // 500ms

// Performance measurement
let start = time.instant()
// ... code to measure ...
let elapsed = time.elapsed(start)
print("Took " + elapsed + "ms")

// Current date/time
let year = time.year()
let month = time.month()
let day = time.day()
let hour = time.hour()
let min = time.minute()
let sec = time.second()
let dow = time.weekday()      // 0 = Sunday

// Formatting
let formatted = time.format(time.now(), "%Y-%m-%d %H:%M:%S")

// Timezone
let tz = time.timezone()
let dst = time.is_dst()
```

**Constants:**
- `time.SECOND` = 1000
- `time.MINUTE` = 60000
- `time.HOUR` = 3600000
- `time.DAY` = 86400000

---

### 🔢 `math` - Mathematics

```asteron
import math

// Trigonometry
let s = math.sin(math.PI / 2)
let c = math.cos(0)
let t = math.tan(math.PI / 4)
let a = math.atan2(1, 1)

// Hyperbolic
let sh = math.sinh(1)
let ch = math.cosh(1)
let th = math.tanh(1)

// Exponentials
let sq = math.sqrt(16)        // 4
let cb = math.cbrt(27)        // 3
let pw = math.pow(2, 10)      // 1024
let ex = math.exp(1)          // e
let lg = math.log(math.E)     // 1
let l2 = math.log2(8)         // 3
let l10 = math.log10(100)     // 2
let hy = math.hypot(3, 4)     // 5

// Rounding
let f = math.floor(3.7)       // 3
let cl = math.ceil(3.2)       // 4
let r = math.round(3.5)       // 4
let tr = math.trunc(-3.7)     // -3

// Utilities
let ab = math.abs(-5)         // 5
let mn = math.min(3, 7)       // 3
let mx = math.max(3, 7)       // 7
let sg = math.sign(-5)        // -1
let cl = math.clamp(15, 0, 10) // 10
let lr = math.lerp(0, 100, 0.5) // 50
let fm = math.fmod(5.5, 2)    // 1.5
let rnd = math.random()       // [0, 1)

// Conversions
let deg = math.deg(math.PI)   // 180
let rad = math.rad(180)       // PI

// Checks
let nan = math.is_nan(0/0)    // true
let inf = math.is_inf(1/0)    // true
let fin = math.is_finite(42)  // true
```

**Constants:**
- `math.PI` = 3.14159265358979323846
- `math.E` = 2.71828182845904523536
- `math.TAU` = 6.28318530717958647692
- `math.SQRT2` = 1.41421356237309504880
- `math.LN2` = 0.69314718055994530942
- `math.LN10` = 2.30258509299404568402
- `math.INF` = Infinity
- `math.NAN` = Not a Number

---

### ⚡ `task` - Concurrency

```asteron
import task

// Spawn tasks
let t1 = task.spawn(fn () {
    return compute_heavy()
})

let t2 = task.spawn(fn () {
    return fetch_data()
})

// Wait for results
let result1 = task.join(t1)
let result2 = task.join(t2)

// Checks
if !task.is_done(t1) {
    task.cancel(t1)
}

// Channels (Go-style)
let ch = task.channel(10)     // Buffer of 10

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
    print("Sent!")
}

let maybe = task.try_recv(ch)
if maybe != nil {
    print("Received: " + maybe)
}

// Mutex
let m = task.mutex()
task.lock(m)
// critical section
task.unlock(m)

// Or with try_lock
if task.try_lock(m) {
    // ...
    task.unlock(m)
}

// Utilities
task.yield()                  // Yields CPU
let cpus = task.num_cpus()    // Number of CPUs
task.sleep(100)               // Sleep 100ms
```

---

## Error Codes

```asteron
ASTERON_OK              = 0   // Success
ASTERON_ERROR_GENERIC   = 1   // Generic error
ASTERON_ERROR_COMPILE   = 2   // Compilation error
ASTERON_ERROR_RUNTIME   = 3   // Runtime error
ASTERON_ERROR_TYPE      = 4   // Type error
ASTERON_ERROR_MEMORY    = 5   // Out of memory
ASTERON_ERROR_IO        = 6   // I/O error
ASTERON_ERROR_INVALID   = 7   // Invalid argument
ASTERON_ERROR_NOT_FOUND = 8   // Not found
ASTERON_ERROR_EXISTS    = 9   // Already exists
ASTERON_ERROR_PERM      = 10  // Permission denied
ASTERON_ERROR_BUSY      = 11  // Resource busy
ASTERON_ERROR_TIMEOUT   = 12  // Timeout
ASTERON_ERROR_NET       = 13  // Network error
ASTERON_ERROR_EOF       = 14  // End of file
```

---

## Version

- **ABI Version:** 1.0.0 (FROZEN)
- **Stdlib Version:** 1.0.0
