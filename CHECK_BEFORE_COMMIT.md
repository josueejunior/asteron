# ✅ Checklist Antes de Commitar

Use este checklist antes de fazer commit para garantir que não suba "lixo" para o repositório.

## Verificações Rápidas

### 1. Verificar Status do Git

```bash
git status
```

**Verifique se aparecem:**
- ❌ `asteron` (binário)
- ❌ `wasm_server` (binário)
- ❌ `obj/` (diretório de build)
- ❌ `debug.log` ou `*.log`
- ❌ `saida.txt`
- ❌ `public/asteron.js` ou `public/asteron.wasm`

**Se aparecerem, estão sendo ignorados corretamente pelo .gitignore**

### 2. Verificar Arquivos que Serão Commitados

```bash
git status --short
```

**Deve mostrar apenas:**
- ✅ Arquivos `.c`, `.h` (código fonte)
- ✅ Arquivos `.md` (documentação)
- ✅ Arquivos `.sh` (scripts)
- ✅ Arquivos `.ast` (código Asteron)
- ✅ Arquivos de configuração (`.gitignore`, `LICENSE`, etc.)

### 3. Verificar Tamanho dos Arquivos

```bash
# Ver arquivos grandes (> 1MB)
find . -type f -size +1M -not -path "./.git/*" -not -path "./obj/*" -not -path "./emsdk/*"
```

**Arquivos grandes geralmente não devem ser commitados:**
- ❌ Binários
- ❌ Arquivos Wasm
- ❌ SDKs completos

## Comandos Úteis

### Ver o que está sendo ignorado

```bash
git status --ignored
```

### Ver diferenças antes de commitar

```bash
git diff
git diff --cached  # Para arquivos já adicionados
```

### Adicionar arquivos seletivamente

```bash
# Adicionar apenas arquivos específicos
git add src/
git add docs/
git add README.md

# NÃO faça:
git add .  # Pode adicionar arquivos indesejados
```

### Remover arquivos do staging

```bash
# Se adicionou algo por engano
git reset HEAD arquivo_indesejado
```

## Lista de Arquivos que NUNCA devem ser commitados

### Binários
- `asteron`
- `wasm_server`
- Qualquer `*.exe`, `*.out`, `*.o`

### Build
- `obj/` (todo o diretório)
- `build/`
- `dist/`

### Logs
- `*.log`
- `debug.log`
- `saida.txt`

### WebAssembly
- `public/asteron.js`
- `public/asteron.wasm`

### Sistema/IDE
- `.DS_Store`
- `.vscode/`
- `.idea/`

## Se já commitou por engano

```bash
# 1. Remover do Git (mantém localmente)
git rm --cached asteron
git rm --cached wasm_server
git rm -r --cached obj/

# 2. Adicionar ao .gitignore (se não estiver)
echo "asteron" >> .gitignore
echo "wasm_server" >> .gitignore

# 3. Commit a remoção
git add .gitignore
git commit -m "Remove arquivos de build do repositório"

# 4. Push (cuidado se já fez push antes!)
git push
```

## Comando Final Antes de Push

```bash
# Ver exatamente o que será enviado
git log origin/main..HEAD  # Ver commits locais
git diff origin/main..HEAD --stat  # Ver arquivos modificados
```

---

**Dica**: Configure um hook pre-commit para verificar automaticamente!

