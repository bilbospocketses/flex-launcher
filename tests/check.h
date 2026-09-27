// Minimal test helpers for the C unit tests: no framework, one executable per suite.
#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>

static int check_count = 0;
static int check_failures = 0;

#define CHECK(condition) do { \
    check_count++; \
    if (!(condition)) { \
        check_failures++; \
        fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

#define CHECK_INT(actual, expected) do { \
    int actual_ = (actual); \
    int expected_ = (expected); \
    check_count++; \
    if (actual_ != expected_) { \
        check_failures++; \
        fprintf(stderr, "%s:%d: %s is %d, expected %d\n", __FILE__, __LINE__, #actual, actual_, expected_); \
    } \
} while (0)

// A function to print the totals and give main() its exit code
static int check_report(void)
{
    printf("%d checks, %d failed\n", check_count, check_failures);
    return check_failures ? 1 : 0;
}

#endif
