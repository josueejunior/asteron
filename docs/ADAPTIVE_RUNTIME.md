# 🧠 Consolidated Adaptive Runtime

The **Adaptive Runtime** is the consolidated system that coordinates all auto-adaptation components of Asteron.

## 🎯 Overview

The Adaptive Runtime unites three main systems:

1. **Context Brain** - Meta-layer coordinator that observes and decides
2. **Intent Engine** - Declarative intention system
3. **Self-Tuning Runtime** - Continuous learning

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────┐
│              ADAPTIVE RUNTIME                            │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │ Context Brain│  │Intent Engine │  │ Self-Tuning  │ │
│  │ (Coordinator)│  │ (Intentions) │  │ (Learning)   │ │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘ │
│         │                  │                 │          │
│         └──────────────────┴─────────────────┘          │
│                        │                                 │
│         ┌──────────────▼──────────────┐                 │
│         │   Automatic Adaptation     │                 │
│         └─────────────────────────────┘                 │
│                        │                                 │
│         ┌──────────────▼──────────────┐                 │
│         │   VM / Unified Graph       │                 │
│         └─────────────────────────────┘                 │
└─────────────────────────────────────────────────────────┘
```

## 🔄 Adaptation Cycle

1. **Observation**: Context Brain collects system metrics
2. **Analysis**: Analyzes metrics and generates decisions
3. **Execution**: Executes decisions (parallelization, JIT, etc.)
4. **Learning**: Self-Tuning learns from results
5. **Refinement**: Strategies are refined

## 📊 Collected Metrics

- **CPU Usage**: System CPU usage
- **Memory Usage**: Memory usage
- **Hot Paths**: Number of hot paths
- **JIT Compilations**: Number of JIT compilations
- **Execution Time**: Average execution time

## 🎛️ Automatic Decisions

The system can automatically make the following decisions:

- **Parallelization**: Parallelize independent nodes
- **JIT Tier**: Change JIT tier (Baseline → Optimizing)
- **Deoptimization**: Reduce optimizations under pressure
- **Fine Tuning**: Refine strategies

## 🚀 Usage

```c
// Create adaptive runtime
AdaptiveRuntime* rt = adaptive_runtime_create(vm, graph, healer, scheduler);

// Start automatic adaptation
adaptive_runtime_start(rt);

// During execution, the system adapts automatically
// (called periodically or after events)

// Stop adaptation
adaptive_runtime_stop(rt);

// Cleanup
adaptive_runtime_destroy(rt);
```

## 📈 Statistics

The system maintains statistics on:
- Total adaptations
- Successful adaptations
- Success rate
- Observed improvements

## 🔧 Configuration

- **Adaptation Interval**: Interval between adaptations (default: 1 second)
- **Decision Threshold**: Confidence threshold for decisions (default: 0.7)
- **Auto-Adapt**: Enable/disable automatic adaptation

## 🎓 Learning

The Self-Tuning Runtime continuously learns:
- Which strategies work best
- When to parallelize
- When to optimize JIT
- When to deoptimize

## 🔮 Future

- GPU integration
- AI-driven load prediction
- Adaptation based on declarative intentions
- Complete reinforcement learning
