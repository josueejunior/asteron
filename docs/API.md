# 📚 API Reference

Complete API reference for Asteron.

## Core Language

### Variables

```asteron
let x = 10              // Number
let y = "hello"        // String
let z = true           // Boolean
let w = nil            // Nil
```

### Functions

```asteron
function name(param1, param2) {
    // Code
    return value
}
```

### Conditionals

```asteron
if (condition) {
    // Code
} else {
    // Code
}
```

### Loops

```asteron
while (condition) {
    // Code
}
```

## Built-in Functions

### I/O

```asteron
print(value)           // Prints value
read_line()            // Reads line from stdin
```

### Strings

```asteron
len(str)               // String length
substr(str, start, len) // Substring
index_of(str, substr)  // Substring index
```

### Arrays

```asteron
array_push(arr, item)  // Adds item
array_pop(arr)        // Removes last item
array_len(arr)        // Array length
```

## Net Module

### TCP

```asteron
tcp_connect(host, port)           // Connects
tcp_listen(port)                  // Listens
tcp_accept(listener)              // Accepts connection
tcp_send(socket, data)            // Sends data
tcp_recv(socket, max_bytes)       // Receives data
tcp_close(socket)                 // Closes socket
```

### HTTP

```asteron
http_get(url)          // GET request
http_post(url, body)   // POST request
```

## FS Module

```asteron
read_file(path)        // Reads file
write_file(path, data) // Writes file
exists(path)          // Checks existence
mkdir(path)           // Creates directory
list_dir(path)        // Lists directory
```

## Math Module

```asteron
sqrt(x)               // Square root
pow(x, y)             // Power
sin(x)                // Sine
cos(x)                // Cosine
abs(x)                // Absolute value
```

## Time Module

```asteron
time_now()            // Current timestamp
sleep(seconds)         // Sleeps
format_time(timestamp, format) // Formats time
```

## Reactive System

```asteron
state(initial_value)              // Reactive state
derived(() => expression)         // Derived value
effect(() => { /* code */ })    // Side effect
computed(() => expression)       // Computed value (with cache)
```

## Intent-Based Scheduling

```asteron
@intent optimize_latency
function my_function() {
    // Code optimized for latency
}

@intent optimize_throughput
function another_function() {
    // Code optimized for throughput
}
```

## Ownership & Borrowing

```asteron
let x = create_object()  // Ownership
let y = borrow(x)        // Borrow (read-only)
let z = borrow_mut(x)    // Mutable borrow (exclusive)
```

## Region Memory

```asteron
let region = create_region("name")
let data = alloc_in_region(region, size)
destroy_region(region)  // Frees everything
```

## Distributed Memory

```asteron
let obj = create_object()
let remote = borrow_remote(obj, "node_id")
// Use as if it were local
```

## Persistent Memory

```asteron
let config = persistent_state({
    key: "value"
})
// Survives reboots
```

## Task (Concurrency)

```asteron
let task = task_spawn(function() {
    // Parallel code
})
task_join(task)  // Waits to finish
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
// Modify function
function my_function() {
    // New implementation
}
// Runtime reloads automatically
```

## Annotations

```asteron
@intent optimize_latency
@inline
@no_side_effects
function my_function() {
    // Code
}
```

## Error Handling

```asteron
try {
    // Code that may fail
} catch (error) {
    // Error handling
}
```

## Type System

```asteron
let x: number = 10
let y: string = "hello"
let z: bool = true
```

## Generics (Future)

```asteron
function identity<T>(x: T): T {
    return x
}
```

## Traits/Interfaces (Future)

```asteron
trait Printable {
    function print()
}

impl Printable for MyType {
    function print() {
        // Implementation
    }
}
```

## Pattern Matching (Future)

```asteron
match value {
    case 1 => print("one")
    case 2 => print("two")
    default => print("other")
}
```

---

**Note**: This is a reference in development. Some features may be in development or planned.
