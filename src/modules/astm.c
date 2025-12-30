/**
 * =============================================================================
 * ASTERON MODULE FORMAT (.astm) - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "astm.h"
#include "module.h"
#include "../core/lexer/lexer.h"
#include "../core/parser/parser.h"
#include "../core/ast/ast.h"
#include "../core/typechecker/typechecker.h"
#include "../core/optimizer/ssa.h"
#include "../utils/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

/* =============================================================================
 * HASH FUNCTION (djb2)
 * ============================================================================= */

static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/* =============================================================================
 * TABELA DE SÍMBOLOS
 * ============================================================================= */

#define SYMBOL_TABLE_BUCKETS 64

SymbolTable* symbol_table_create(void) {
    SymbolTable* table = (SymbolTable*)malloc(sizeof(SymbolTable));
    if (table == NULL) return NULL;
    
    table->bucket_count = SYMBOL_TABLE_BUCKETS;
    table->buckets = (SymbolEntry**)calloc(SYMBOL_TABLE_BUCKETS, sizeof(SymbolEntry*));
    if (table->buckets == NULL) {
        free(table);
        return NULL;
    }
    
    table->entry_count = 0;
    
    /* Exports */
    table->export_capacity = 16;
    table->export_count = 0;
    table->exports = (SymbolEntry**)malloc(sizeof(SymbolEntry*) * table->export_capacity);
    
    /* Imports */
    table->import_capacity = 8;
    table->import_count = 0;
    table->imports = malloc(sizeof(*table->imports) * table->import_capacity);
    
    return table;
}

void symbol_table_destroy(SymbolTable* table) {
    if (table == NULL) return;
    
    /* Libera entradas */
    for (size_t i = 0; i < table->bucket_count; i++) {
        SymbolEntry* entry = table->buckets[i];
        while (entry != NULL) {
            SymbolEntry* next = entry->next;
            free(entry->name);
            if (entry->type == SYMBOL_FUNCTION && entry->as.function.param_names) {
                for (int j = 0; j < entry->as.function.arity; j++) {
                    free(entry->as.function.param_names[j]);
                }
                free(entry->as.function.param_names);
            }
            free(entry);
            entry = next;
        }
    }
    free(table->buckets);
    
    /* Libera exports */
    free(table->exports);
    
    /* Libera imports */
    for (size_t i = 0; i < table->import_count; i++) {
        free(table->imports[i].module_name);
        for (size_t j = 0; j < table->imports[i].symbol_count; j++) {
            free(table->imports[i].symbols[j]);
        }
        free(table->imports[i].symbols);
    }
    free(table->imports);
    
    free(table);
}

SymbolEntry* symbol_table_add(SymbolTable* table, const char* name,
                               SymbolType type, SymbolVisibility vis) {
    if (table == NULL || name == NULL) return NULL;
    
    /* Verifica se já existe */
    SymbolEntry* existing = symbol_table_lookup(table, name);
    if (existing != NULL) {
        return existing; /* Já existe */
    }
    
    /* Cria nova entrada */
    SymbolEntry* entry = (SymbolEntry*)calloc(1, sizeof(SymbolEntry));
    if (entry == NULL) return NULL;
    
    entry->name = strdup(name);
    entry->type = type;
    entry->visibility = vis;
    entry->next = NULL;
    
    /* Insere na hash table */
    uint32_t hash = hash_string(name);
    size_t bucket = hash % table->bucket_count;
    
    entry->next = table->buckets[bucket];
    table->buckets[bucket] = entry;
    table->entry_count++;
    
    return entry;
}

SymbolEntry* symbol_table_lookup(SymbolTable* table, const char* name) {
    if (table == NULL || name == NULL) return NULL;
    
    uint32_t hash = hash_string(name);
    size_t bucket = hash % table->bucket_count;
    
    SymbolEntry* entry = table->buckets[bucket];
    while (entry != NULL) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

void symbol_table_export(SymbolTable* table, const char* name) {
    if (table == NULL || name == NULL) return;
    
    SymbolEntry* entry = symbol_table_lookup(table, name);
    if (entry == NULL) return;
    
    entry->visibility = SYMBOL_VIS_PUBLIC;
    
    /* Adiciona ao array de exports */
    if (table->export_count >= table->export_capacity) {
        table->export_capacity *= 2;
        table->exports = realloc(table->exports, 
            sizeof(SymbolEntry*) * table->export_capacity);
    }
    
    /* Verifica se já está na lista */
    for (size_t i = 0; i < table->export_count; i++) {
        if (table->exports[i] == entry) return;
    }
    
    table->exports[table->export_count++] = entry;
    
    if (entry->type == SYMBOL_FUNCTION) {
        entry->as.function.is_exported = 1;
    }
}

void symbol_table_add_import(SymbolTable* table, const char* module_name,
                              const char* symbol_name) {
    if (table == NULL || module_name == NULL) return;
    
    /* Procura módulo existente */
    size_t mod_idx = (size_t)-1;
    for (size_t i = 0; i < table->import_count; i++) {
        if (strcmp(table->imports[i].module_name, module_name) == 0) {
            mod_idx = i;
            break;
        }
    }
    
    /* Cria novo import se necessário */
    if (mod_idx == (size_t)-1) {
        if (table->import_count >= table->import_capacity) {
            table->import_capacity *= 2;
            table->imports = realloc(table->imports,
                sizeof(*table->imports) * table->import_capacity);
        }
        mod_idx = table->import_count++;
        table->imports[mod_idx].module_name = strdup(module_name);
        table->imports[mod_idx].symbols = malloc(sizeof(char*) * 8);
        table->imports[mod_idx].symbol_count = 0;
    }
    
    /* Adiciona símbolo se especificado */
    if (symbol_name != NULL) {
        size_t sym_count = table->imports[mod_idx].symbol_count;
        table->imports[mod_idx].symbols[sym_count] = strdup(symbol_name);
        table->imports[mod_idx].symbol_count++;
    }
}

void symbol_table_print(SymbolTable* table) {
    if (table == NULL) return;
    
    printf("\n=== Tabela de Símbolos ===\n");
    printf("  Total: %zu símbolos\n", table->entry_count);
    printf("  Exports: %zu\n", table->export_count);
    printf("  Imports: %zu módulos\n\n", table->import_count);
    
    /* Lista símbolos */
    printf("  Símbolos:\n");
    for (size_t i = 0; i < table->bucket_count; i++) {
        SymbolEntry* entry = table->buckets[i];
        while (entry != NULL) {
            const char* type_str = "???";
            switch (entry->type) {
                case SYMBOL_FUNCTION: type_str = "func"; break;
                case SYMBOL_VARIABLE: type_str = "var"; break;
                case SYMBOL_CONSTANT: type_str = "const"; break;
                case SYMBOL_TYPE:     type_str = "type"; break;
                case SYMBOL_MODULE:   type_str = "module"; break;
                case SYMBOL_NATIVE:   type_str = "native"; break;
            }
            
            const char* vis_str = entry->visibility == SYMBOL_VIS_PUBLIC ? "pub" : "priv";
            
            printf("    [%s] %s %s", vis_str, type_str, entry->name);
            
            if (entry->type == SYMBOL_FUNCTION) {
                printf("/%d", entry->as.function.arity);
            }
            printf("\n");
            
            entry = entry->next;
        }
    }
    
    /* Lista imports */
    if (table->import_count > 0) {
        printf("\n  Imports:\n");
        for (size_t i = 0; i < table->import_count; i++) {
            printf("    from %s import ", table->imports[i].module_name);
            if (table->imports[i].symbol_count == 0) {
                printf("*");
            } else {
                for (size_t j = 0; j < table->imports[i].symbol_count; j++) {
                    printf("%s%s", table->imports[i].symbols[j],
                           j < table->imports[i].symbol_count - 1 ? ", " : "");
                }
            }
            printf("\n");
        }
    }
    
    printf("==========================\n\n");
}

/* =============================================================================
 * COMPILAÇÃO E CARREGAMENTO DE .astm
 * ============================================================================= */

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) return NULL;
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    
    char* buffer = (char*)malloc(size + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }
    
    size_t read = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);
    
    return buffer;
}

static uint64_t get_file_mtime(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (uint64_t)st.st_mtime;
}

/* Coleta símbolos da AST */
static void collect_symbols_from_ast(ASTNode* node, SymbolTable* table) {
    if (node == NULL || table == NULL) return;
    
    switch (node->type) {
        case AST_FUNCTION_DECLARATION: {
            SymbolEntry* entry = symbol_table_add(table, 
                node->as.function_decl.name, SYMBOL_FUNCTION, SYMBOL_VIS_PRIVATE);
            if (entry) {
                entry->as.function.arity = (int)node->as.function_decl.parameter_count;
                entry->as.function.is_exported = 0;
                
                /* Copia nomes de parâmetros */
                if (node->as.function_decl.parameter_count > 0) {
                    entry->as.function.param_names = malloc(
                        sizeof(char*) * node->as.function_decl.parameter_count);
                    for (size_t i = 0; i < node->as.function_decl.parameter_count; i++) {
                        entry->as.function.param_names[i] = 
                            strdup(node->as.function_decl.parameters[i]);
                    }
                }
            }
            collect_symbols_from_ast(node->as.function_decl.body, table);
            break;
        }
        
        case AST_VARIABLE_DECLARATION: {
            symbol_table_add(table, node->as.variable_decl.name, 
                SYMBOL_VARIABLE, SYMBOL_VIS_PRIVATE);
            collect_symbols_from_ast(node->as.variable_decl.value, table);
            break;
        }
        
        case AST_IMPORT: {
            symbol_table_add_import(table, node->as.import_stmt.module_name, NULL);
            for (size_t i = 0; i < node->as.import_stmt.symbol_count; i++) {
                symbol_table_add_import(table, node->as.import_stmt.module_name,
                    node->as.import_stmt.symbols[i]);
            }
            break;
        }
        
        case AST_BLOCK: {
            for (size_t i = 0; i < node->as.block.count; i++) {
                collect_symbols_from_ast(node->as.block.statements[i], table);
            }
            break;
        }
        
        case AST_IF_STATEMENT:
            collect_symbols_from_ast(node->as.if_stmt.condition, table);
            collect_symbols_from_ast(node->as.if_stmt.then_branch, table);
            collect_symbols_from_ast(node->as.if_stmt.else_branch, table);
            break;
            
        case AST_WHILE_STATEMENT:
            collect_symbols_from_ast(node->as.while_stmt.condition, table);
            collect_symbols_from_ast(node->as.while_stmt.body, table);
            break;
            
        case AST_FOR_STATEMENT:
            collect_symbols_from_ast(node->as.for_stmt.init, table);
            collect_symbols_from_ast(node->as.for_stmt.condition, table);
            collect_symbols_from_ast(node->as.for_stmt.increment, table);
            collect_symbols_from_ast(node->as.for_stmt.body, table);
            break;
            
        default:
            break;
    }
}

CompiledModule* astm_compile(const char* source_path, const char* output_path) {
    if (source_path == NULL) return NULL;
    
    printf("[ASTM] Compilando: %s\n", source_path);
    
    /* Lê arquivo fonte */
    char* source = read_file(source_path);
    if (source == NULL) {
        fprintf(stderr, "[ASTM] Erro: Não foi possível ler %s\n", source_path);
        return NULL;
    }
    
    /* Fase 1: Lexer */
    Lexer* lexer = lexer_create(source);
    if (lexer == NULL) {
        free(source);
        return NULL;
    }
    
    /* Fase 2: Parser */
    Parser* parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        free(source);
        return NULL;
    }
    
    ASTNode* ast = parser_parse(parser);
    if (parser_had_error(parser) || ast == NULL) {
        fprintf(stderr, "[ASTM] Erro de parsing\n");
        if (ast) ast_destroy_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return NULL;
    }
    
    /* Fase 3: Type Check */
    if (typechecker_check(ast)) {
        fprintf(stderr, "[ASTM] Erro de tipo\n");
        ast_destroy_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return NULL;
    }
    
    /* Fase 4: Otimizações */
    ssa_transform(ast);
    
    /* Fase 5: Coleta símbolos */
    SymbolTable* symbols = symbol_table_create();
    collect_symbols_from_ast(ast, symbols);
    
    /* Fase 6: Compila para bytecode */
    BytecodeProgram* bytecode = compiler_compile(ast);
    if (bytecode == NULL) {
        fprintf(stderr, "[ASTM] Erro na compilação para bytecode\n");
        symbol_table_destroy(symbols);
        ast_destroy_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return NULL;
    }
    
    /* Cria módulo compilado */
    CompiledModule* module = (CompiledModule*)calloc(1, sizeof(CompiledModule));
    if (module == NULL) {
        bytecode_program_destroy(bytecode);
        symbol_table_destroy(symbols);
        ast_destroy_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return NULL;
    }
    
    /* Extrai nome do módulo do path */
    const char* filename = strrchr(source_path, '/');
    if (filename == NULL) filename = strrchr(source_path, '\\');
    filename = filename ? filename + 1 : source_path;
    
    char* name = strdup(filename);
    char* dot = strrchr(name, '.');
    if (dot) *dot = '\0';
    
    module->name = name;
    module->path = output_path ? strdup(output_path) : NULL;
    module->version = (ASTM_VERSION_MAJOR << 16) | ASTM_VERSION_MINOR;
    module->flags = ASTM_FLAG_NONE;
    module->symbols = symbols;
    module->bytecode = bytecode;
    module->jit_cache = NULL;
    module->jit_valid = 0;
    module->deps = NULL;
    module->dep_count = 0;
    module->source_mtime = get_file_mtime(source_path);
    module->compile_time = (uint64_t)time(NULL);
    module->ref_count = 1;
    
    /* Limpeza */
    ast_destroy_node(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    free(source);
    
    /* Salva se output_path especificado */
    if (output_path != NULL) {
        astm_save(module, output_path);
    }
    
    printf("[ASTM] Módulo '%s' compilado com sucesso\n", module->name);
    printf("[ASTM]   %zu símbolos, %zu instruções\n", 
           symbols->entry_count, bytecode->instruction_count);
    
    return module;
}

int astm_save(CompiledModule* module, const char* path) {
    if (module == NULL || path == NULL) return -1;
    
    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "[ASTM] Erro: Não foi possível criar %s\n", path);
        return -1;
    }
    
    /* Prepara header */
    AstmHeader header = {0};
    header.magic = ASTM_MAGIC;
    header.version_major = ASTM_VERSION_MAJOR;
    header.version_minor = ASTM_VERSION_MINOR;
    header.flags = module->flags;
    header.checksum = 0; /* TODO: Calcular CRC32 */
    
    /* Calcula offsets */
    size_t current_offset = sizeof(AstmHeader);
    
    header.symbol_offset = (uint32_t)current_offset;
    header.symbol_count = (uint32_t)module->symbols->entry_count;
    current_offset += header.symbol_count * sizeof(AstmSymbol);
    
    header.bytecode_offset = (uint32_t)current_offset;
    header.bytecode_size = (uint32_t)(module->bytecode->instruction_count * sizeof(Instruction));
    current_offset += header.bytecode_size;
    
    header.const_offset = (uint32_t)current_offset;
    header.const_count = (uint32_t)module->bytecode->constant_count;
    current_offset += header.const_count * sizeof(Constant);
    
    header.meta_offset = (uint32_t)current_offset;
    header.meta_size = 0; /* TODO: Metadados */
    
    /* Escreve header */
    fwrite(&header, sizeof(AstmHeader), 1, file);
    
    /* Escreve símbolos (simplificado) */
    for (size_t i = 0; i < module->symbols->bucket_count; i++) {
        SymbolEntry* entry = module->symbols->buckets[i];
        while (entry != NULL) {
            AstmSymbol sym = {0};
            sym.name_offset = 0; /* TODO: String table */
            sym.name_length = (uint16_t)strlen(entry->name);
            sym.type = (uint8_t)entry->type;
            sym.visibility = (uint8_t)entry->visibility;
            
            if (entry->type == SYMBOL_FUNCTION) {
                sym.arity = (uint16_t)entry->as.function.arity;
            }
            
            fwrite(&sym, sizeof(AstmSymbol), 1, file);
            entry = entry->next;
        }
    }
    
    /* Escreve bytecode */
    fwrite(module->bytecode->instructions, sizeof(Instruction),
           module->bytecode->instruction_count, file);
    
    /* Escreve constantes */
    fwrite(module->bytecode->constants, sizeof(Constant),
           module->bytecode->constant_count, file);
    
    fclose(file);
    
    printf("[ASTM] Salvo: %s (%zu bytes)\n", path, current_offset);
    
    return 0;
}

CompiledModule* astm_load(const char* path) {
    if (path == NULL) return NULL;
    
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "[ASTM] Erro: Não foi possível abrir %s\n", path);
        return NULL;
    }
    
    /* Lê header */
    AstmHeader header;
    if (fread(&header, sizeof(AstmHeader), 1, file) != 1) {
        fclose(file);
        return NULL;
    }
    
    /* Verifica magic */
    if (header.magic != ASTM_MAGIC) {
        fprintf(stderr, "[ASTM] Erro: Formato inválido\n");
        fclose(file);
        return NULL;
    }
    
    /* Verifica versão */
    if (header.version_major != ASTM_VERSION_MAJOR) {
        fprintf(stderr, "[ASTM] Erro: Versão incompatível (%d.%d)\n",
                header.version_major, header.version_minor);
        fclose(file);
        return NULL;
    }
    
    printf("[ASTM] Carregando: %s (v%d.%d)\n", path,
           header.version_major, header.version_minor);
    
    /* Cria módulo */
    CompiledModule* module = (CompiledModule*)calloc(1, sizeof(CompiledModule));
    if (module == NULL) {
        fclose(file);
        return NULL;
    }
    
    /* Cria tabela de símbolos */
    module->symbols = symbol_table_create();
    
    /* Lê símbolos */
    fseek(file, header.symbol_offset, SEEK_SET);
    for (uint32_t i = 0; i < header.symbol_count; i++) {
        AstmSymbol sym;
        fread(&sym, sizeof(AstmSymbol), 1, file);
        /* TODO: Reconstruir entrada completa */
    }
    
    /* Cria bytecode program */
    module->bytecode = (BytecodeProgram*)calloc(1, sizeof(BytecodeProgram));
    module->bytecode->instruction_count = header.bytecode_size / sizeof(Instruction);
    module->bytecode->instruction_capacity = module->bytecode->instruction_count;
    module->bytecode->instructions = malloc(header.bytecode_size);
    
    /* Lê bytecode */
    fseek(file, header.bytecode_offset, SEEK_SET);
    fread(module->bytecode->instructions, 1, header.bytecode_size, file);
    
    /* Lê constantes */
    module->bytecode->constant_count = header.const_count;
    module->bytecode->constant_capacity = header.const_count;
    module->bytecode->constants = malloc(sizeof(Constant) * header.const_count);
    
    fseek(file, header.const_offset, SEEK_SET);
    fread(module->bytecode->constants, sizeof(Constant), header.const_count, file);
    
    fclose(file);
    
    /* Extrai nome do path */
    const char* filename = strrchr(path, '/');
    if (filename == NULL) filename = strrchr(path, '\\');
    filename = filename ? filename + 1 : path;
    
    char* name = strdup(filename);
    char* dot = strrchr(name, '.');
    if (dot) *dot = '\0';
    
    module->name = name;
    module->path = strdup(path);
    module->version = (header.version_major << 16) | header.version_minor;
    module->flags = header.flags;
    module->ref_count = 1;
    
    printf("[ASTM] Módulo '%s' carregado\n", module->name);
    
    return module;
}

void astm_free(CompiledModule* module) {
    if (module == NULL) return;
    
    module->ref_count--;
    if (module->ref_count > 0) return;
    
    free(module->name);
    free(module->path);
    
    if (module->symbols) {
        symbol_table_destroy(module->symbols);
    }
    
    if (module->bytecode) {
        bytecode_program_destroy(module->bytecode);
    }
    
    if (module->deps) {
        for (size_t i = 0; i < module->dep_count; i++) {
            astm_free(module->deps[i]);
        }
        free(module->deps);
    }
    
    /* TODO: Liberar JIT cache */
    
    free(module);
}

int astm_is_valid(const char* astm_path, const char* source_path) {
    if (astm_path == NULL || source_path == NULL) return 0;
    
    uint64_t astm_mtime = get_file_mtime(astm_path);
    uint64_t source_mtime = get_file_mtime(source_path);
    
    if (astm_mtime == 0) return 0; /* .astm não existe */
    if (source_mtime == 0) return 1; /* Fonte não existe, usa cache */
    
    return astm_mtime >= source_mtime;
}

void astm_get_version(int* major, int* minor) {
    if (major) *major = ASTM_VERSION_MAJOR;
    if (minor) *minor = ASTM_VERSION_MINOR;
}

void astm_print_info(CompiledModule* module) {
    if (module == NULL) return;
    
    printf("\n=== Módulo Compilado ===\n");
    printf("  Nome: %s\n", module->name);
    printf("  Path: %s\n", module->path ? module->path : "(memória)");
    printf("  Versão: %d.%d\n", module->version >> 16, module->version & 0xFFFF);
    printf("  Flags: 0x%04X\n", module->flags);
    printf("  RefCount: %d\n", module->ref_count);
    
    if (module->bytecode) {
        printf("  Instruções: %zu\n", module->bytecode->instruction_count);
        printf("  Constantes: %zu\n", module->bytecode->constant_count);
    }
    
    printf("  JIT: %s\n", module->jit_valid ? "Sim" : "Não");
    printf("  Dependências: %zu\n", module->dep_count);
    
    if (module->symbols) {
        printf("  Símbolos: %zu\n", module->symbols->entry_count);
        printf("  Exports: %zu\n", module->symbols->export_count);
    }
    
    printf("========================\n\n");
}

