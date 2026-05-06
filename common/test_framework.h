/*
 * ARM Privilege Test Framework - Test Framework Core
 *
 * Provides macros for test registration, lifecycle, and assertions.
 * Design mirrors damo-priv-test's approach:
 *   - TEST_REGISTER: linker-section auto-collection
 *   - TEST_BEGIN/END: lifecycle + result tracking
 *   - EXPECT_TRAP/NO_TRAP: armed-trap assertion pattern
 *   - EL_DO_TRAP/EL_DO_NO_TRAP: lower-EL execution + deferred assertion
 */
#ifndef _TEST_FRAMEWORK_H_
#define _TEST_FRAMEWORK_H_

#include "types.h"
#include "trap.h"
#include "privilege.h"
#include "uart.h"
#include "encoding.h"

/* ---- Test function type ---- */
typedef bool (*test_func_t)(void);

/* ---- Linker-collected test table ---- */
extern test_func_t _test_table[];
extern test_func_t _test_table_end[];

/* ---- Test registration (linker-section auto-collection) ---- */
#define TEST_REGISTER(test_fn)                                      \
    bool test_fn(void);                                             \
    static test_func_t test_fn##_ptr                                \
        __attribute__((section(".test_table"), used)) = test_fn

/* ---- Test lifecycle ---- */

/* Per-test state */
typedef struct {
    const char *name;
    bool passed;
    bool skipped;
} test_state_t;

/* Global counters */
typedef struct {
    uint32_t total;
    uint32_t passed;
    uint32_t failed;
    uint32_t skipped;
} test_summary_t;

extern test_state_t current_test;
extern test_summary_t test_summary;

#define TEST_BEGIN(test_name) do {                                   \
    current_test.name = (test_name);                                \
    current_test.passed = true;                                     \
    current_test.skipped = false;                                   \
    trap_record_reset();                                            \
    printf("[TEST] %s ... ", test_name);                             \
} while (0)

#define TEST_END() do {                                             \
    test_summary.total++;                                           \
    if (current_test.skipped) {                                     \
        test_summary.skipped++;                                     \
        printf("SKIP\n");                                           \
    } else if (current_test.passed) {                               \
        test_summary.passed++;                                      \
        printf("PASS\n");                                           \
    } else {                                                        \
        test_summary.failed++;                                      \
        printf("FAIL\n");                                           \
    }                                                               \
    trap_record_reset();                                            \
    return current_test.passed;                                     \
} while (0)

#define TEST_SKIP(reason) do {                                      \
    current_test.skipped = true;                                    \
    printf("(skip: %s) ", reason);                                  \
    TEST_END();                                                     \
} while (0)

/* ---- Assertions ---- */

#define TEST_ASSERT(msg, cond) do {                                 \
    if (!(cond)) {                                                  \
        printf("\n  ASSERT FAIL: %s\n", msg);                       \
        current_test.passed = false;                                \
    }                                                               \
} while (0)

#define TEST_ASSERT_EQ(msg, actual, expected) do {                  \
    uint64_t _a = (uint64_t)(actual);                               \
    uint64_t _e = (uint64_t)(expected);                             \
    if (_a != _e) {                                                 \
        printf("\n  ASSERT_EQ FAIL: %s\n", msg);                    \
        printf("    expected: 0x%lx, actual: 0x%lx\n", _e, _a);    \
        current_test.passed = false;                                \
    }                                                               \
} while (0)

#define TEST_ASSERT_NEQ(msg, actual, not_expected) do {             \
    uint64_t _a = (uint64_t)(actual);                               \
    uint64_t _ne = (uint64_t)(not_expected);                        \
    if (_a == _ne) {                                                \
        printf("\n  ASSERT_NEQ FAIL: %s\n", msg);                   \
        printf("    should not be: 0x%lx\n", _ne);                 \
        current_test.passed = false;                                \
    }                                                               \
} while (0)

#define TEST_ASSERT_BITS_SET(msg, value, mask) do {                 \
    uint64_t _v = (uint64_t)(value);                                \
    uint64_t _m = (uint64_t)(mask);                                 \
    if ((_v & _m) != _m) {                                         \
        printf("\n  ASSERT_BITS_SET FAIL: %s\n", msg);              \
        printf("    value: 0x%lx, mask: 0x%lx\n", _v, _m);         \
        current_test.passed = false;                                \
    }                                                               \
} while (0)

#define TEST_ASSERT_BITS_CLEAR(msg, value, mask) do {               \
    uint64_t _v = (uint64_t)(value);                                \
    uint64_t _m = (uint64_t)(mask);                                 \
    if ((_v & _m) != 0) {                                          \
        printf("\n  ASSERT_BITS_CLEAR FAIL: %s\n", msg);            \
        printf("    value: 0x%lx, mask: 0x%lx\n", _v, _m);         \
        current_test.passed = false;                                \
    }                                                               \
} while (0)

/* ---- Armed-trap assertions (from EL1, for same-EL or self tests) ---- */

/*
 * Execute statement and assert NO exception occurs.
 * Must be called from EL1.
 */
#define EXPECT_NO_TRAP(stmt) do {                                   \
    trap_expect_begin();                                            \
    stmt;                                                           \
    trap_expect_end();                                              \
    if (trap_record.triggered) {                                    \
        printf("\n  UNEXPECTED TRAP: EC=0x%x ESR=0x%lx ELR=0x%lx\n", \
               trap_record.exception_class,                         \
               trap_record.esr, trap_record.elr);                   \
        current_test.passed = false;                                \
    }                                                               \
} while (0)

/*
 * Execute statement and assert it triggers a trap with the given EC.
 * Must be called from EL1.
 */
#define EXPECT_TRAP(expected_ec, stmt) do {                         \
    trap_expect_begin();                                            \
    stmt;                                                           \
    trap_expect_end();                                              \
    if (!trap_record.triggered) {                                   \
        printf("\n  EXPECTED TRAP (EC=0x%x) but none occurred\n",   \
               (expected_ec));                                      \
        current_test.passed = false;                                \
    } else if (trap_record.exception_class != (expected_ec)) {      \
        printf("\n  WRONG TRAP EC: expected 0x%x, got 0x%x\n",     \
               (expected_ec), trap_record.exception_class);         \
        current_test.passed = false;                                \
    }                                                               \
} while (0)

/*
 * Execute statement and assert it traps with the given EC at the given EL.
 * Convenience macro that combines EXPECT_TRAP + target_el assertion.
 */
#define EXPECT_TRAP_AT(expected_ec, expected_el, stmt) do {         \
    EXPECT_TRAP(expected_ec, stmt);                                 \
    if (trap_record.triggered) {                                    \
        TEST_ASSERT_EQ("trap target EL",                            \
                        trap_record.target_el, (expected_el));      \
    }                                                               \
} while (0)

/*
 * Lower-EL execution with deferred assertion.
 *
 * Because EL0 cannot access UART (no permission), we use a two-phase
 * pattern similar to damo-priv-test's PRIV_DO_TRAP + CHECK_TRAP:
 *
 *   1. EL_DO_TRAP(stmt):     arm trap, execute at current EL (may be EL0)
 *   2. CHECK_TRAP(msg, ec):  back at EL1, assert trap occurred
 */
#define EL_DO_TRAP(stmt) do {                                       \
    trap_expect_begin();                                            \
    stmt;                                                           \
    trap_expect_end();                                              \
} while (0)

#define EL_DO_NO_TRAP(stmt) do {                                    \
    trap_expect_begin();                                            \
    stmt;                                                           \
    trap_expect_end();                                              \
} while (0)

#define CHECK_TRAP(msg, expected_ec) do {                            \
    if (!trap_record.triggered) {                                   \
        printf("\n  %s: expected trap (EC=0x%x) but none\n",        \
               (msg), (expected_ec));                               \
        current_test.passed = false;                                \
    } else if (trap_record.exception_class != (uint32_t)(expected_ec)) { \
        printf("\n  %s: wrong EC, expected 0x%x got 0x%x\n",       \
               (msg), (expected_ec), trap_record.exception_class);  \
        current_test.passed = false;                                \
    }                                                               \
    trap_record_reset();                                            \
} while (0)

#define CHECK_NO_TRAP(msg) do {                                     \
    if (trap_record.triggered) {                                    \
        printf("\n  %s: unexpected trap EC=0x%x ESR=0x%lx\n",      \
               (msg), trap_record.exception_class,                  \
               trap_record.esr);                                    \
        current_test.passed = false;                                \
    }                                                               \
    trap_record_reset();                                            \
} while (0)

/* ---- Test runner & summary ---- */
void test_run_all(void);
void test_print_summary(void);

#endif /* _TEST_FRAMEWORK_H_ */
