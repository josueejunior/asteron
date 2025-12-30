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

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// =============================================================================
// ESTRUTURAS DE TESTE
// =============================================================================

typedef struct {
    const char* name;
    bool (*test_func)(void);
    bool passed;
    const char* error_msg;
} TestCase;

typedef struct {
    const char* suite_name;
    TestCase* tests;
    size_t test_count;
    size_t test_capacity;
    size_t passed_count;
    size_t failed_count;
} TestSuite;

// =============================================================================
// MACROS DE TESTE
// =============================================================================

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "  ✗ ASSERTION FAILED: %s\n", message); \
            return false; \
        } \
    } while (0)

#define ASSERT_EQ(a, b, message) \
    do { \
        if ((a) != (b)) { \
            fprintf(stderr, "  ✗ ASSERTION FAILED: %s (expected %d, got %d)\n", \
                message, (int)(b), (int)(a)); \
            return false; \
        } \
    } while (0)

#define ASSERT_STR_EQ(a, b, message) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            fprintf(stderr, "  ✗ ASSERTION FAILED: %s (expected '%s', got '%s')\n", \
                message, (b), (a)); \
            return false; \
        } \
    } while (0)

#define ASSERT_NOT_NULL(ptr, message) \
    do { \
        if ((ptr) == NULL) { \
            fprintf(stderr, "  ✗ ASSERTION FAILED: %s (pointer is NULL)\n", message); \
            return false; \
        } \
    } while (0)

#define ASSERT_NULL(ptr, message) \
    do { \
        if ((ptr) != NULL) { \
            fprintf(stderr, "  ✗ ASSERTION FAILED: %s (pointer is not NULL)\n", message); \
            return false; \
        } \
    } while (0)

// =============================================================================
// API DO FRAMEWORK
// =============================================================================

TestSuite* test_suite_create(const char* name);
void test_suite_destroy(TestSuite* suite);
void test_suite_add(TestSuite* suite, const char* test_name, bool (*test_func)(void));
int test_suite_run(TestSuite* suite);
void test_suite_print_summary(TestSuite* suite);

// Função auxiliar para rodar todos os testes
int run_all_tests(void);

#endif // TEST_FRAMEWORK_H

