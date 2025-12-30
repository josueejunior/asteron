# 🤝 Contributing Guide

Thank you for considering contributing to Asteron! This document provides guidelines for contributions.

## Code of Conduct

This project follows the [Contributor Covenant Code of Conduct](https://www.contributor-covenant.org/). By participating, you agree to uphold this code.

## How to Contribute

### 1. Report Bugs

Before reporting a bug:

1. Check if the bug has already been reported in [Issues](https://github.com/your-user/asteron/issues)
2. If not, create a new issue with:
   - **Clear and descriptive title**
   - **Problem description**
   - **Steps to reproduce**
   - **Expected vs. actual behavior**
   - **Environment** (OS, version, etc.)
   - **Logs/errors** (if applicable)

### 2. Suggest Improvements

To suggest a new feature:

1. Check if it has already been suggested
2. Create an issue with:
   - **Clear title**
   - **Detailed description**
   - **Use cases**
   - **Benefits**
   - **Possible implementations** (if you have ideas)

### 3. Contribute Code

#### Environment Setup

1. **Fork** the repository
2. **Clone** your fork:
   ```bash
   git clone https://github.com/your-user/asteron.git
   cd asteron
   ```
3. **Compile** the project:
   ```bash
   bash compile.sh
   ```

#### Development Process

1. **Create a branch**:
   ```bash
   git checkout -b feature/new-feature
   # or
   git checkout -b fix/bug-fix
   ```

2. **Make your changes**:
   - Follow existing code style
   - Add comments when necessary
   - Maintain GPL v3 license

3. **Test your changes**:
   ```bash
   bash compile.sh
   ./asteron test/test.ast
   ```

4. **Commit your changes**:
   ```bash
   git add .
   git commit -m "feat: add new feature X"
   ```

   **Commit Convention:**
   - `feat:` New feature
   - `fix:` Bug fix
   - `docs:` Documentation
   - `style:` Formatting (doesn't affect code)
   - `refactor:` Refactoring
   - `test:` Tests
   - `chore:` Maintenance

5. **Push to your fork**:
   ```bash
   git push origin feature/new-feature
   ```

6. **Open a Pull Request**:
   - Describe your changes
   - Reference related issues
   - Add screenshots (if applicable)

## Code Guidelines

### C Style

- Use **4 spaces** for indentation
- Maximum of **100 characters** per line
- Use **snake_case** for functions and variables
- Use **UPPER_CASE** for constants
- Use **PascalCase** for types/structs

### Example:

```c
// Good
void process_data(DataContainer* container) {
    if (container == NULL) {
        return;
    }
    
    for (size_t i = 0; i < container->count; i++) {
        process_item(&container->items[i]);
    }
}

// Bad
void processData(DataContainer* c){
if(c==NULL)return;
for(int i=0;i<c->count;i++)processItem(&c->items[i]);
}
```

### Comments

- Use comments to explain **why**, not **what**
- Document public functions
- Use `//` for line comments
- Use `/* */` for block comments

### File Structure

```
src/
├── core/           # Runtime core
│   ├── lexer/      # Tokenization
│   ├── parser/     # Parsing
│   ├── ast/        # AST
│   ├── vm/         # Virtual Machine
│   └── jit/        # JIT Compiler
├── modules/        # Native modules
├── graph/          # Graph system
└── utils/          # Utilities
```

### Tests

- Add tests for new features
- Tests should be simple and focused
- Use `test/` for test files

## Areas Needing Contribution

### High Priority

1. **Tests**: More unit and integration tests
2. **Documentation**: Improve API documentation
3. **Performance**: Performance optimizations
4. **Bugs**: Fix known bugs

### Medium Priority

1. **Modules**: New native modules
2. **JIT Optimizations**: JIT improvements
3. **WebAssembly**: Wasm integration improvements
4. **Tooling**: Development tools

### Low Priority

1. **Examples**: More code examples
2. **Tutorials**: Step-by-step tutorials
3. **Translations**: Documentation translation

## Review Process

1. **Maintainers** review PRs
2. **Feedback** is given within 7 days
3. **Corrections** may be requested
4. **Approval** when everything is OK
5. **Merge** is done by maintainers

## License

By contributing, you agree that your contributions will be licensed under **GNU GPL v3**.

## Questions?

- Open an issue for questions
- Contact the maintainers
- See the [documentation](../README.md)

## Acknowledgments

Thank you for contributing to Asteron! 🚀
