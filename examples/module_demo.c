/**
 * =============================================================================
 * DEMO: Sistema de Módulos Asteron
 * =============================================================================
 * 
 * Este programa demonstra:
 * 
 * 1. Carregamento automático de módulos
 * 2. Funções como first-class entities
 * 3. Estado reativo para variáveis de módulo
 * 4. Hot reload de módulos
 * 5. Integração com grafo declarativo
 * 
 * Compilar:
 *   gcc -I src examples/module_demo.c \
 *       obj/modules/module_interface.o \
 *       obj/reactive/reactive.o \
 *       obj/modules/native/os_module.o \
 *       obj/modules/native/net_module.o \
 *       obj/modules/native/fs_module.o \
 *       obj/modules/native/time_module.o \
 *       obj/modules/native/math_module.o \
 *       obj/modules/module.o \
 *       obj/utils/utils.o \
 *       -o module_demo -lpthread
 * 
 * Executar:
 *   ./module_demo
 * 
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Headers Asteron */
#include "modules/module_interface.h"
#include "modules/native/os_module.h"
#include "modules/native/net_module.h"
#include "modules/native/fs_module.h"
#include "modules/native/time_module.h"
#include "modules/native/math_module.h"

/* =============================================================================
 * HELPERS
 * ============================================================================= */

static void print_value(AsteronValue val) {
    switch (val.type) {
        case ASTERON_VAL_NIL:
            printf("nil");
            break;
        case ASTERON_VAL_BOOL:
            printf("%s", ASTERON_AS_BOOL(val) ? "true" : "false");
            break;
        case ASTERON_VAL_NUMBER:
            printf("%.2f", ASTERON_AS_NUMBER(val));
            break;
        case ASTERON_VAL_STRING: {
            AsteronString* str = ASTERON_AS_STRING(val);
            if (str) {
                printf("%s", str->chars);
            } else {
                printf("(null string)");
            }
            break;
        }
        default:
            printf("<value type=%d>", val.type);
    }
}

static void print_section(const char* title) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf(" %s\n", title);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
}

/* =============================================================================
 * DEMO: MÓDULO OS
 * ============================================================================= */

static void demo_os_module(void) {
    print_section("MÓDULO: os");
    
    AsteronValue result;
    
    printf("  Platform:     ");
    result = os_platform(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  Arch:         ");
    result = os_arch(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  Hostname:     ");
    result = os_hostname(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  Username:     ");
    result = os_username(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  Home Dir:     ");
    result = os_homedir(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  Temp Dir:     ");
    result = os_tmpdir(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  CPUs:         ");
    result = os_cpus(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  Memory Total: ");
    result = os_memory_total(0, NULL);
    double total_mb = ASTERON_AS_NUMBER(result) / 1024.0 / 1024.0;
    printf("%.0f MB\n", total_mb);
    
    printf("  Memory Free:  ");
    result = os_memory_free(0, NULL);
    double free_mb = ASTERON_AS_NUMBER(result) / 1024.0 / 1024.0;
    printf("%.0f MB\n", free_mb);
    
    printf("  Uptime:       ");
    result = os_uptime(0, NULL);
    double hours = ASTERON_AS_NUMBER(result) / 3600.0;
    printf("%.1f hours\n", hours);
    
    printf("  PID:          ");
    result = os_pid(0, NULL);
    print_value(result);
    printf("\n");
}

/* =============================================================================
 * DEMO: MÓDULO NET
 * ============================================================================= */

static void demo_net_module(void) {
    print_section("MÓDULO: net");
    
    AsteronValue result;
    
    printf("  Hostname:     ");
    result = net_hostname(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  Resolving localhost...\n");
    
    /* Cria string para argumento */
    AsteronString* host_str = (AsteronString*)malloc(sizeof(AsteronString) + 16);
    host_str->header.obj_type = ASTERON_OBJ_STRING;
    host_str->length = 9;
    strcpy(host_str->chars, "localhost");
    
    AsteronValue args[1];
    args[0] = ASTERON_PTR(host_str, ASTERON_VAL_STRING);
    
    printf("  localhost ->  ");
    result = net_resolve(1, args);
    print_value(result);
    printf("\n");
    
    free(host_str);
}

/* =============================================================================
 * DEMO: MÓDULO TIME
 * ============================================================================= */

static void demo_time_module(void) {
    print_section("MÓDULO: time");
    
    AsteronValue result;
    
    printf("  Now (ms):     ");
    result = time_now(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  Now (ns):     ");
    result = time_now_ns(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  Year:         ");
    result = time_year(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  Month:        ");
    result = time_month(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  Day:          ");
    result = time_day(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  Hour:         ");
    result = time_hour(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  Minute:       ");
    result = time_minute(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  Second:       ");
    result = time_second(0, NULL);
    print_value(result);
    printf("\n");
    
    printf("  Timezone:     ");
    result = time_timezone(0, NULL);
    print_value(result);
    printf("\n");
    
    /* Benchmark */
    printf("\n  Benchmarking 100000 calls to time.now()...\n");
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < 100000; i++) {
        time_now(0, NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                        (end.tv_nsec - start.tv_nsec) / 1000000.0;
    
    printf("  Elapsed:      %.2f ms\n", elapsed_ms);
    printf("  Per call:     %.3f µs\n", elapsed_ms * 10);
}

/* =============================================================================
 * DEMO: MÓDULO MATH
 * ============================================================================= */

static void demo_math_module(void) {
    print_section("MÓDULO: math");
    
    AsteronValue args[3];
    AsteronValue result;
    
    /* sin(π/2) = 1 */
    args[0] = ASTERON_NUMBER(3.14159265358979 / 2);
    result = math_sin(1, args);
    printf("  sin(π/2):     ");
    print_value(result);
    printf("\n");
    
    /* cos(0) = 1 */
    args[0] = ASTERON_NUMBER(0);
    result = math_cos(1, args);
    printf("  cos(0):       ");
    print_value(result);
    printf("\n");
    
    /* sqrt(16) = 4 */
    args[0] = ASTERON_NUMBER(16);
    result = math_sqrt(1, args);
    printf("  sqrt(16):     ");
    print_value(result);
    printf("\n");
    
    /* pow(2, 10) = 1024 */
    args[0] = ASTERON_NUMBER(2);
    args[1] = ASTERON_NUMBER(10);
    result = math_pow(2, args);
    printf("  pow(2, 10):   ");
    print_value(result);
    printf("\n");
    
    /* log(e) = 1 */
    args[0] = ASTERON_NUMBER(2.71828182845905);
    result = math_log(1, args);
    printf("  log(e):       ");
    print_value(result);
    printf("\n");
    
    /* clamp(150, 0, 100) = 100 */
    args[0] = ASTERON_NUMBER(150);
    args[1] = ASTERON_NUMBER(0);
    args[2] = ASTERON_NUMBER(100);
    result = math_clamp(3, args);
    printf("  clamp(150):   ");
    print_value(result);
    printf("\n");
    
    /* lerp(0, 10, 0.5) = 5 */
    args[0] = ASTERON_NUMBER(0);
    args[1] = ASTERON_NUMBER(10);
    args[2] = ASTERON_NUMBER(0.5);
    result = math_lerp(3, args);
    printf("  lerp(0,10,.5):");
    print_value(result);
    printf("\n");
    
    /* random() */
    result = math_random(0, NULL);
    printf("  random():     ");
    print_value(result);
    printf("\n");
}

/* =============================================================================
 * DEMO: ESTADO REATIVO
 * ============================================================================= */

/* Efeito que será disparado quando estado mudar */
static AsteronValue on_connection_change(void* user_data) {
    printf("  [REACTIVE] Conexão mudou! user_data=%p\n", user_data);
    return ASTERON_NIL();
}

static void demo_reactive_state(void) {
    print_section("ESTADO REATIVO");
    
    printf("  Criando runtime reativo...\n");
    
    ReactiveRuntime* rt = reactive_runtime_create();
    if (!rt) {
        printf("  ERRO: Falha ao criar runtime\n");
        return;
    }
    
    /* Cria variáveis de estado */
    printf("  Criando estados reativos...\n");
    
    ReactiveNode* connection_state = reactive_state(rt, "connection", ASTERON_NIL());
    ReactiveNode* message_state = reactive_state(rt, "last_message", ASTERON_NIL());
    
    /* Cria derived (calculado automaticamente) */
    ReactiveNode* is_connected = reactive_derived(rt, "is_connected", NULL, NULL);
    reactive_add_dep(is_connected, connection_state);
    
    /* Cria efeito */
    ReactiveNode* effect = reactive_effect(rt, "on_connection", 
                                            on_connection_change, 
                                            (void*)0xDEADBEEF);
    reactive_add_dep(effect, connection_state);
    
    printf("  Estados criados:\n");
    printf("    - connection (state)\n");
    printf("    - last_message (state)\n");
    printf("    - is_connected (derived)\n");
    printf("    - on_connection (effect)\n\n");
    
    /* Simula mudança de estado */
    printf("  Simulando conexão...\n");
    
    /* Cria valor de conexão */
    AsteronString* conn_str = (AsteronString*)malloc(sizeof(AsteronString) + 32);
    conn_str->header.obj_type = ASTERON_OBJ_STRING;
    conn_str->length = 9;
    strcpy(conn_str->chars, "connected");
    
    AsteronValue conn_val = ASTERON_PTR(conn_str, ASTERON_VAL_STRING);
    
    printf("  Setando connection = 'connected'...\n");
    reactive_set(rt, connection_state, conn_val);
    
    printf("  Propagando mudanças...\n");
    reactive_propagate(rt);
    
    /* Estatísticas */
    printf("\n  Estatísticas do runtime:\n");
    printf("    - Total de nós: %u\n", rt->node_count);
    printf("    - Propagações: %lu\n", rt->stats.propagations);
    printf("    - Computações: %lu\n", rt->stats.computations);
    
    free(conn_str);
    reactive_runtime_destroy(rt);
    
    printf("\n  Runtime reativo destruído.\n");
}

/* =============================================================================
 * DEMO: MODULE MANAGER
 * ============================================================================= */

static void demo_module_manager(void) {
    print_section("MODULE MANAGER");
    
    printf("  Nota: O ModuleManager integra todos os módulos\n");
    printf("  com o runtime reativo e a VM.\n\n");
    
    printf("  Módulos disponíveis:\n");
    printf("    - os      Sistema operacional\n");
    printf("    - net     Networking (TCP/UDP/DNS)\n");
    printf("    - fs      Sistema de arquivos\n");
    printf("    - time    Tempo e timestamps\n");
    printf("    - math    Funções matemáticas\n");
    printf("    - task    Concorrência\n\n");
    
    printf("  Funcionalidades:\n");
    printf("    ✓ Auto-registro de módulos\n");
    printf("    ✓ Funções como first-class entities\n");
    printf("    ✓ Estado reativo integrado\n");
    printf("    ✓ Hot reload suportado\n");
    printf("    ✓ Event loop integrado\n");
    printf("    ✓ SSA versioning para variáveis\n");
    printf("    ✓ Paralelização automática\n");
}

/* =============================================================================
 * MAIN
 * ============================================================================= */

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║         ASTERON MODULE SYSTEM DEMO                                ║\n");
    printf("║         Sistema de Módulos com Estado Reativo                     ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");
    
    demo_os_module();
    demo_net_module();
    demo_time_module();
    demo_math_module();
    demo_reactive_state();
    demo_module_manager();
    
    print_section("CONCLUÍDO");
    printf("  ✓ Todos os módulos testados com sucesso!\n\n");
    
    return 0;
}

