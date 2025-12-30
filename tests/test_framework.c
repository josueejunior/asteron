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

// =============================================================================
// CRIAÇÃO E DESTRUIÇÃO
// =============================================================================

TestSuite* test_suite_create(const char* name) {
    TestSuite* suite = (TestSuite*)calloc(1, sizeof(TestSuite));
    if (!suite) return NULL;
    
    suite->suite_name = name;
    suite->test_capacity = 16;
    suite->tests = (TestCase*)calloc(suite->test_capacity, sizeof(TestCase));
    
    return suite;
}

void test_suite_destroy(TestSuite* suite) {
    if (!suite) return;
    
    if (suite->tests) {
        for (size_t i = 0; i < suite->test_count; i++) {
            if (suite->tests[i].error_msg) {
                free((void*)suite->tests[i].error_msg);
            }
        }
        free(suite->tests);
    }
    
    free(suite);
}

// =============================================================================
// ADICIONAR TESTES
// =============================================================================

void test_suite_add(TestSuite* suite, const char* test_name, bool (*test_func)(void)) {
    if (!suite || !test_name || !test_func) return;
    
    // Expandir array se necessário
    if (suite->test_count >= suite->test_capacity) {
        suite->test_capacity *= 2;
        suite->tests = (TestCase*)realloc(suite->tests, 
            suite->test_capacity * sizeof(TestCase));
    }
    
    TestCase* test = &suite->tests[suite->test_count++];
    test->name = test_name;
    test->test_func = test_func;
    test->passed = false;
    test->error_msg = NULL;
}

// =============================================================================
// EXECUTAR TESTES
// =============================================================================

int test_suite_run(TestSuite* suite) {
    if (!suite) return -1;
    
    printf("\n=== Running Test Suite: %s ===\n", suite->suite_name);
    
    suite->passed_count = 0;
    suite->failed_count = 0;
    
    for (size_t i = 0; i < suite->test_count; i++) {
        TestCase* test = &suite->tests[i];
        printf("  [%zu/%zu] %s... ", i + 1, suite->test_count, test->name);
        fflush(stdout);
        
        bool result = test->test_func();
        test->passed = result;
        
        if (result) {
            printf("✓ PASSED\n");
            suite->passed_count++;
        } else {
            printf("✗ FAILED\n");
            suite->failed_count++;
        }
    }
    
    return suite->failed_count > 0 ? 1 : 0;
}

void test_suite_print_summary(TestSuite* suite) {
    if (!suite) return;
    
    printf("\n=== Test Suite Summary: %s ===\n", suite->suite_name);
    printf("  Total: %zu\n", suite->test_count);
    printf("  Passed: %zu\n", suite->passed_count);
    printf("  Failed: %zu\n", suite->failed_count);
    printf("  Success Rate: %.1f%%\n", 
        suite->test_count > 0 ? 
        (double)suite->passed_count / suite->test_count * 100.0 : 0.0);
    
    if (suite->failed_count > 0) {
        printf("\n  Failed Tests:\n");
        for (size_t i = 0; i < suite->test_count; i++) {
            if (!suite->tests[i].passed) {
                printf("    - %s\n", suite->tests[i].name);
            }
        }
    }
    printf("==============================\n");
}

// =============================================================================
// RUNNER GLOBAL
// =============================================================================

// Lista global de suites (simplificado)
#define MAX_SUITES 32
static TestSuite* g_suites[MAX_SUITES];
static size_t g_suite_count = 0;

void register_test_suite(TestSuite* suite) {
    if (g_suite_count < MAX_SUITES) {
        g_suites[g_suite_count++] = suite;
    }
}

int run_all_tests(void) {
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║     ASTERON TEST SUITE                                         ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    
    int total_failed = 0;
    size_t total_tests = 0;
    size_t total_passed = 0;
    
    for (size_t i = 0; i < g_suite_count; i++) {
        int failed = test_suite_run(g_suites[i]);
        test_suite_print_summary(g_suites[i]);
        total_failed += failed;
        total_tests += g_suites[i]->test_count;
        total_passed += g_suites[i]->passed_count;
    }
    
    printf("\n╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║     OVERALL SUMMARY                                            ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("  Total Tests: %zu\n", total_tests);
    printf("  Passed: %zu\n", total_passed);
    printf("  Failed: %zu\n", total_tests - total_passed);
    printf("  Success Rate: %.1f%%\n", 
        total_tests > 0 ? (double)total_passed / total_tests * 100.0 : 0.0);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return total_failed > 0 ? 1 : 0;
}

