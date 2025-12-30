/*
 * Asteron Runtime - (C) 2025 Josué Junior da Cruz de Freitas
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "test_framework.h"
#include "../src/core/brain/context_brain.h"
#include "../src/core/brain/intent_engine.h"
#include "../src/core/brain/self_tuning.h"
#include "../src/devtools/visual_debugger.h"
#include "../src/core/vm/vm.h"
#include "../src/graph/unified_graph.h"

// =============================================================================
// TESTES BÁSICOS
// =============================================================================

static bool test_context_brain_create_destroy(void) {
    // Criar VM e Graph mock (simplificado)
    VM* vm = NULL; // TODO: Criar VM mock
    UnifiedGraph* graph = NULL; // TODO: Criar Graph mock
    
    ContextBrain* brain = context_brain_create(vm, graph);
    ASSERT_NOT_NULL(brain, "Context Brain deve ser criado");
    
    context_brain_destroy(brain);
    
    return true;
}

static bool test_intent_engine_create_destroy(void) {
    VM* vm = NULL; // TODO: Criar VM mock
    
    IntentEngine* engine = intent_engine_create(vm);
    ASSERT_NOT_NULL(engine, "Intent Engine deve ser criado");
    
    intent_engine_destroy(engine);
    
    return true;
}

static bool test_self_tuning_create_destroy(void) {
    UnifiedGraph* graph = NULL; // TODO: Criar Graph mock
    
    SelfTuningRuntime* tuning = self_tuning_create(graph);
    ASSERT_NOT_NULL(tuning, "Self-Tuning Runtime deve ser criado");
    
    self_tuning_destroy(tuning);
    
    return true;
}

static bool test_visual_debugger_create_destroy(void) {
    VM* vm = NULL; // TODO: Criar VM mock
    UnifiedGraph* graph = NULL; // TODO: Criar Graph mock
    
    VisualDebugger* debugger = visual_debugger_create(vm, graph);
    ASSERT_NOT_NULL(debugger, "Visual Debugger deve ser criado");
    
    visual_debugger_destroy(debugger);
    
    return true;
}

// =============================================================================
// REGISTRO DE TESTES
// =============================================================================

TestSuite* create_brain_test_suite(void) {
    TestSuite* suite = test_suite_create("Brain Systems");
    
    test_suite_add(suite, "Context Brain create/destroy", test_context_brain_create_destroy);
    test_suite_add(suite, "Intent Engine create/destroy", test_intent_engine_create_destroy);
    test_suite_add(suite, "Self-Tuning create/destroy", test_self_tuning_create_destroy);
    test_suite_add(suite, "Visual Debugger create/destroy", test_visual_debugger_create_destroy);
    
    return suite;
}

