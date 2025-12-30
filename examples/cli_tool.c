/**
 * =============================================================================
 * EXEMPLO: Ferramenta CLI em Asteron
 * =============================================================================
 * 
 * Demonstra como usar o framework CLI do Asteron para criar
 * ferramentas de linha de comando ultra-rápidas.
 * 
 * Compile com:
 *   gcc -O3 -I src examples/cli_tool.c \
 *       obj/sys/runtime.o obj/sys/cli.o obj/memory/ownership.o \
 *       -o asteron-tool
 * 
 * Use:
 *   ./asteron-tool --help
 *   ./asteron-tool build arquivo.ast
 *   ./asteron-tool run arquivo.ast --verbose
 * 
 * =============================================================================
 */

#include "../src/sys/cli.h"
#include "../src/sys/runtime.h"
#include <string.h>

/* Handler para subcomando 'build' */
i32 cmd_build(CliApp* app, i32 argc, char** argv) {
    (void)argc; (void)argv;
    
    const char* input = cli_get_positional(app, 0);
    bool verbose = cli_get_flag(app, "--verbose");
    const char* output = cli_get_string(app, "--output");
    
    if (!input) {
        cli_error("Arquivo de entrada requerido");
        return 1;
    }
    
    if (verbose) {
        cli_info("Modo verbose ativado");
    }
    
    /* Simula compilação com progress bar */
    Spinner sp = spinner_new("Compilando...");
    
    for (int i = 0; i < 10; i++) {
        spinner_tick(&sp);
        struct TimeSpec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
        sys_nanosleep(&ts, 0);
    }
    
    spinner_success(&sp, "Compilação concluída!");
    
    if (output) {
        cli_info("Saída: %s");
        print_str("  -> ");
        print_str(output);
        print_str("\n");
    }
    
    return 0;
}

/* Handler para subcomando 'run' */
i32 cmd_run(CliApp* app, i32 argc, char** argv) {
    (void)argc; (void)argv;
    
    const char* input = cli_get_positional(app, 0);
    bool verbose = cli_get_flag(app, "--verbose");
    
    if (!input) {
        cli_error("Arquivo de entrada requerido");
        return 1;
    }
    
    cli_info("Executando...");
    print_str("  Arquivo: ");
    print_str(input);
    print_str("\n");
    
    if (verbose) {
        print_str("  Modo: verbose\n");
    }
    
    /* Simula execução */
    ProgressBar pb = progress_new(100, 30);
    pb.prefix = "  Progresso";
    
    for (usize i = 0; i <= 100; i += 5) {
        progress_update(&pb, i);
        struct TimeSpec ts = { .tv_sec = 0, .tv_nsec = 50000000 };
        sys_nanosleep(&ts, 0);
    }
    
    progress_finish(&pb);
    cli_success("Execução concluída!");
    
    return 0;
}

/* Handler para subcomando 'check' */
i32 cmd_check(CliApp* app, i32 argc, char** argv) {
    (void)argc; (void)argv;
    
    const char* input = cli_get_positional(app, 0);
    
    if (!input) {
        cli_error("Arquivo de entrada requerido");
        return 1;
    }
    
    cli_info("Verificando tipos e ownership...");
    
    /* Demonstra borrow checker */
    BorrowChecker* bc = borrow_checker_create();
    
    /* Simula algumas variáveis */
    MemoryLayout layout = { .size = 8, .location = MEM_STACK, .copy_type = COPY_TYPE_NONE };
    
    uint32_t x = borrow_checker_add_var(bc, "x", layout);
    uint32_t y = borrow_checker_add_var(bc, "y", layout);
    
    /* Simula borrow */
    borrow_checker_borrow(bc, x, 1);
    
    /* Tenta mut borrow (deve falhar) */
    OwnershipError err = borrow_checker_borrow_mut(bc, x, 1);
    if (err != OWN_ERR_NONE) {
        cli_warn("Detectado: %s");
        print_str("  ");
        print_str(ownership_error_message(err));
        print_str("\n");
    }
    
    borrow_checker_unborrow(bc, x);
    
    /* Move */
    borrow_checker_move(bc, x, y);
    
    /* Uso após move (deve falhar) */
    err = borrow_checker_borrow(bc, x, 1);
    if (err != OWN_ERR_NONE) {
        cli_warn("Detectado: %s");
        print_str("  ");
        print_str(ownership_error_message(err));
        print_str("\n");
    }
    
    borrow_checker_print_state(bc);
    
    if (borrow_checker_verify(bc)) {
        cli_success("Nenhum erro de ownership!");
    } else {
        cli_error("Erros de ownership encontrados");
        borrow_checker_destroy(bc);
        return 1;
    }
    
    borrow_checker_destroy(bc);
    return 0;
}

/* Handler principal */
i32 main_handler(CliApp* app, i32 argc, char** argv) {
    (void)argc; (void)argv;
    
    /* Sem subcomando - mostra info */
    print_str("\n");
    print_str(ANSI_BOLD ANSI_CYAN);
    print_str("  █████╗ ███████╗████████╗███████╗██████╗  ██████╗ ███╗   ██╗\n");
    print_str(" ██╔══██╗██╔════╝╚══██╔══╝██╔════╝██╔══██╗██╔═══██╗████╗  ██║\n");
    print_str(" ███████║███████╗   ██║   █████╗  ██████╔╝██║   ██║██╔██╗ ██║\n");
    print_str(" ██╔══██║╚════██║   ██║   ██╔══╝  ██╔══██╗██║   ██║██║╚██╗██║\n");
    print_str(" ██║  ██║███████║   ██║   ███████╗██║  ██║╚██████╔╝██║ ╚████║\n");
    print_str(" ╚═╝  ╚═╝╚══════╝   ╚═╝   ╚══════╝╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═══╝\n");
    print_str(ANSI_RESET);
    print_str("\n");
    print_str("  Sistema de Programação de Sistemas\n");
    print_str("  Ownership + Zero-Cost + Syscalls Diretas\n\n");
    
    cli_print_help(app);
    
    return 0;
}

/* Entry point */
i32 asteron_main(i32 argc, char** argv) {
    CliApp* app = CLI_APP("asteron-tool", "Ferramenta de desenvolvimento Asteron")
        ->version = "1.0.0",
        app;
    
    cli_version(app, "1.0.0");
    cli_author(app, "Asteron Team");
    
    /* Flags globais */
    cli_flag(app, "-v", "--verbose", "Saída detalhada");
    cli_option(app, "-o", "--output", "Arquivo de saída", "FILE");
    cli_option_int(app, "-j", "--jobs", "Número de threads", 4);
    
    /* Argumentos posicionais */
    cli_arg(app, "input", "Arquivo de entrada (.ast)");
    
    /* Subcomandos */
    cli_subcommand(app, "build", "Compila um módulo Asteron", cmd_build);
    cli_subcommand(app, "run", "Executa um programa Asteron", cmd_run);
    cli_subcommand(app, "check", "Verifica tipos e ownership", cmd_check);
    
    cli_handler(app, main_handler);
    
    return cli_run(app, argc, argv);
}

/* Usa macro para setup do runtime */
ASTERON_MAIN(asteron_main)

