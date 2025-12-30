/**
 * =============================================================================
 * ASTERON CLI FRAMEWORK - Implementação
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include "cli.h"
#include <string.h>
#include <stdarg.h>

/* =============================================================================
 * HELPERS
 * ============================================================================= */

static void print(const char* s) {
    print_str(s);
}

static void println(const char* s) {
    print_str(s);
    print_str("\n");
}

static bool str_equals(const char* a, const char* b) {
    if (a == b) return true;
    if (a == 0 || b == 0) return false;
    while (*a && *b) {
        if (*a++ != *b++) return false;
    }
    return *a == *b;
}

static i64 parse_int(const char* s) {
    i64 result = 0;
    i64 sign = 1;
    
    if (*s == '-') {
        sign = -1;
        s++;
    }
    
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    
    return result * sign;
}

/* =============================================================================
 * CONSTRUÇÃO
 * ============================================================================= */

CliApp* cli_app(const char* name, const char* description) {
    CliApp* app = (CliApp*)rt_alloc(sizeof(CliApp));
    memset(app, 0, sizeof(CliApp));
    
    app->name = name;
    app->description = description;
    app->version = "0.1.0";
    
    app->arg_capacity = 16;
    app->args = (CliArg*)rt_alloc(sizeof(CliArg) * app->arg_capacity);
    
    /* Adiciona -h/--help automaticamente */
    cli_flag(app, "-h", "--help", "Mostra esta mensagem de ajuda");
    
    return app;
}

CliApp* cli_version(CliApp* app, const char* version) {
    app->version = version;
    
    /* Adiciona -V/--version */
    cli_flag(app, "-V", "--version", "Mostra a versão");
    
    return app;
}

CliApp* cli_author(CliApp* app, const char* author) {
    app->author = author;
    return app;
}

static CliArg* add_arg(CliApp* app) {
    if (app->arg_count >= app->arg_capacity) {
        usize new_cap = app->arg_capacity * 2;
        CliArg* new_args = (CliArg*)rt_alloc(sizeof(CliArg) * new_cap);
        memcpy(new_args, app->args, sizeof(CliArg) * app->arg_count);
        app->args = new_args;
        app->arg_capacity = new_cap;
    }
    
    CliArg* arg = &app->args[app->arg_count++];
    memset(arg, 0, sizeof(CliArg));
    return arg;
}

CliApp* cli_flag(CliApp* app, const char* short_name, const char* long_name,
                  const char* description) {
    CliArg* arg = add_arg(app);
    arg->type = ARG_FLAG;
    arg->short_name = short_name;
    arg->long_name = long_name;
    arg->description = description;
    arg->value_type = VAL_NONE;
    return app;
}

CliApp* cli_option(CliApp* app, const char* short_name, const char* long_name,
                    const char* description, const char* value_name) {
    CliArg* arg = add_arg(app);
    arg->type = ARG_OPTION;
    arg->short_name = short_name;
    arg->long_name = long_name;
    arg->description = description;
    arg->value_name = value_name;
    arg->value_type = VAL_STRING;
    return app;
}

CliApp* cli_option_int(CliApp* app, const char* short_name, const char* long_name,
                        const char* description, i64 default_value) {
    CliArg* arg = add_arg(app);
    arg->type = ARG_OPTION;
    arg->short_name = short_name;
    arg->long_name = long_name;
    arg->description = description;
    arg->value_name = "NUM";
    arg->value_type = VAL_INT;
    arg->value.integer = default_value;
    return app;
}

CliApp* cli_arg(CliApp* app, const char* name, const char* description) {
    CliArg* arg = add_arg(app);
    arg->type = ARG_POSITIONAL;
    arg->long_name = name;
    arg->description = description;
    arg->required = false;
    return app;
}

CliApp* cli_arg_required(CliApp* app, const char* name, const char* description) {
    CliArg* arg = add_arg(app);
    arg->type = ARG_POSITIONAL;
    arg->long_name = name;
    arg->description = description;
    arg->required = true;
    return app;
}

CliApp* cli_subcommand(CliApp* app, const char* name, const char* description,
                        CliHandler handler) {
    CliSubcommand* sub = (CliSubcommand*)rt_alloc(sizeof(CliSubcommand));
    sub->name = name;
    sub->description = description;
    sub->handler = handler;
    sub->args = 0;
    sub->arg_count = 0;
    sub->next = app->subcommands;
    app->subcommands = sub;
    return app;
}

CliApp* cli_handler(CliApp* app, CliHandler handler) {
    app->main_handler = handler;
    return app;
}

/* =============================================================================
 * PARSING
 * ============================================================================= */

static CliArg* find_arg(CliApp* app, const char* name) {
    for (usize i = 0; i < app->arg_count; i++) {
        CliArg* arg = &app->args[i];
        if ((arg->short_name && str_equals(arg->short_name, name)) ||
            (arg->long_name && str_equals(arg->long_name, name))) {
            return arg;
        }
    }
    return 0;
}

bool cli_parse(CliApp* app, i32 argc, char** argv) {
    app->argc = argc;
    app->argv = argv;
    
    /* Aloca array para posicionais */
    app->positional_args = (char**)rt_alloc(sizeof(char*) * argc);
    app->positional_count = 0;
    
    bool after_double_dash = false;
    
    for (i32 i = 1; i < argc; i++) {
        char* arg = argv[i];
        
        /* -- significa fim das opções */
        if (str_equals(arg, "--")) {
            after_double_dash = true;
            continue;
        }
        
        if (after_double_dash || arg[0] != '-') {
            /* Verifica se é subcomando */
            if (!after_double_dash && app->positional_count == 0) {
                CliSubcommand* sub = app->subcommands;
                while (sub) {
                    if (str_equals(sub->name, arg)) {
                        app->active_subcommand = sub;
                        /* Passa resto dos args pro subcomando */
                        return true;
                    }
                    sub = sub->next;
                }
            }
            
            /* Argumento posicional */
            app->positional_args[app->positional_count++] = arg;
            continue;
        }
        
        /* Flag ou opção */
        CliArg* cli_arg = find_arg(app, arg);
        
        if (cli_arg == 0) {
            cli_error("Argumento desconhecido: %s", arg);
            return false;
        }
        
        cli_arg->present = true;
        
        if (cli_arg->type == ARG_FLAG) {
            cli_arg->value.flag = true;
            
            /* Verifica flags especiais */
            if (str_equals(arg, "-h") || str_equals(arg, "--help")) {
                app->help_requested = true;
            }
            if (str_equals(arg, "-V") || str_equals(arg, "--version")) {
                app->version_requested = true;
            }
        } else if (cli_arg->type == ARG_OPTION) {
            /* Precisa de valor */
            if (i + 1 >= argc) {
                cli_error("Opção %s requer um valor", arg);
                return false;
            }
            
            i++;
            char* value = argv[i];
            
            if (cli_arg->value_type == VAL_INT) {
                cli_arg->value.integer = parse_int(value);
            } else {
                cli_arg->value.string = value;
            }
        }
    }
    
    app->parsed = true;
    return true;
}

i32 cli_run(CliApp* app, i32 argc, char** argv) {
    if (!cli_parse(app, argc, argv)) {
        cli_print_help(app);
        return 1;
    }
    
    if (app->help_requested) {
        cli_print_help(app);
        return 0;
    }
    
    if (app->version_requested) {
        cli_print_version(app);
        return 0;
    }
    
    /* Executa subcomando se ativo */
    if (app->active_subcommand && app->active_subcommand->handler) {
        return app->active_subcommand->handler(app, argc, argv);
    }
    
    /* Executa handler principal */
    if (app->main_handler) {
        return app->main_handler(app, argc, argv);
    }
    
    /* Sem handler - mostra help */
    cli_print_help(app);
    return 0;
}

/* =============================================================================
 * GETTERS
 * ============================================================================= */

bool cli_get_flag(CliApp* app, const char* name) {
    CliArg* arg = find_arg(app, name);
    return arg ? arg->value.flag : false;
}

const char* cli_get_string(CliApp* app, const char* name) {
    CliArg* arg = find_arg(app, name);
    return arg ? arg->value.string : 0;
}

i64 cli_get_int(CliApp* app, const char* name) {
    CliArg* arg = find_arg(app, name);
    return arg ? arg->value.integer : 0;
}

const char* cli_get_positional(CliApp* app, usize index) {
    if (index >= app->positional_count) return 0;
    return app->positional_args[index];
}

usize cli_positional_count(CliApp* app) {
    return app->positional_count;
}

/* =============================================================================
 * OUTPUT
 * ============================================================================= */

void cli_print_help(CliApp* app) {
    /* Header */
    print(ANSI_BOLD);
    print(app->name);
    print(ANSI_RESET);
    
    if (app->version) {
        print(" ");
        print(app->version);
    }
    
    println("");
    
    if (app->description) {
        println(app->description);
    }
    
    println("");
    
    /* Usage */
    print(ANSI_YELLOW "USAGE:" ANSI_RESET "\n    ");
    print(app->name);
    print(" [OPTIONS]");
    
    /* Subcomandos */
    if (app->subcommands) {
        print(" <COMMAND>");
    }
    
    /* Posicionais */
    for (usize i = 0; i < app->arg_count; i++) {
        CliArg* arg = &app->args[i];
        if (arg->type == ARG_POSITIONAL) {
            print(" ");
            if (!arg->required) print("[");
            print("<");
            print(arg->long_name);
            print(">");
            if (!arg->required) print("]");
        }
    }
    
    println("\n");
    
    /* Opções */
    print(ANSI_YELLOW "OPTIONS:" ANSI_RESET "\n");
    
    for (usize i = 0; i < app->arg_count; i++) {
        CliArg* arg = &app->args[i];
        if (arg->type == ARG_FLAG || arg->type == ARG_OPTION) {
            print("    ");
            
            if (arg->short_name) {
                print(ANSI_GREEN);
                print(arg->short_name);
                print(ANSI_RESET);
                if (arg->long_name) print(", ");
            }
            
            if (arg->long_name) {
                print(ANSI_GREEN);
                print(arg->long_name);
                print(ANSI_RESET);
            }
            
            if (arg->value_name) {
                print(" <");
                print(arg->value_name);
                print(">");
            }
            
            println("");
            
            if (arg->description) {
                print("            ");
                println(arg->description);
            }
        }
    }
    
    /* Subcomandos */
    if (app->subcommands) {
        println("");
        print(ANSI_YELLOW "COMMANDS:" ANSI_RESET "\n");
        
        CliSubcommand* sub = app->subcommands;
        while (sub) {
            print("    ");
            print(ANSI_GREEN);
            print(sub->name);
            print(ANSI_RESET);
            println("");
            
            if (sub->description) {
                print("            ");
                println(sub->description);
            }
            
            sub = sub->next;
        }
    }
}

void cli_print_version(CliApp* app) {
    print(app->name);
    print(" ");
    println(app->version ? app->version : "0.0.0");
}

void cli_error(const char* fmt, ...) {
    print(ANSI_RED ANSI_BOLD "error" ANSI_RESET ": ");
    println(fmt);
}

void cli_warn(const char* fmt, ...) {
    print(ANSI_YELLOW ANSI_BOLD "warning" ANSI_RESET ": ");
    println(fmt);
}

void cli_info(const char* fmt, ...) {
    print(ANSI_BLUE ANSI_BOLD "info" ANSI_RESET ": ");
    println(fmt);
}

void cli_success(const char* fmt, ...) {
    print(ANSI_GREEN ANSI_BOLD "success" ANSI_RESET ": ");
    println(fmt);
}

/* =============================================================================
 * PROGRESS BAR
 * ============================================================================= */

ProgressBar progress_new(usize total, usize width) {
    return (ProgressBar){
        .total = total,
        .current = 0,
        .width = width > 0 ? width : 40,
        .prefix = "",
        .show_percent = true,
        .show_count = false
    };
}

void progress_update(ProgressBar* pb, usize current) {
    pb->current = current;
    
    usize filled = (pb->current * pb->width) / pb->total;
    
    print("\r");
    if (pb->prefix[0]) {
        print(pb->prefix);
        print(" ");
    }
    
    print("[");
    print(ANSI_GREEN);
    
    for (usize i = 0; i < pb->width; i++) {
        if (i < filled) {
            print("█");
        } else {
            print(" ");
        }
    }
    
    print(ANSI_RESET "] ");
    
    if (pb->show_percent) {
        usize percent = (pb->current * 100) / pb->total;
        /* Print percent - simplified */
        char buf[8];
        buf[0] = '0' + (percent / 100) % 10;
        buf[1] = '0' + (percent / 10) % 10;
        buf[2] = '0' + percent % 10;
        buf[3] = '%';
        buf[4] = 0;
        if (buf[0] == '0') {
            if (buf[1] == '0') {
                print(buf + 2);
            } else {
                print(buf + 1);
            }
        } else {
            print(buf);
        }
    }
}

void progress_inc(ProgressBar* pb) {
    progress_update(pb, pb->current + 1);
}

void progress_finish(ProgressBar* pb) {
    progress_update(pb, pb->total);
    println("");
}

/* =============================================================================
 * SPINNER
 * ============================================================================= */

static const char* SPINNER_FRAMES[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
#define SPINNER_FRAME_COUNT 10

Spinner spinner_new(const char* message) {
    return (Spinner){
        .frames = SPINNER_FRAMES,
        .frame_count = SPINNER_FRAME_COUNT,
        .current_frame = 0,
        .message = message
    };
}

void spinner_tick(Spinner* sp) {
    print("\r");
    print(ANSI_CYAN);
    print(sp->frames[sp->current_frame % sp->frame_count]);
    print(ANSI_RESET " ");
    print(sp->message);
    
    sp->current_frame++;
}

void spinner_success(Spinner* sp, const char* message) {
    print("\r" ANSI_GREEN "✓" ANSI_RESET " ");
    println(message ? message : sp->message);
}

void spinner_error(Spinner* sp, const char* message) {
    print("\r" ANSI_RED "✗" ANSI_RESET " ");
    println(message ? message : sp->message);
}

