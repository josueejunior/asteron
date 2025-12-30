/**
 * Teste direto do módulo net em C
 * 
 * Compile com:
 *   gcc -I src tests/test_net_c.c \
 *       obj/modules/module.o \
 *       obj/modules/native/net_module.o \
 *       -o test_net
 * 
 * Execute:
 *   ./test_net
 */

#include <stdio.h>
#include <string.h>
#include "../src/modules/native/net_module.h"

int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              TESTE DO MÓDULO NET                             ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    /* Teste 1: hostname() */
    printf("1. Testando hostname()...\n");
    AsteronValue host = net_hostname(0, NULL);
    if (ASTERON_IS_STRING(host)) {
        AsteronString* str = ASTERON_AS_STRING(host);
        printf("   ✓ Hostname: %s\n\n", str->chars);
    } else {
        printf("   ✗ Erro ao obter hostname\n\n");
    }
    
    /* Teste 2: resolve() */
    printf("2. Testando resolve(\"google.com\")...\n");
    
    /* Cria string para argumento */
    char hostname_str[] = "google.com";
    AsteronString* arg_str = (AsteronString*)malloc(sizeof(AsteronString) + strlen(hostname_str) + 1);
    arg_str->header.obj_type = ASTERON_OBJ_STRING;
    arg_str->header.ref_count = 1;
    arg_str->length = strlen(hostname_str);
    strcpy(arg_str->chars, hostname_str);
    
    AsteronValue args[1];
    args[0] = ASTERON_PTR(arg_str, ASTERON_VAL_STRING);
    
    AsteronValue ip = net_resolve(1, args);
    if (ASTERON_IS_STRING(ip)) {
        AsteronString* ip_str = ASTERON_AS_STRING(ip);
        printf("   ✓ IP: %s\n\n", ip_str->chars);
    } else {
        printf("   ✗ Erro ao resolver DNS\n\n");
    }
    
    /* Teste 3: tcp_connect + tcp_send + tcp_recv */
    printf("3. Testando conexão TCP (example.com:80)...\n");
    
    /* Prepara argumentos para tcp_connect */
    char host_arg[] = "example.com";
    AsteronString* host_str = (AsteronString*)malloc(sizeof(AsteronString) + strlen(host_arg) + 1);
    host_str->header.obj_type = ASTERON_OBJ_STRING;
    host_str->header.ref_count = 1;
    host_str->length = strlen(host_arg);
    strcpy(host_str->chars, host_arg);
    
    AsteronValue connect_args[2];
    connect_args[0] = ASTERON_PTR(host_str, ASTERON_VAL_STRING);
    connect_args[1] = ASTERON_NUMBER(80);
    
    AsteronValue sock = net_tcp_connect(2, connect_args);
    
    if (ASTERON_IS_HANDLE(sock)) {
        printf("   ✓ Conectado!\n");
        
        /* Envia HTTP request */
        char request[] = "GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n";
        AsteronString* req_str = (AsteronString*)malloc(sizeof(AsteronString) + strlen(request) + 1);
        req_str->header.obj_type = ASTERON_OBJ_STRING;
        req_str->header.ref_count = 1;
        req_str->length = strlen(request);
        strcpy(req_str->chars, request);
        
        AsteronValue send_args[2];
        send_args[0] = sock;
        send_args[1] = ASTERON_PTR(req_str, ASTERON_VAL_STRING);
        
        AsteronValue sent = net_tcp_send(2, send_args);
        if (ASTERON_IS_NUMBER(sent)) {
            printf("   ✓ Enviado %d bytes\n", (int)ASTERON_AS_NUMBER(sent));
        }
        
        /* Recebe resposta */
        AsteronValue recv_args[2];
        recv_args[0] = sock;
        recv_args[1] = ASTERON_NUMBER(500);  /* Max 500 bytes */
        
        AsteronValue response = net_tcp_recv(2, recv_args);
        if (ASTERON_IS_STRING(response)) {
            AsteronString* resp_str = ASTERON_AS_STRING(response);
            printf("   ✓ Recebido %zu bytes:\n", resp_str->length);
            printf("   ─────────────────────────────\n");
            /* Mostra primeiras linhas */
            char* line = resp_str->chars;
            int lines = 0;
            for (size_t i = 0; i < resp_str->length && lines < 5; i++) {
                putchar(resp_str->chars[i]);
                if (resp_str->chars[i] == '\n') lines++;
            }
            printf("   ...\n");
            printf("   ─────────────────────────────\n");
        } else {
            printf("   ✗ Erro ao receber\n");
        }
        
        /* Fecha */
        AsteronValue close_args[1] = { sock };
        net_tcp_close(1, close_args);
        printf("   ✓ Conexão fechada\n\n");
        
    } else {
        printf("   ✗ Erro ao conectar (código: %d)\n\n", ASTERON_ERROR_CODE(sock));
    }
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              TESTE CONCLUÍDO!                                ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    return 0;
}

