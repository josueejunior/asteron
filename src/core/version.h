/**
 * =============================================================================
 * ASTERON CORE VERSION - FROZEN
 * =============================================================================
 * 
 * Este arquivo marca a versão do Core Asteron.
 * 
 * STATUS: CONGELADO (FROZEN)
 * 
 * Após o congelamento:
 * - A ABI é estável e não pode quebrar compatibilidade
 * - Novos recursos são adicionados de forma aditiva
 * - Bugs são corrigidos sem alterar a interface
 * 
 * =============================================================================
 */

#ifndef ASTERON_VERSION_H
#define ASTERON_VERSION_H

/* Versão do Core */
#define ASTERON_VERSION_MAJOR 1
#define ASTERON_VERSION_MINOR 0
#define ASTERON_VERSION_PATCH 0

/* String de versão */
#define ASTERON_VERSION_STRING "1.0.0"

/* Status do Core */
#define ASTERON_CORE_STATUS "FROZEN"
#define ASTERON_CORE_FROZEN 1

/* Data do congelamento */
#define ASTERON_FREEZE_DATE "2025-12-19"

/* =============================================================================
 * FEATURE FLAGS
 * ============================================================================= */

/* Recursos disponíveis nesta versão */
#define ASTERON_FEATURE_LEXER           1   /* Análise léxica */
#define ASTERON_FEATURE_PARSER          1   /* Parser */
#define ASTERON_FEATURE_AST             1   /* Abstract Syntax Tree */
#define ASTERON_FEATURE_TYPECHECKER     1   /* Verificação de tipos */
#define ASTERON_FEATURE_INTERPRETER     1   /* Interpretador direto */
#define ASTERON_FEATURE_VM              1   /* Virtual Machine */
#define ASTERON_FEATURE_BYTECODE        1   /* Compilação para bytecode */
#define ASTERON_FEATURE_JIT             1   /* Just-In-Time compilation */
#define ASTERON_FEATURE_SSA             1   /* Forma SSA */
#define ASTERON_FEATURE_ESCAPE_ANALYSIS 1   /* Análise de escape */
#define ASTERON_FEATURE_INLINE          1   /* Inlining de funções */
#define ASTERON_FEATURE_REG_ALLOC       1   /* Alocação de registradores */

/* =============================================================================
 * COMPATIBILIDADE
 * ============================================================================= */

/**
 * Macro para verificar compatibilidade de versão
 * Uso: #if ASTERON_VERSION_CHECK(1, 0, 0)
 */
#define ASTERON_VERSION_CHECK(major, minor, patch) \
    ((ASTERON_VERSION_MAJOR > (major)) || \
     (ASTERON_VERSION_MAJOR == (major) && ASTERON_VERSION_MINOR > (minor)) || \
     (ASTERON_VERSION_MAJOR == (major) && ASTERON_VERSION_MINOR == (minor) && ASTERON_VERSION_PATCH >= (patch)))

/**
 * Versão como número único (para comparações)
 * Formato: XXYYZZ onde XX=major, YY=minor, ZZ=patch
 */
#define ASTERON_VERSION_NUMBER \
    (ASTERON_VERSION_MAJOR * 10000 + ASTERON_VERSION_MINOR * 100 + ASTERON_VERSION_PATCH)

/* =============================================================================
 * MÓDULOS DO CORE (CONGELADOS)
 * ============================================================================= */

/*
 * Lista de módulos que fazem parte do Core:
 * 
 * src/core/
 * ├── abi.h              - Interface Binária de Aplicação (ABI)
 * ├── version.h          - Este arquivo
 * ├── common.h           - Tipos comuns
 * ├── ast/               - Abstract Syntax Tree
 * │   ├── ast.h
 * │   └── ast.c
 * ├── lexer/             - Análise Léxica
 * │   ├── lexer.h
 * │   └── lexer.c
 * ├── parser/            - Parser
 * │   ├── parser.h
 * │   └── parser.c
 * ├── typechecker/       - Verificação de Tipos
 * │   ├── typechecker.h
 * │   └── typechecker.c
 * ├── interpreter/       - Interpretador Direto
 * │   ├── interpreter.h
 * │   └── interpreter.c
 * ├── optimizer/         - Otimizações
 * │   ├── ssa.h/.c       - Forma SSA
 * │   ├── escape.h/.c    - Análise de Escape
 * │   ├── inline.h/.c    - Inlining
 * │   └── reg_alloc.h/.c - Alocação de Registradores
 * ├── vm/                - Virtual Machine
 * │   ├── vm.h
 * │   ├── vm.c
 * │   └── compiler.c     - Compilador AST -> Bytecode
 * └── jit/               - Just-In-Time
 *     ├── jit.h
 *     └── jit.c
 * 
 * NOTA: Módulos fora de src/core/ NÃO são parte do core congelado:
 * - src/graph/           - Análise de grafos (extensão)
 * - src/scheduler/       - Scheduler paralelo (extensão)
 * - src/graph_declarative/ - Grafos declarativos (extensão)
 */

#endif /* ASTERON_VERSION_H */

