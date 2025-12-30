# Quick Start - Framework de IA Operacional

## 🚀 Início Rápido

### 1. Execute o servidor

```bash
cd /mnt/c/Users/Usuário/asteron_lingguagens
./asteron framework/main.ast
```

### 2. Acesse no navegador

```
http://localhost:8080
http://localhost:8080/products
http://localhost:8080/about
```

### 3. Veja o grafo sendo construído

Cada request:
- Identifica o cliente
- Registra a página visitada no grafo
- Gera resposta HTML personalizada

## 📊 Monitoramento

O servidor mostra no console:
- Requests recebidos
- Páginas acessadas
- Estatísticas do grafo (a cada 10 requests)

## 🎯 Teste de Recomendação

1. Acesse várias páginas como `client_1`
2. Acesse páginas similares como `client_2`
3. O sistema detecta similaridade
4. Recomendações são geradas baseadas em clientes similares

## 🔧 Personalização

Edite `framework/logic/decision_engine.ast` para:
- Mudar regras de decisão
- Personalizar HTML
- Adicionar novas lógicas

## 📈 Analytics

Use `framework/logic/analytics.ast` para:
- Encontrar clusters de clientes
- Analisar padrões de navegação
- Gerar insights




