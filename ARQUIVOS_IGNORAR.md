# 🗑️ Arquivos que NÃO devem ser subidos para o Git

## Binários Compilados

```
asteron              # Executável principal
wasm_server          # Servidor Wasm
*.exe                # Executáveis Windows
*.out                # Executáveis Linux
*.o                  # Arquivos objeto (.o)
*.a                  # Bibliotecas estáticas
*.so                 # Bibliotecas dinâmicas Linux
*.dylib              # Bibliotecas dinâmicas macOS
*.dll                # Bibliotecas dinâmicas Windows
```

## Diretórios de Build

```
obj/                 # Todos os arquivos objeto compilados
build/               # Diretório de build
dist/                # Distribuição
bin/                 # Binários
```

## WebAssembly

```
public/asteron.js    # JavaScript gerado pelo Emscripten
public/asteron.wasm  # Binário WebAssembly
*.wasm               # Qualquer arquivo .wasm
```

## Logs e Arquivos Temporários

```
*.log                # Todos os logs
debug.log            # Log de debug
saida.txt            # Saída de testes
*.tmp                # Arquivos temporários
*.temp               # Arquivos temporários
*~                   # Arquivos de backup do editor
```

## Sistema Operacional

```
.DS_Store            # macOS
Thumbs.db            # Windows
desktop.ini          # Windows
```

## IDEs e Editores

```
.vscode/             # Configurações do VS Code
.idea/               # Configurações do IntelliJ/CLion
*.swp                # Vim swap files
*.swo                # Vim swap files
*.swn                # Vim swap files
.project             # Eclipse
.classpath           # Eclipse
.settings/           # Eclipse
```

## Emscripten

```
emsdk/               # SDK do Emscripten (muito grande)
.cache/              # Cache do Emscripten
```

## Testes e Coverage

```
test_output/         # Saída de testes
*.test               # Arquivos de teste compilados
coverage/            # Relatórios de cobertura
*.gcda               # Dados de cobertura GCC
*.gcno               # Notas de cobertura GCC
*.gcov               # Relatórios de cobertura
```

## Valgrind e Debug

```
vgcore.*             # Core dumps do Valgrind
core                 # Core dumps
core.*               # Core dumps
```

## Backup

```
*.bak                # Arquivos de backup
*.backup             # Arquivos de backup
*_backup             # Arquivos de backup
```

## Configuração Local

```
.env                 # Variáveis de ambiente
.env.local           # Variáveis de ambiente local
config.local.*       # Configurações locais
```

## Documentação Gerada

```
docs/_build/         # Build de documentação
site/                # Site gerado
```

## Arquivos Específicos do Projeto

```
saida.txt            # Saída de testes/debug
debug.log            # Log de debug
```

---

## Como verificar o que será ignorado

```bash
# Ver o que está sendo ignorado
git status --ignored

# Ver arquivos que seriam adicionados (mas estão ignorados)
git ls-files --others --ignored --exclude-standard
```

## Limpar arquivos já commitados por engano

Se você já commitou algum desses arquivos:

```bash
# Remove do Git (mas mantém localmente)
git rm --cached asteron
git rm --cached wasm_server
git rm --cached debug.log
git rm -r --cached obj/

# Commit a remoção
git commit -m "Remove arquivos de build do repositório"
```

## Verificar antes de commitar

```bash
# Ver o que será commitado
git status

# Ver diferenças
git diff

# Ver arquivos que serão adicionados
git add -n .
```

