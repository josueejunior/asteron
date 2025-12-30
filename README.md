# 🚀 Asteron - Runtime Autoconsciente com JIT Avançado

**Asteron** é uma linguagem de programação e runtime revolucionário que combina:
- ⚡ **JIT Tiered** (Baseline + Optimizing)
- 🧠 **Grafo Unificado** (CFG + Call Graph + Dependencies)
- 🔄 **Sistema Reativo Distribuído**
- 🧬 **Memória Holográfica** (DVM + PMEM)
- 🎯 **Intent-Based Scheduling**
- 🔧 **Self-Healing Runtime**
- 🌐 **WebAssembly Integration**

## 📋 Índice

- [Características](#-características)
- [Instalação](#-instalação)
- [Uso Rápido](#-uso-rápido)
- [Arquitetura](#-arquitetura)
- [Documentação](#-documentação)
- [Contribuição](#-contribuição)
- [Licença](#-licença)

## ✨ Características

### Core
- **Parser e Lexer** completos
- **Type System** com inferência
- **VM** com bytecode otimizado
- **Interpreter** para execução direta

### JIT Avançado
- **Tiered JIT**: Baseline → Optimizing (SSA + Graph Coloring)
- **OSR** (On-Stack Replacement)
- **Hot Path Detection**
- **Type Feedback**

### Memória
- **Reference Counting**
- **Ownership & Borrowing** (Rust-inspired)
- **Region-based Memory** (Arenas)
- **Zero-Copy Integration**
- **Holographic Memory** (DVM)

### Grafos e Métricas
- **Unified Graph** (CFG + Call Graph + Data Flow)
- **Node Metrics** (execution time, cache, contention)
- **Profile-Guided Optimization**
- **Auto-Paralelização**

### Reactividade
- **Reactive Runtime** local
- **Distributed State Propagation**
- **Transparent RPC**
- **Cluster-Aware Reactivity**

### Scheduling
- **Intent-Based Scheduling**
- **Dynamic Hardware Heuristics** (AVX-512, SIMD)
- **AI-driven Load Prediction**
- **Pre-allocation & JIT Warming**

### Persistência
- **Snapshot Manager**
- **Persistent Memory** (PMEM/NVMe/Optane)
- **Reactive Variables** sobrevivem reboots

### WebAssembly
- **Compilação para Wasm** (Emscripten)
- **Servidor HTTP** para frontend
- **Visualização de grafo em tempo real**

## 🛠️ Instalação

### Pré-requisitos

```bash
# GCC ou Clang
sudo apt-get install build-essential

# Para WebAssembly (opcional)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

### Compilação

```bash
# Compilar runtime
bash compile.sh

# Compilar para WebAssembly (opcional)
chmod +x build_wasm.sh
./build_wasm.sh
```

## 🚀 Uso Rápido

### Executar código Asteron

```bash
./asteron seu_arquivo.ast
```

### Servidor Wasm

```bash
chmod +x run_wasm_server.sh
./run_wasm_server.sh
# Acesse: http://localhost:8080
```

### Framework de IA

```bash
./asteron framework/main.ast
```

## 🏗️ Arquitetura

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

## 📚 Documentação

- [Tiered JIT](docs/TIERED_JIT.md)
- [Region Memory](docs/REGION_MEMORY.md)
- [Self-Healing Runtime](docs/SELF_HEALING.md)
- [Distributed Reactive](docs/DISTRIBUTED_REACTIVE.md)
- [Intent-Based Scheduling](docs/INTENT_BASED_SCHEDULING.md)
- [Holographic Memory](docs/HOLOGRAPHIC_MEMORY.md)
- [WebAssembly Integration](docs/WASM_INTEGRATION.md)
- [Framework de IA](framework/README.md)

## 🤝 Contribuição

Contribuições são bem-vindas! Por favor:

1. Fork o projeto
2. Crie uma branch (`git checkout -b feature/nova-feature`)
3. Commit suas mudanças (`git commit -m 'Adiciona nova feature'`)
4. Push para a branch (`git push origin feature/nova-feature`)
5. Abra um Pull Request

### Diretrizes

- Siga o estilo de código existente
- Adicione testes quando possível
- Documente novas funcionalidades
- Mantenha a licença GPL v3

## 🛡️ Licença

O **Asteron** é um projeto de código aberto sob a licença **GNU GPL v3**.

### Regra de Ouro

Se você utilizar o Asteron como base para sua própria linguagem, ferramenta ou runtime, a licença exige que:

1. **Mantenha o Código Aberto:** Suas modificações devem ser públicas.
2. **Divulgue para a Comunidade:** Você deve informar à comunidade Asteron sobre o seu projeto para que possamos evoluir o ecossistema juntos.
3. **Preserve a Licença:** Qualquer trabalho derivado deve ser distribuído sob a mesma licença GPL v3.

> *Transformar o conhecimento em algo fechado é o fim da inovação. Vamos construir o futuro do JIT juntos.*

### O que você pode fazer

✅ **Usar** o Asteron em seus projetos  
✅ **Modificar** o código-fonte  
✅ **Distribuir** versões modificadas  
✅ **Comercializar** produtos que usam Asteron (desde que o código-fonte seja disponibilizado)

### O que você deve fazer

📋 **Incluir** o arquivo LICENSE em distribuições  
📋 **Manter** os avisos de copyright  
📋 **Disponibilizar** o código-fonte de trabalhos derivados  
📋 **Informar** a comunidade sobre melhorias significativas

### Por que GPL v3?

A GPL v3 garante que:

- **Inovações permaneçam abertas:** Melhorias no runtime beneficiam toda a comunidade
- **Transparência:** Todos podem ver e auditar o código
- **Colaboração:** Facilita contribuições e melhorias coletivas
- **Proteção:** Previne apropriação indevida do trabalho comunitário

**Texto completo da licença:** Veja o arquivo [LICENSE](LICENSE) na raiz do projeto.

## 📞 Contato

Para questões sobre licenciamento ou contribuições, abra uma issue no repositório.

---

**Asteron** - Construindo o futuro do JIT, juntos. 🚀

