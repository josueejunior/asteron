# 🤝 Guia de Contribuição

Obrigado por considerar contribuir com o Asteron! Este documento fornece diretrizes para contribuições.

## Código de Conduta

Este projeto segue o [Código de Conduta do Contributor Covenant](https://www.contributor-covenant.org/). Ao participar, você concorda em manter este código.

## Como Contribuir

### 1. Reportar Bugs

Antes de reportar um bug:

1. Verifique se o bug já foi reportado nas [Issues](https://github.com/seu-usuario/asteron/issues)
2. Se não foi, crie uma nova issue com:
   - **Título claro e descritivo**
   - **Descrição do problema**
   - **Passos para reproduzir**
   - **Comportamento esperado vs. atual**
   - **Ambiente** (OS, versão, etc.)
   - **Logs/erros** (se aplicável)

### 2. Sugerir Melhorias

Para sugerir uma nova funcionalidade:

1. Verifique se já foi sugerida
2. Crie uma issue com:
   - **Título claro**
   - **Descrição detalhada**
   - **Casos de uso**
   - **Benefícios**
   - **Possíveis implementações** (se tiver ideias)

### 3. Contribuir com Código

#### Setup do Ambiente

1. **Fork** o repositório
2. **Clone** seu fork:
   ```bash
   git clone https://github.com/seu-usuario/asteron.git
   cd asteron
   ```
3. **Compile** o projeto:
   ```bash
   bash compile.sh
   ```

#### Processo de Desenvolvimento

1. **Crie uma branch**:
   ```bash
   git checkout -b feature/nova-funcionalidade
   # ou
   git checkout -b fix/correcao-bug
   ```

2. **Faça suas alterações**:
   - Siga o estilo de código existente
   - Adicione comentários quando necessário
   - Mantenha a licença GPL v3

3. **Teste suas alterações**:
   ```bash
   bash compile.sh
   ./asteron test/test.ast
   ```

4. **Commit suas mudanças**:
   ```bash
   git add .
   git commit -m "feat: adiciona nova funcionalidade X"
   ```

   **Convenção de Commits:**
   - `feat:` Nova funcionalidade
   - `fix:` Correção de bug
   - `docs:` Documentação
   - `style:` Formatação (não afeta código)
   - `refactor:` Refatoração
   - `test:` Testes
   - `chore:` Manutenção

5. **Push para seu fork**:
   ```bash
   git push origin feature/nova-funcionalidade
   ```

6. **Abra um Pull Request**:
   - Descreva suas mudanças
   - Referencie issues relacionadas
   - Adicione screenshots (se aplicável)

## Diretrizes de Código

### Estilo C

- Use **4 espaços** para indentação
- Máximo de **100 caracteres** por linha
- Use **snake_case** para funções e variáveis
- Use **UPPER_CASE** para constantes
- Use **PascalCase** para tipos/structs

### Exemplo:

```c
// Bom
void process_data(DataContainer* container) {
    if (container == NULL) {
        return;
    }
    
    for (size_t i = 0; i < container->count; i++) {
        process_item(&container->items[i]);
    }
}

// Ruim
void processData(DataContainer* c){
if(c==NULL)return;
for(int i=0;i<c->count;i++)processItem(&c->items[i]);
}
```

### Comentários

- Use comentários para explicar **por quê**, não **o quê**
- Documente funções públicas
- Use `//` para comentários de linha
- Use `/* */` para comentários de bloco

### Estrutura de Arquivos

```
src/
├── core/           # Core do runtime
│   ├── lexer/      # Tokenização
│   ├── parser/     # Parsing
│   ├── ast/        # AST
│   ├── vm/         # Virtual Machine
│   └── jit/        # JIT Compiler
├── modules/        # Módulos nativos
├── graph/          # Sistema de grafos
└── utils/          # Utilitários
```

### Testes

- Adicione testes para novas funcionalidades
- Testes devem ser simples e focados
- Use `test/` para arquivos de teste

## Áreas que Precisam de Contribuição

### Prioridade Alta

1. **Testes**: Mais testes unitários e de integração
2. **Documentação**: Melhorar documentação de APIs
3. **Performance**: Otimizações de performance
4. **Bugs**: Correção de bugs conhecidos

### Prioridade Média

1. **Módulos**: Novos módulos nativos
2. **Otimizações JIT**: Melhorias no JIT
3. **WebAssembly**: Melhorias na integração Wasm
4. **Tooling**: Ferramentas de desenvolvimento

### Prioridade Baixa

1. **Exemplos**: Mais exemplos de código
2. **Tutoriais**: Tutoriais passo a passo
3. **Traduções**: Tradução de documentação

## Processo de Review

1. **Mantenedores** revisam PRs
2. **Feedback** é dado em até 7 dias
3. **Correções** podem ser solicitadas
4. **Aprovação** quando tudo estiver OK
5. **Merge** é feito pelos mantenedores

## Licença

Ao contribuir, você concorda que suas contribuições serão licenciadas sob a **GNU GPL v3**.

## Perguntas?

- Abra uma issue para perguntas
- Entre em contato com os mantenedores
- Veja a [documentação](../README.md)

## Agradecimentos

Obrigado por contribuir com o Asteron! 🚀

