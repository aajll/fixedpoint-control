/*
 * @file: test_main.c
 * @brief Unified unit-test driver for the fixedpoint-control library.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern void run_pid_tests(void);
extern void run_filters_tests(void);

int
main(void)
{
    printf("TAP version 13\n");
    // We don't know the total number of tests beforehand, 
    // so we omit the "1..N" line or we could calculate it.
    // For now, let's just skip it or use a large number if needed.
    // But standard TAP allows omitting it.

    run_pid_tests();
    run_filters_tests();

    return EXIT_SUCCESS;
}
