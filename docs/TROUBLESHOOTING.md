# 🔧 Troubleshooting

Guide to resolve common problems with Asteron.

## Compilation Problems

### Error: "gcc: command not found"

**Cause**: GCC is not installed.

**Solution**:
```bash
# Ubuntu/Debian
sudo apt-get install build-essential

# Fedora
sudo dnf install gcc make

# macOS
xcode-select --install
```

### Error: "undefined reference"

**Cause**: Files were not compiled or linked correctly.

**Solution**:
```bash
# Clean and recompile
rm -rf obj/
bash compile.sh
```

### Error: "Permission denied"

**Cause**: Files don't have execution permission.

**Solution**:
```bash
chmod +x compile.sh
chmod +x asteron
chmod +x run_wasm_server.sh
chmod +x build_wasm.sh
```

### Error: "No such file or directory" (obj/)

**Cause**: Object directories were not created.

**Solution**:
```bash
# Create manually or recompile
mkdir -p obj/core/jit obj/core/memory obj/core/runtime obj/core/scheduling obj/server
bash compile.sh
```

## Execution Problems

### Error: "Error during VM execution"

**Cause**: Runtime error (e.g., function not found, incorrect type).

**Solution**:
1. Check if the function exists
2. Check argument types
3. Check if the function was registered correctly
4. Use `--debug` for more information:
   ```bash
   ./asteron --debug your_file.ast
   ```

### Error: "Variable not defined"

**Cause**: Variable used before being defined.

**Solution**:
```asteron
// Bad
print(x)  // x not defined
let x = 10

// Good
let x = 10
print(x)
```

### Error: "Function not found"

**Cause**: Function was not registered or doesn't exist.

**Solution**:
1. Check if the function is defined
2. Check if it's in the correct scope
3. For built-in functions, check if the module is available

## Wasm Server Problems

### Error: "Port already in use"

**Cause**: Port is already in use.

**Solution**:
```bash
# Use another port
./run_wasm_server.sh 3000

# Or kill the process on the port
lsof -ti:8080 | xargs kill
```

### Error: "emcc not found"

**Cause**: Emscripten is not installed or not in PATH.

**Solution**:
```bash
# Install Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# Return to project directory
cd ..
./build_wasm.sh
```

### Error: "asteron.js not found" (404)

**Cause**: Wasm file was not compiled.

**Solution**:
```bash
# Compile for Wasm
./build_wasm.sh

# Check if files exist
ls -la public/asteron.*
```

### Error: "compileCode is not defined"

**Cause**: JavaScript didn't load correctly.

**Solution**:
1. Reload the page (Ctrl+F5)
2. Check browser console
3. Check if server is running

## Memory Problems

### Error: "Memory leak detected"

**Cause**: Objects are not being freed.

**Solution**:
1. Check if you're using `value_release` correctly
2. Use Region-based Memory for temporary allocations
3. Use Valgrind to detect leaks:
   ```bash
   valgrind --leak-check=full ./asteron your_file.ast
   ```

### Error: "Out of memory"

**Cause**: Too much memory being used.

**Solution**:
1. Use Region-based Memory
2. Free objects when you don't need them anymore
3. Reduce the size of processed data

## Performance Problems

### Code is too slow

**Cause**: Code is not being optimized by JIT.

**Solution**:
1. Execute the code multiple times (JIT needs "warm-up")
2. Use `@intent optimize_latency` or `optimize_throughput`
3. Check if the code is in a "hot path"

### JIT is not activating

**Cause**: Code is not executed frequently enough.

**Solution**:
1. Execute the code in a loop
2. Execute multiple times
3. Check unified graph metrics

## Network Problems

### Error: "Connection refused"

**Cause**: Server is not running or wrong port.

**Solution**:
```bash
# Check if server is running
ps aux | grep asteron

# Check the port
netstat -tuln | grep 8080
```

### Error: "Timeout"

**Cause**: Connection took too long.

**Solution**:
1. Increase timeout
2. Check network
3. Check if server is responding

## WebAssembly Problems

### Error: "Wasm module failed to load"

**Cause**: Wasm module was not compiled correctly.

**Solution**:
1. Recompile Wasm:
   ```bash
   ./build_wasm.sh
   ```
2. Check if files were generated
3. Check browser console for more details

### Error: "Function not exported"

**Cause**: Function was not exported in Wasm.

**Solution**:
1. Check `src/wasm/asteron_wasm.c`
2. Check if function is in `EXPORTED_FUNCTIONS` in `build_wasm.sh`
3. Recompile

## Debugging Problems

### Debugger doesn't work

**Cause**: Binary was not compiled with debug.

**Solution**:
```bash
# Edit compile.sh and add -g
CFLAGS="-Wall -Wextra -std=c11 -g -O0 ..."

# Recompile
bash compile.sh

# Use GDB
gdb ./asteron
```

### Logs don't appear

**Cause**: Logs may be disabled.

**Solution**:
1. Use `--debug` flag
2. Check if `fprintf(stderr)` is being used
3. Check if `fflush` is being called

## System-Specific Problems

### Linux

#### Error: "clock_gettime not found"
**Solution**: Add `#define _POSIX_C_SOURCE 200809L`

#### Error: "pthread not found"
**Solution**: Install `libpthread-dev`

### macOS

#### Error: "ld: library not found"
**Solution**: Install Xcode Command Line Tools

### Windows (WSL)

#### Error: "Address already in use"
**Solution**: WSL may have port issues. Try another port.

## Still Having Problems?

1. **Check logs**: Look for error messages
2. **Consult documentation**: See [docs/](docs/)
3. **Open an issue**: [GitHub Issues](https://github.com/your-user/asteron/issues)
4. **Contact**: Community Discord/Forum

## Useful Information

### Collect Information for Debug

```bash
# Version
./asteron --version

# System information
uname -a
gcc --version

# Detailed logs
./asteron --debug your_file.ast 2>&1 | tee debug.log
```

### Useful Commands

```bash
# Clean build
rm -rf obj/ asteron

# Recompile everything
bash compile.sh

# Check files
find src -name "*.c" -o -name "*.h" | wc -l

# Check size
du -sh .
```

---

**Last updated**: 2025-01-XX
