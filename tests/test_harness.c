/*
 * @file: test_harness.c
 * @brief Test runner implementation.
 */

#include "test_harness.h"

static unsigned int test_index = 1;

void
run_test(void (*test_func)(void), const char *name)
{
    test_func();
    printf("ok %u - %s\n", test_index++, name);
}
