/**
 * =============================================================================
 * ASTERON CLI FRAMEWORK v1.0
 * =============================================================================
 * 
 * Framework para construção de ferramentas de linha de comando ultra-rápidas.
 * 
 * Características:
 * - Zero allocations para parsing básico
 * - Argument parsing type-safe
 * - Subcomandos
 * - Auto-generated help
 * - Color output
 * 
 * Uso:
 *   CLI_APP("myapp", "Descrição do app")
 *       .version("1.0.0")
 *       .flag("-v", "--verbose", "Modo verbose")
 *       .option("-o", "--output", "Arquivo de saída", "FILE")
 *       .arg("input", "Arquivo de entrada")
 *       .run(main_handler);
 * 
 * =============================================================================
 */

#ifndef ASTERON_CLI_H
#define ASTERON_CLI_H

#include "runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * CORES ANSI
 * ============================================================================= */

#define ANSI_RESET      "\x1b[0m"
#define ANSI_BOLD       "\x1b[1m"
#define ANSI_DIM        "\x1b[2m"
#define ANSI_RED        "\x1b[31m"
#define ANSI_GREEN      "\x1b[32m"
#define ANSI_YELLOW     "\x1b[33m"
#define ANSI_BLUE       "\x1b[34m"
#define ANSI_MAGENTA    "\x1b[35m"
#define ANSI_CYAN       "\x1b[36m"
#define ANSI_WHITE      "\x1b[37m"

/* =============================================================================
 * TIPOS DE ARGUMENTO
 * ============================================================================= */

typedef enum {
    ARG_FLAG,           /* -v, --verbose (boolean) */
    ARG_OPTION,         /* -o FILE, --output FILE */
    ARG_POSITIONAL,     /* <input> */
    ARG_SUBCOMMAND      /* build, run, test */
} ArgType;

typedef enum {
    VAL_NONE,           /* Flag sem valor */
    VAL_STRING,         /* String */
    VAL_INT,            /* Inteiro */
    VAL_FLOAT,          /* Float */
    VAL_PATH            /* Caminho de arquivo */
} ArgValueType;

/* =============================================================================
 * DEFINIÇÃO DE ARGUMENTO
 * ============================================================================= */

typedef struct {
    ArgType type;
    const char* short_name;     /* -v */
    const char* long_name;      /* --verbose */
    const char* description;    /* Descrição para help */
    const char* value_name;     /* Nome do valor (FILE, PATH, etc) */
    ArgValueType value_type;
    bool required;
    const char* default_value;
    
    /* Valores parseados */
    bool present;               /* Foi fornecido? */
    union {
        bool flag;
        const char* string;
        i64 integer;
        f64 floating;
    } value;
} CliArg;

/* =============================================================================
 * SUBCOMANDO
 * ============================================================================= */

struct CliApp;

typedef i32 (*CliHandler)(struct CliApp* app, i32 argc, char** argv);

typedef struct CliSubcommand {
    const char* name;
    const char* description;
    CliArg* args;
    usize arg_count;
    CliHandler handler;
    struct CliSubcommand* next;
} CliSubcommand;

/* =============================================================================
 * APLICAÇÃO CLI
 * ============================================================================= */

typedef struct CliApp {
    const char* name;
    const char* description;
    const char* version;
    const char* author;
    
    /* Argumentos globais */
    CliArg* args;
    usize arg_count;
    usize arg_capacity;
    
    /* Subcomandos */
    CliSubcommand* subcommands;
    CliSubcommand* active_subcommand;
    
    /* Estado */
    i32 argc;
    char** argv;
    bool parsed;
    bool help_requested;
    bool version_requested;
    
    /* Handler principal */
    CliHandler main_handler;
    
    /* Argumentos posicionais extras */
    char** positional_args;
    usize positional_count;
} CliApp;

/* =============================================================================
 * API DE CONSTRUÇÃO
 * ============================================================================= */

/**
 * Cria nova aplicação CLI
 */
CliApp* cli_app(const char* name, const char* description);

/**
 * Define versão
 */
CliApp* cli_version(CliApp* app, const char* version);

/**
 * Define autor
 */
CliApp* cli_author(CliApp* app, const char* author);

/**
 * Adiciona flag (-v, --verbose)
 */
CliApp* cli_flag(CliApp* app, const char* short_name, const char* long_name,
                  const char* description);

/**
 * Adiciona opção com valor (-o FILE)
 */
CliApp* cli_option(CliApp* app, const char* short_name, const char* long_name,
                    const char* description, const char* value_name);

/**
 * Adiciona opção inteira
 */
CliApp* cli_option_int(CliApp* app, const char* short_name, const char* long_name,
                        const char* description, i64 default_value);

/**
 * Adiciona argumento posicional
 */
CliApp* cli_arg(CliApp* app, const char* name, const char* description);

/**
 * Adiciona argumento posicional requerido
 */
CliApp* cli_arg_required(CliApp* app, const char* name, const char* description);

/**
 * Adiciona subcomando
 */
CliApp* cli_subcommand(CliApp* app, const char* name, const char* description,
                        CliHandler handler);

/**
 * Define handler principal
 */
CliApp* cli_handler(CliApp* app, CliHandler handler);

/* =============================================================================
 * API DE EXECUÇÃO
 * ============================================================================= */

/**
 * Parseia argumentos
 */
bool cli_parse(CliApp* app, i32 argc, char** argv);

/**
 * Executa aplicação
 */
i32 cli_run(CliApp* app, i32 argc, char** argv);

/**
 * Obtém valor de flag
 */
bool cli_get_flag(CliApp* app, const char* name);

/**
 * Obtém valor de opção string
 */
const char* cli_get_string(CliApp* app, const char* name);

/**
 * Obtém valor de opção inteira
 */
i64 cli_get_int(CliApp* app, const char* name);

/**
 * Obtém argumento posicional
 */
const char* cli_get_positional(CliApp* app, usize index);

/**
 * Número de argumentos posicionais
 */
usize cli_positional_count(CliApp* app);

/* =============================================================================
 * API DE SAÍDA
 * ============================================================================= */

/**
 * Imprime help
 */
void cli_print_help(CliApp* app);

/**
 * Imprime versão
 */
void cli_print_version(CliApp* app);

/**
 * Imprime erro
 */
void cli_error(const char* fmt, ...);

/**
 * Imprime warning
 */
void cli_warn(const char* fmt, ...);

/**
 * Imprime info
 */
void cli_info(const char* fmt, ...);

/**
 * Imprime sucesso
 */
void cli_success(const char* fmt, ...);

/* =============================================================================
 * PROGRESS BAR
 * ============================================================================= */

typedef struct {
    usize total;
    usize current;
    usize width;
    const char* prefix;
    bool show_percent;
    bool show_count;
} ProgressBar;

/**
 * Cria progress bar
 */
ProgressBar progress_new(usize total, usize width);

/**
 * Atualiza progress bar
 */
void progress_update(ProgressBar* pb, usize current);

/**
 * Incrementa progress bar
 */
void progress_inc(ProgressBar* pb);

/**
 * Finaliza progress bar
 */
void progress_finish(ProgressBar* pb);

/* =============================================================================
 * SPINNER
 * ============================================================================= */

typedef struct {
    const char** frames;
    usize frame_count;
    usize current_frame;
    const char* message;
} Spinner;

/**
 * Cria spinner
 */
Spinner spinner_new(const char* message);

/**
 * Atualiza spinner (chame em loop)
 */
void spinner_tick(Spinner* sp);

/**
 * Para spinner com sucesso
 */
void spinner_success(Spinner* sp, const char* message);

/**
 * Para spinner com erro
 */
void spinner_error(Spinner* sp, const char* message);

/* =============================================================================
 * MACROS DE CONVENIÊNCIA
 * ============================================================================= */

/**
 * Macro para criar app rapidamente
 */
#define CLI_APP(name, desc) cli_app(name, desc)

/**
 * Macro para definir e executar CLI
 */
#define CLI_MAIN(app, argc, argv) \
    do { \
        i32 __cli_result = cli_run(app, argc, argv); \
        if (__cli_result != 0) return __cli_result; \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* ASTERON_CLI_H */

