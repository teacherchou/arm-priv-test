/*
 * EL2 Extension - HVC Trap Tests
 *
 * Verifies HVC instruction behavior:
 *   - HVC from EL1 traps to EL2 with EC=0x16
 *   - HVC immediate value is correctly captured in ISS
 *   - EL1→EL2 roundtrip via run_at_el works correctly
 */
#include "test_framework.h"
#include "sysreg_ops.h"

/* ============================================================
 * EL2-HVC-01: HVC from EL1 traps to EL2 with EC=0x16
 * ============================================================ */
TEST_REGISTER(test_el2_hvc_trap_ec);
bool test_el2_hvc_trap_ec(void)
{
    TEST_BEGIN("EL2-HVC-01: HVC from EL1 → trap to EL2 (EC=0x16)");

    EXPECT_TRAP(EC_HVC_AARCH64, execute_hvc(0));

    TEST_ASSERT("trap target is EL2", trap_record.target_el == 2);
    TEST_ASSERT_EQ("exception class", trap_record.exception_class, EC_HVC_AARCH64);

    TEST_END();
}

/* ============================================================
 * EL2-HVC-02: HVC immediate value is captured in ISS
 * ============================================================ */
TEST_REGISTER(test_el2_hvc_iss_value);
bool test_el2_hvc_iss_value(void)
{
    TEST_BEGIN("EL2-HVC-02: HVC #0x42 → ISS captures imm16=0x42");

    EXPECT_TRAP(EC_HVC_AARCH64, execute_hvc(0x42));

    uint32_t iss = (uint32_t)(trap_record.esr & ESR_ISS_MASK);
    TEST_ASSERT_EQ("ISS imm16", iss & 0xFFFF, 0x42);

    TEST_END();
}

/* ============================================================
 * EL2-HVC-03: run_at_el(EL2) roundtrip executes at EL2
 * ============================================================ */
static volatile uint32_t el2_observed_el;

static void check_el_at_el2(uint64_t arg)
{
    (void)arg;
    uint64_t current_el;
    asm volatile("mrs %0, CurrentEL" : "=r"(current_el));
    el2_observed_el = (uint32_t)((current_el >> 2) & 0x3);
}

TEST_REGISTER(test_el2_roundtrip_executes_at_el2);
bool test_el2_roundtrip_executes_at_el2(void)
{
    TEST_BEGIN("EL2-HVC-03: run_at_el(EL2) executes fn at EL2");

    el2_observed_el = 0xFF;
    run_at_el(EL2, check_el_at_el2, 0);

    TEST_ASSERT_EQ("observed EL", el2_observed_el, EL2);
    TEST_ASSERT_EQ("returned to EL1", get_current_el(), EL1);

    TEST_END();
}

/* ============================================================
 * EL2-HVC-04: run_at_el(EL2) passes argument correctly
 * ============================================================ */
static volatile uint64_t el2_received_arg;

static void capture_arg_at_el2(uint64_t arg)
{
    el2_received_arg = arg;
}

TEST_REGISTER(test_el2_roundtrip_passes_arg);
bool test_el2_roundtrip_passes_arg(void)
{
    TEST_BEGIN("EL2-HVC-04: run_at_el(EL2) passes argument");

    uint64_t test_val = 0xDEADBEEF12345678ULL;
    el2_received_arg = 0;
    run_at_el(EL2, capture_arg_at_el2, test_val);

    TEST_ASSERT_EQ("argument passed", el2_received_arg, test_val);

    TEST_END();
}
