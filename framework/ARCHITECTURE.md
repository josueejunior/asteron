# Arquitetura do Framework de IA Operacional

## 🏗️ Visão Geral

O framework é composto por 4 camadas principais:

```
┌─────────────────────────────────────────┐
│   HTTP Server (server.ast)             │  ← Recebe requests
├─────────────────────────────────────────┤
│   Graph Manager (graph_manager.ast)    │  ← Gerencia dados
├─────────────────────────────────────────┤
│   Logic Modules                         │  ← Processa e analisa
│   - recommendation.ast                  │
│   - analytics.ast                       │
│   - decision_engine.ast                 │
├─────────────────────────────────────────┤
│   Graph Module (nativo)                 │  ← Armazenamento
└─────────────────────────────────────────┘
```

## 📊 Fluxo de Dados

```
1. Cliente → HTTP Request
   ↓
2. Server extrai: client_id, page
   ↓
3. Graph Manager atualiza grafo:
   - Adiciona nós (cliente, página)
   - Adiciona aresta (cliente → página)
   ↓
4. Logic Modules analisam:
   - Recommendation: encontra produtos similares
   - Analytics: descobre padrões
   - Decision Engine: decide conteúdo
   ↓
5. Decision Engine gera HTML personalizado
   ↓
6. Server envia resposta HTTP
   ↓
7. Cliente recebe HTML adaptado
```

## 🔧 Componentes Detalhados

### 1. Graph Manager (`graph/graph_manager.ast`)

**Responsabilidades:**
- Gerenciar nós (clientes, páginas, produtos)
- Gerenciar arestas (relações entre nós)
- Buscar contexto de clientes
- Encontrar clientes similares

**Funções principais:**
```asteron
add_node(node_id, node_type, metadata)
add_edge(from_node, to_node, edge_type, weight)
get_neighbors(node_id)
get_context(client_id)
find_similar_clients(client_id)
```

### 2. Recommendation Engine (`logic/recommendation.ast`)

**Algoritmos:**
- **Collaborative Filtering**: Baseado em clientes similares
- **Content-Based**: Baseado no histórico do cliente
- **Híbrido**: Combina ambos

**Funções:**
```asteron
recommend_by_similarity(client_id)
recommend_by_history(client_id)
recommend_hybrid(client_id)
```

### 3. Analytics (`logic/analytics.ast`)

**Análises:**
- Clustering de clientes
- Páginas mais visitadas
- Padrões de navegação
- Insights gerais

**Funções:**
```asteron
find_client_clusters()
find_most_visited_pages(limit)
analyze_navigation_pattern(client_id)
generate_insights()
```

### 4. Decision Engine (`logic/decision_engine.ast`)

**Decisões:**
- Qual conteúdo mostrar
- Quando mostrar recomendações
- Quando mostrar ofertas
- Qual produto destacar

**Funções:**
```asteron
generate_personalized_response(client_id, page)
decide_content(client_id, page, recommendations)
should_show_offer(client_id)
decide_featured_product(client_id)
```

## 🎯 Regras de Decisão

O Decision Engine implementa regras como:

1. **Cliente novo** (≤1 página visitada) → Mostra boas-vindas
2. **Cliente ativo** (>3 páginas) → Mostra recomendações
3. **Página /products** → Mostra lista de produtos
4. **Cliente sem compras** (>5 páginas) → Mostra oferta especial

## 📈 Escalabilidade

### Atual (Memória)
- Grafo armazenado em memória
- Limite: ~1MB de dados
- Performance: O(n) para buscas

### Futuro (Persistência)
- Salvar em JSON ou banco de dados
- Indexação para buscas rápidas
- Cache de recomendações
- Sharding para múltiplos servidores

## 🔮 Evolução para IA Avançada

### Fase 1: Atual ✅
- Regras baseadas em grafos
- Collaborative Filtering simples
- Analytics básico

### Fase 2: Próximo
- Graph Neural Networks (GNNs)
- Machine Learning para recomendações
- Análise preditiva

### Fase 3: Futuro
- Integração com LLMs
- Descrições geradas por IA
- Personalização avançada

## 🛠️ Extensibilidade

Para adicionar novos módulos:

1. Crie arquivo `.ast` em `framework/logic/`
2. Implemente funções seguindo padrão existente
3. Integre no `decision_engine.ast`
4. Teste com `main.ast`

## 📝 Notas de Implementação

- **Imports**: Sistema de imports do Asteron ainda em desenvolvimento
- **Funções compartilhadas**: Atualmente duplicadas em cada módulo
- **Persistência**: Adicionar salvamento em JSON ou DB
- **Performance**: Otimizar buscas em grafos grandes





