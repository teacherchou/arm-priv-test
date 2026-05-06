/*
 * ARM Privilege Test Framework - Test Runner Implementation
 */
#include "test_framework.h"
#include "uart.h"

test_state_t current_test;
test_summary_t test_summary;

void test_run_all(void)
{
    test_func_t *entry;

    test_summary.total = 0;
    test_summary.passed = 0;
    test_summary.failed = 0;
    test_summary.skipped = 0;

    for (entry = _test_table; entry < _test_table_end; entry++) {
        if (*entry) {
            trap_record_reset();
            (*entry)();
        }
    }
}

void test_print_summary(void)
{
    printf("\n========================================\n");
    printf("  Test Summary\n");
    printf("========================================\n");
    printf("  Total:   %d\n", test_summary.total);
    printf("  Passed:  %d\n", test_summary.passed);
    printf("  Failed:  %d\n", test_summary.failed);
    printf("  Skipped: %d\n", test_summary.skipped);
    printf("========================================\n");

    if (test_summary.failed > 0) {
        printf("  RESULT: FAIL\n");
    } else {
        printf("  RESULT: PASS\n");
    }
    printf("========================================\n\n");
}
