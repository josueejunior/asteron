# Changelog

Todas as mudanças notáveis neste projeto serão documentadas neste arquivo.

O formato é baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.0.0/),
e este projeto adere ao [Semantic Versioning](https://semver.org/lang/pt-BR/).

## [Unreleased]

### Adicionado
- Sistema de licença GPL v3
- Cabeçalhos de licença em arquivos principais
- Documentação completa (ARCHITECTURE, CONCEPTS, API, etc.)
- Servidor WebAssembly
- Modo simulado para compilação sem Wasm
- Tratamento de favicon.ico no servidor

### Corrigido
- Servidor não fecha mais após requisições
- Função compileCode agora está no escopo global
- Erros 404 do favicon.ico
- Estruturas incompletas em holographic.c
- Campo prediction_capacity em intent_based.h

### Mudado
- Melhorado tratamento de erros no servidor Wasm
- Melhorada experiência do usuário no frontend

## [0.1.0] - 2025-01-XX

### Adicionado
- Core runtime completo
- Lexer e Parser
- AST (Abstract Syntax Tree)
- Type Checker
- Bytecode Compiler
- Virtual Machine
- Interpreter

### JIT Compilation
- Trace-based JIT
- Tiered JIT (Baseline + Optimizing)
- SSA (Static Single Assignment)
- Graph Coloring (Register Allocation)
- OSR (On-Stack Replacement)

### Memory Management
- Reference Counting
- Ownership & Borrowing
- Region-based Memory
- Zero-Copy Integration

### Unified Graph
- CFG (Control Flow Graph)
- Call Graph
- Data Flow Graph
- Node Metrics
- Graph Visualization

### Reactive System
- Local Reactivity
- Reactive Nodes (STATE, DERIVED, EFFECT)
- Dependency Tracking

### Self-Healing Runtime
- Auto-Paralelização
- Profile-Guided Re-optimization
- Performance Monitoring

### Intent-Based Scheduling
- Intent Declarations
- Hardware Detection (AVX-512, SIMD)
- Load Prediction

### Holographic Memory
- DVM (Distributed Virtual Machine)
- Global Address Space
- Distributed Ownership
- Persistent Memory Support

### WebAssembly
- Emscripten Integration
- Browser Runtime
- Real-time Visualization
- Interactive Editor

### Módulos Nativos
- net: Networking (TCP, HTTP)
- fs: Sistema de arquivos
- math: Operações matemáticas
- time: Manipulação de tempo
- task: Concorrência
- graph: Sistema de grafos
- agent: Sistema de agentes
- os: Operações do sistema

---

## Formato

- **Adicionado**: Para novas funcionalidades
- **Mudado**: Para mudanças em funcionalidades existentes
- **Descontinuado**: Para funcionalidades que serão removidas
- **Removido**: Para funcionalidades removidas
- **Corrigido**: Para correções de bugs
- **Segurança**: Para vulnerabilidades

