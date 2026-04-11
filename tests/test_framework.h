/**
 * @file test_framework.h
 * @brief Minimal testing framework for libdoudizhu
 * @author Peng Yang Deng, Emma Le
 * @date 08-Mar-26
 */

#ifndef LIBDOUDIZHU_TEST_FRAMEWORK_H
#define LIBDOUDIZHU_TEST_FRAMEWORK_H

#include <stdio.h>

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT(cond, msg)                                                                                              \
    do {                                                                                                               \
        if (cond) {                                                                                                    \
            g_passed++;                                                                                                \
        } else {                                                                                                       \
            fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg);                                           \
            g_failed++;                                                                                                \
        }                                                                                                              \
    } while (0)

#define EXPECT_EQ(a, b, msg) EXPECT((a) == (b), msg)
#define EXPECT_NE(a, b, msg) EXPECT((a) != (b), msg)

static void beginSuite(const char* name) { printf("| -> %s\n", name); }

#define PRINT_RESULTS() printf("--- Results: %d passed, %d failed ---\n", g_passed, g_failed)

#define RETURN_TEST_RESULT() return (g_failed > 0 ? 1 : 0)

#endif // LIBDOUDIZHU_TEST_FRAMEWORK_H
