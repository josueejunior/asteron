# Framework de IA Operacional para Asteron

Framework completo que integra servidor HTTP, sistema de grafos e módulos de IA para criar respostas personalizadas baseadas em comportamento do usuário.

## 📁 Estrutura

```
framework/
├── server/
│   ├── server.ast          # Servidor HTTP principal
│   └── routes/              # Endpoints e rotas (futuro)
│
├── graph/
│   └── graph_manager.ast   # Gerenciador de grafos (nós, arestas, contexto)
│
├── logic/
│   ├── recommendation.ast  # Sistema de recomendação (collaborative + content-based)
│   ├── analytics.ast        # Análise de padrões e insights
│   └── decision_engine.ast # Motor de decisão automática
│
├── database/
│   └── migrations/         # Migrations (futuro)
│
└── main.ast                # Ponto de entrada que une tudo
```

## 🚀 Como Usar

### 1. Compilar o Asteron

```bash
cd /mnt/c/Users/Usuário/asteron_lingguagens
bash compile.sh
```

### 2. Executar o Framework

```bash
./asteron framework/main.ast
```

### 3. Acessar

Abra no navegador: `http://localhost:8080`

## 🔧 Componentes

### Graph Manager (`graph/graph_manager.ast`)

Funções principais:
- `add_node(node_id, node_type, metadata)` - Adiciona nó ao grafo
- `add_edge(from_node, to_node, edge_type, weight)` - Adiciona aresta
- `get_neighbors(node_id)` - Obtém vizinhos de um nó
- `get_context(client_id)` - Obtém contexto completo do cliente
- `find_similar_clients(client_id)` - Encontra clientes similares

### Recommendation Engine (`logic/recommendation.ast`)

Algoritmos:
- `recommend_by_similarity(client_id)` - Collaborative Filtering
- `recommend_by_history(client_id)` - Content-Based Filtering
- `recommend_hybrid(client_id)` - Combinação híbrida

### Analytics (`logic/analytics.ast`)

Análises:
- `find_client_clusters()` - Encontra clusters de clientes
- `find_most_visited_pages(limit)` - Páginas mais visitadas
- `analyze_navigation_pattern(client_id)` - Padrões de navegação
- `generate_insights()` - Insights gerais

### Decision Engine (`logic/decision_engine.ast`)

Decisões:
- `generate_personalized_response(client_id, page)` - Gera resposta HTML
- `decide_content(client_id, page, recommendations)` - Decide conteúdo
- `should_show_offer(client_id)` - Decide se mostra oferta
- `decide_featured_product(client_id)` - Produto em destaque

## 📊 Fluxo de Operação

```
Cliente → Servidor HTTP → Grafo (atualiza) → Lógica (analisa) → Decision Engine → HTML Personalizado → Cliente
```

1. **Cliente faz request** → Servidor recebe
2. **Servidor identifica cliente** → Extrai client_id
3. **Atualiza grafo** → Registra comportamento (página visitada)
4. **Lógica analisa** → Recomendação, Analytics
5. **Decision Engine decide** → Qual conteúdo mostrar
6. **Gera HTML** → Resposta personalizada
7. **Cliente recebe** → HTML adaptado ao comportamento

## 🎯 Exemplo de Uso

### Adicionar nó e aresta:

```asteron
// Adiciona cliente
add_node("client_1", "client", "")

// Adiciona página
add_node("/products", "page", "")

// Cliente visitou página
add_edge("client_1", "/products", "visited", "")
```

### Obter recomendações:

```asteron
// Recomendação híbrida
let recommendations = recommend_hybrid("client_1")
```

### Gerar resposta personalizada:

```asteron
let html = generate_personalized_response("client_1", "/products")
```

## 🔮 Próximos Passos

1. **Sistema de Imports** - Permitir importar módulos .ast
2. **Persistência** - Salvar grafo em JSON ou banco de dados
3. **Sessões** - Gerenciar sessões de usuário
4. **Cache** - Cache de recomendações
5. **APIs REST** - Endpoints para consultar grafo e analytics
6. **GNNs** - Integrar Graph Neural Networks (futuro)
7. **LLMs** - Integrar com LLMs para descrições (futuro)

## 📝 Notas

- O framework usa o módulo `graph` nativo do Asteron
- Todos os módulos são escritos em Asteron
- O grafo é armazenado em memória (adicionar persistência)
- Sistema de imports ainda não está completo (copiar código ou usar funções globais)

## 🐛 Troubleshooting

Se o servidor não iniciar:
- Verifique se a porta 8080 está livre
- Tente portas 8081, 8082, etc.

Se funções não forem encontradas:
- Certifique-se de que o módulo `graph` está importado
- Verifique se todas as funções estão definidas antes de usar




