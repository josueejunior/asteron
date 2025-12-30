# 🚀 Quick Start - Asteron

Quick guide to get started with Asteron in 5 minutes.

## 1. Quick Installation

```bash
# Clone and compile
git clone https://github.com/your-user/asteron.git
cd asteron
bash compile.sh
```

## 2. Your First Program

Create a file `hello.ast`:

```asteron
function main() {
    print("Hello, Asteron!")
    return 0
}
```

Run:

```bash
./asteron hello.ast
```

## 3. Basic Concepts

### Variables

```asteron
let x = 10
let y = 20
let sum = x + y
```

### Functions

```asteron
function add(a, b) {
    return a + b
}

let result = add(5, 3)
print(result)  // 8
```

### Conditionals

```asteron
let x = 10

if (x > 5) {
    print("x is greater than 5")
} else {
    print("x is less than or equal to 5")
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

## 4. Complete Example

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
    print("Factorial of " + n + " is " + result)
    return 0
}
```

## 5. Using Modules

```asteron
// net module
let socket = tcp_connect("example.com", 80)
tcp_send(socket, "GET / HTTP/1.1\r\n\r\n")
let response = tcp_recv(socket, 1024)
print(response)
tcp_close(socket)

// fs module
let content = read_file("file.txt")
print(content)

// math module
let result = sqrt(16)
print(result)  // 4.0
```

## 6. Reactive System

```asteron
// Reactive state
let count = state(0)

// Derived (updates automatically)
let doubled = derived(() => count * 2)

// Effect (executes when count changes)
effect(() => {
    print("Count: " + count + ", Doubled: " + doubled)
})

count = 10  // Effect executes automatically
```

## 7. Intent-Based Scheduling

```asteron
@intent optimize_latency
function process_data(data) {
    // Runtime automatically optimizes for low latency
    return heavy_computation(data)
}
```

## 8. Next Steps

- See [EXAMPLES.md](EXAMPLES.md) for more examples
- See [CONCEPTS.md](CONCEPTS.md) to understand concepts
- See [ARCHITECTURE.md](ARCHITECTURE.md) for architecture
- See [API.md](API.md) for API reference

## Tips

1. **Use interactive mode** (when available):
   ```bash
   ./asteron -i
   ```

2. **Compile with debug**:
   ```bash
   ./asteron --debug your_file.ast
   ```

3. **View unified graph**:
   ```bash
   ./asteron --graph your_file.ast
   ```

4. **Use Wasm server for visualization**:
   ```bash
   ./run_wasm_server.sh
   # Access http://localhost:8080
   ```

## Resources

- **Documentation**: [docs/](../docs/)
- **Examples**: [examples/](../examples/)
- **Issues**: [GitHub Issues](https://github.com/your-user/asteron/issues)
- **Discord/Forum**: [Community link]

## Help

Need help?
1. See the [complete documentation](../README.md)
2. Search [Issues](https://github.com/your-user/asteron/issues)
3. Open a new issue
4. Contact the community

Welcome to Asteron! 🚀
