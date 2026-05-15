/*
 * @file: test_harness.h
 * @brief Minimal test harness used by the fixedpoint-control unit-test suite.
 */

#ifndef TEST_HARNESS_H_
#define TEST_HARNESS_H_

#include <stdio.h>
#include <stdlib.h>

#define TEST_ASSERT(expr)                                                      \
        do {                                                                   \
                if (!(expr)) {                                                 \
                        fprintf(stderr, "FAIL  %s:%d  %s\n", __FILE__,         \
                                __LINE__, #expr);                              \
                        exit(EXIT_FAILURE);                                    \
                }                                                              \
        } while (0)

#define TEST_CASE(name)                                                        \
        static void name(void);                                                \
        static void name(void)

void run_test(void (*test_func)(void), const char *name);

#endif /* TEST_HARNESS_H_ */
