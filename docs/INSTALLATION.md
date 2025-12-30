# 📦 Installation Guide

## Prerequisites

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
# Install Xcode Command Line Tools
xcode-select --install

# Or use Homebrew
brew install gcc make git
```

### Windows

1. Install [WSL2](https://docs.microsoft.com/wsl/install) or [MSYS2](https://www.msys2.org/)
2. Follow Linux instructions inside WSL/MSYS2

## Installation

### 1. Clone the Repository

```bash
git clone https://github.com/your-user/asteron.git
cd asteron
```

### 2. Compile the Project

```bash
bash compile.sh
```

This will:
- Compile all source files
- Link the executable
- Create the `asteron` binary

### 3. Test the Installation

```bash
./asteron --version
```

Or run a test file:

```bash
echo 'print("Hello, Asteron!")' > test.ast
./asteron test.ast
```

## Optional Installation: WebAssembly

To compile for WebAssembly:

### 1. Install Emscripten

```bash
# Clone Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

# Install and activate
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

### 2. Compile for Wasm

```bash
cd asteron
chmod +x build_wasm.sh
./build_wasm.sh
```

This generates:
- `public/asteron.js`
- `public/asteron.wasm`

### 3. Run Wasm Server

```bash
chmod +x run_wasm_server.sh
./run_wasm_server.sh
```

Access: `http://localhost:8080`

## Development Installation

For development, you may want to:

### 1. Compile with Debug

```bash
# Edit compile.sh and add -g -O0
CFLAGS="-Wall -Wextra -std=c11 -g -O0 -I src ..."
bash compile.sh
```

### 2. Use GDB/LLDB

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

### Error: "gcc: command not found"

**Solution**: Install build-essential:
```bash
sudo apt-get install build-essential
```

### Error: "Permission denied"

**Solution**: Give execution permission:
```bash
chmod +x compile.sh
chmod +x asteron
```

### Error: "undefined reference"

**Solution**: Check if all files were compiled:
```bash
bash compile.sh 2>&1 | grep error
```

### Error: "Port already in use" (Wasm Server)

**Solution**: Use another port:
```bash
./run_wasm_server.sh 3000
```

### Error: "emcc not found" (Wasm)

**Solution**: Install and activate Emscripten:
```bash
source emsdk/emsdk_env.sh
```

## Installation Verification

Run the verification script:

```bash
# Create a test file
cat > test_install.ast << 'EOF'
function main() {
    print("Asteron installed successfully!")
    return 0
}
EOF

# Run
./asteron test_install.ast
```

If you see "Asteron installed successfully!", the installation is correct!

## Next Steps

- See [QUICKSTART.md](QUICKSTART.md) to get started
- See [EXAMPLES.md](EXAMPLES.md) for examples
- See [ARCHITECTURE.md](ARCHITECTURE.md) to understand the architecture

## Support

If you have problems:
1. Check [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
2. Open an [issue](https://github.com/your-user/asteron/issues)
3. Contact the maintainers
