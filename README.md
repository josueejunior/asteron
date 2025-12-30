# 🚀 Asteron - Self-Aware Runtime with Advanced JIT

**Developed by:** Josué Junior da Cruz de Freitas

**Asteron** is a revolutionary programming language and runtime that combines:
- ⚡ **Tiered JIT** (Baseline + Optimizing)
- 🧠 **Unified Graph** (CFG + Call Graph + Dependencies)
- 🔄 **Distributed Reactive System**
- 🧬 **Holographic Memory** (DVM + PMEM)
- 🎯 **Intent-Based Scheduling**
- 🔧 **Self-Healing Runtime**
- 🌐 **WebAssembly Integration**

## 📋 Table of Contents

- [Features](#-features)
- [Installation](#-installation)
- [Quick Usage](#-quick-usage)
- [Architecture](#-architecture)
- [Documentation](#-documentation)
- [Contributing](#-contributing)
- [License](#-license)

## ✨ Features

### Core
- Complete **Parser and Lexer**
- **Type System** with inference
- **VM** with optimized bytecode
- **Interpreter** for direct execution

### Advanced JIT
- **Tiered JIT**: Baseline → Optimizing (SSA + Graph Coloring)
- **OSR** (On-Stack Replacement)
- **Hot Path Detection**
- **Type Feedback**

### Memory
- **Reference Counting**
- **Ownership & Borrowing** (Rust-inspired)
- **Region-based Memory** (Arenas)
- **Zero-Copy Integration**
- **Holographic Memory** (DVM)

### Graphs and Metrics
- **Unified Graph** (CFG + Call Graph + Data Flow)
- **Node Metrics** (execution time, cache, contention)
- **Profile-Guided Optimization**
- **Auto-Parallelization**

### Reactivity
- Local **Reactive Runtime**
- **Distributed State Propagation**
- **Transparent RPC**
- **Cluster-Aware Reactivity**

### Scheduling
- **Intent-Based Scheduling**
- **Dynamic Hardware Heuristics** (AVX-512, SIMD)
- **AI-driven Load Prediction**
- **Pre-allocation & JIT Warming**

### Persistence
- **Snapshot Manager**
- **Persistent Memory** (PMEM/NVMe/Optane)
- **Reactive Variables** survive reboots

### WebAssembly
- **Compilation to Wasm** (Emscripten)
- **HTTP Server** for frontend
- **Real-time graph visualization**

## 🛠️ Installation

### Prerequisites

```bash
# GCC or Clang
sudo apt-get install build-essential

# For WebAssembly (optional)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

### Compilation

```bash
# Compile runtime
bash compile.sh

# Compile for WebAssembly (optional)
chmod +x build_wasm.sh
./build_wasm.sh
```

## 🚀 Quick Usage

### Run Asteron code

```bash
./asteron your_file.ast
```

### Wasm Server

```bash
chmod +x run_wasm_server.sh
./run_wasm_server.sh
# Access: http://localhost:8080
```

### AI Framework

```bash
./asteron framework/main.ast
```

## 🏗️ Architecture

```
┌─────────────────────────────────────────┐
│   Frontend (React/Next.js + Wasm)      │
├─────────────────────────────────────────┤
│   WebAssembly (Emscripten)              │
├─────────────────────────────────────────┤
│   VM + JIT (Tiered)                     │
├─────────────────────────────────────────┤
│   Unified Graph + Metrics               │
├─────────────────────────────────────────┤
│   Memory (Ownership + Regions + DVM)   │
├─────────────────────────────────────────┤
│   Reactive Runtime (Local + Distributed)│
├─────────────────────────────────────────┤
│   Intent-Based Scheduler                │
└─────────────────────────────────────────┘
```

## 📚 Documentation

- [Tiered JIT](docs/TIERED_JIT.md)
- [Region Memory](docs/REGION_MEMORY.md)
- [Self-Healing Runtime](docs/SELF_HEALING.md)
- [Distributed Reactive](docs/DISTRIBUTED_REACTIVE.md)
- [Intent-Based Scheduling](docs/INTENT_BASED_SCHEDULING.md)
- [Holographic Memory](docs/HOLOGRAPHIC_MEMORY.md)
- [WebAssembly Integration](docs/WASM_INTEGRATION.md)
- [AI Framework](framework/README.md)
- [Adaptive Runtime](docs/ADAPTIVE_RUNTIME.md)

## 🤝 Contributing

Contributions are welcome! Please:

1. Fork the project
2. Create a branch (`git checkout -b feature/new-feature`)
3. Commit your changes (`git commit -m 'Add new feature'`)
4. Push to the branch (`git push origin feature/new-feature`)
5. Open a Pull Request

### Guidelines

- Follow the existing code style
- Add tests when possible
- Document new features
- Maintain GPL v3 license

## 🛡️ License

**Asteron** is an open-source project under the **GNU GPL v3** license.

### Golden Rule

If you use Asteron as a base for your own language, tool, or runtime, the license requires that:

1. **Keep Code Open:** Your modifications must be public.
2. **Share with Community:** You must inform the Asteron community about your project so we can evolve the ecosystem together.
3. **Preserve License:** Any derivative work must be distributed under the same GPL v3 license.

> *Turning knowledge into something closed is the end of innovation. Let's build the future of JIT together.*

### What you can do

✅ **Use** Asteron in your projects  
✅ **Modify** the source code  
✅ **Distribute** modified versions  
✅ **Commercialize** products that use Asteron (as long as source code is made available)

### What you must do

📋 **Include** the LICENSE file in distributions  
📋 **Maintain** copyright notices  
📋 **Make available** the source code of derivative works  
📋 **Inform** the community about significant improvements

### Why GPL v3?

GPL v3 ensures that:

- **Innovations remain open:** Runtime improvements benefit the entire community
- **Transparency:** Everyone can see and audit the code
- **Collaboration:** Facilitates contributions and collective improvements
- **Protection:** Prevents misappropriation of community work

**Full license text:** See the [LICENSE](LICENSE) file in the project root.

## 👨‍💻 Developer

**Josué Junior da Cruz de Freitas**

Main developer and maintainer of the Asteron project.

## 📞 Contact

For questions about licensing or contributions, open an issue in the repository.

---

**Asteron** - Building the future of JIT, together. 🚀
