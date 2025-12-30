/**
 * =============================================================================
 * EXEMPLO: SISTEMA DE CHAMADAS EM TEMPO REAL
 * =============================================================================
 * 
 * Demonstra o paradigma STATE-DRIVEN do Asteron.
 * 
 * MODELO TRADICIONAL (request-driven):
 * 
 *   on_websocket_message(ws, data) {
 *       if (data.type == "offer") {
 *           process_offer(data);
 *           send_to_peer(data);
 *       } else if (data.type == "answer") {
 *           process_answer(data);
 *           connect_audio();
 *       }
 *   }
 * 
 * MODELO ASTERON (state-driven):
 * 
 *   // WebSocket só MUTA estado
 *   on ws.message { session.data = data }
 *   
 *   // Runtime REAGE automaticamente
 *   when session.offer_ready && session.answer_ready {
 *       connect_audio(session.userA, session.userB)
 *   }
 * 
 * A DIFERENÇA:
 *   ❌ Tradicional: Lógica dentro do evento
 *   ✅ Asteron: Evento muta estado, runtime executa
 * 
 * =============================================================================
 */

#include "../src/reactive/reactive.h"
#include "../src/reactive/ws_driver.h"
#include <stdio.h>
#include <string.h>

/* =============================================================================
 * CONTEXTO DA SESSÃO DE CHAMADA
 * ============================================================================= */

typedef struct {
    ReactiveNode* userA_socket;
    ReactiveNode* userB_socket;
    ReactiveNode* status;
    ReactiveNode* offer;
    ReactiveNode* answer;
    ReactiveNode* audio_active;
} CallSession;

CallSession* create_call_session(ReactiveRuntime* rt) {
    CallSession* session = (CallSession*)malloc(sizeof(CallSession));
    
    /* Cria contexto vivo */
    ReactiveNode* ctx = reactive_context(rt, "CallSession");
    
    /* Campos reativos */
    session->userA_socket = reactive_context_field(rt, ctx, "userA", ASTERON_NIL());
    session->userB_socket = reactive_context_field(rt, ctx, "userB", ASTERON_NIL());
    session->status = reactive_context_field(rt, ctx, "status", 
        ASTERON_NUMBER(0));  /* 0=idle, 1=ringing, 2=connected, 3=ended */
    session->offer = reactive_context_field(rt, ctx, "offer", ASTERON_NIL());
    session->answer = reactive_context_field(rt, ctx, "answer", ASTERON_NIL());
    session->audio_active = reactive_context_field(rt, ctx, "audio_active", 
        ASTERON_BOOL(false));
    
    return session;
}

/* =============================================================================
 * EFEITOS REATIVOS (WHEN BLOCKS)
 * ============================================================================= */

/* Dispara quando ambos os lados têm SDP */
static AsteronValue on_sdp_ready(ReactiveRuntime* rt, ReactiveNode* node) {
    (void)node;
    
    printf("🔗 [REACTIVE] SDP de ambos os lados prontos!\n");
    printf("   Estado mudou -> Conectando audio...\n");
    
    /* Aqui conectaria o áudio real */
    /* Por enquanto, apenas imprime */
    
    return ASTERON_BOOL(true);
}

/* Dispara quando status muda para "connected" */
static AsteronValue on_connected(ReactiveRuntime* rt, ReactiveNode* node) {
    CallSession* session = (CallSession*)node->user_data;
    
    /* Lê valores atuais (registra dependências) */
    AsteronValue status = reactive_get(rt, session->status);
    
    if (ASTERON_AS_NUMBER(status) == 2) {  /* connected */
        printf("📞 [REACTIVE] Chamada conectada!\n");
        printf("   Iniciando stream de áudio automaticamente...\n");
        
        /* Ativa áudio */
        reactive_set(rt, session->audio_active, ASTERON_BOOL(true));
    }
    
    return ASTERON_NIL();
}

/* Dispara quando recebe mensagem do WebSocket */
static AsteronValue on_ws_message(ReactiveRuntime* rt, ReactiveNode* node) {
    printf("📨 [REACTIVE] Mensagem recebida do WebSocket\n");
    printf("   Processando automaticamente...\n");
    
    return ASTERON_NIL();
}

/* =============================================================================
 * SIMULAÇÃO
 * ============================================================================= */

void simulate_call_flow(ReactiveRuntime* rt, CallSession* session) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║     ASTERON STATE-DRIVEN CALL SESSION                        ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Modelo: Estado -> Execução (não Evento -> Callback)          ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    /* Configura efeitos (when blocks) */
    ReactiveNode* connected_effect = reactive_effect(rt, "on_connected", 
                                                       on_connected, session);
    connected_effect->user_data = session;
    reactive_add_dep(connected_effect, session->status);
    
    /* Imprime estado inicial */
    printf("📊 Estado inicial:\n");
    reactive_print_graph(rt);
    
    /* Simula: Usuário A inicia chamada */
    printf("👤 Usuário A inicia chamada...\n");
    reactive_set(rt, session->status, ASTERON_NUMBER(1));  /* ringing */
    
    /* Simula: Usuário B aceita */
    printf("👤 Usuário B aceita...\n");
    
    /* Em batch para evitar múltiplas propagações */
    REACTIVE_BATCH(rt, {
        reactive_set(rt, session->status, ASTERON_NUMBER(2));  /* connected */
        
        /* Simula recebimento de SDP */
        AsteronString* offer = (AsteronString*)malloc(sizeof(AsteronString) + 64);
        offer->header.obj_type = ASTERON_OBJ_STRING;
        offer->header.ref_count = 1;
        offer->length = 10;
        strcpy(offer->chars, "sdp:offer");
        reactive_set(rt, session->offer, ASTERON_PTR(offer, ASTERON_VAL_STRING));
        
        AsteronString* answer = (AsteronString*)malloc(sizeof(AsteronString) + 64);
        answer->header.obj_type = ASTERON_OBJ_STRING;
        answer->header.ref_count = 1;
        answer->length = 11;
        strcpy(answer->chars, "sdp:answer");
        reactive_set(rt, session->answer, ASTERON_PTR(answer, ASTERON_VAL_STRING));
    });
    
    printf("\n📊 Estado após conexão:\n");
    reactive_print_graph(rt);
    
    /* Verifica áudio ativo */
    AsteronValue audio = reactive_get(rt, session->audio_active);
    if (ASTERON_AS_BOOL(audio)) {
        printf("🔊 Áudio ativo automaticamente!\n");
    }
    
    /* Estatísticas */
    uint64_t updates, props, effects;
    reactive_get_stats(rt, &updates, &props, &effects);
    
    printf("\n📈 Estatísticas:\n");
    printf("   Updates: %lu\n", updates);
    printf("   Propagations: %lu\n", props);
    printf("   Effects triggered: %lu\n", effects);
    
    printf("\n✅ Demonstração concluída!\n");
    printf("   O runtime reagiu automaticamente às mudanças de estado.\n");
    printf("   Nenhum callback manual foi necessário.\n\n");
}

/* =============================================================================
 * MAIN
 * ============================================================================= */

int main(void) {
    printf("🚀 Inicializando Asteron State-Driven Runtime...\n\n");
    
    /* Cria runtime reativo */
    ReactiveRuntime* rt = reactive_runtime_create();
    if (!rt) {
        fprintf(stderr, "Erro ao criar runtime reativo\n");
        return 1;
    }
    
    /* Cria sessão de chamada */
    CallSession* session = create_call_session(rt);
    
    /* Simula fluxo de chamada */
    simulate_call_flow(rt, session);
    
    /* Exporta grafo para visualização */
    char* dot = reactive_export_dot(rt);
    printf("📝 Grafo exportado para Graphviz:\n%s\n", dot);
    free(dot);
    
    /* Cleanup */
    free(session);
    reactive_runtime_destroy(rt);
    
    return 0;
}

