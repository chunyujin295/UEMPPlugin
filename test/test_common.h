#pragma once
// Shared test infrastructure for all test targets.

#include <cstdio>
#include <cstdlib>

// Each test target gets its own counters (static per TU).
struct TestRunner {
    int asserts  = 0;
    int failures = 0;

    void check(bool cond, const char* msg) {
        asserts++;
        if (!cond) { failures++; printf("  FAIL: %s\n", msg); }
        else       { printf("  PASS: %s\n", msg); }
    }

    int finish() {
        printf("\n  %d assertions, %d failures\n", asserts, failures);
        return failures > 0 ? 1 : 0;
    }
};
