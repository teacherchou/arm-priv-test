/*
 * EL3 Extension - SMC Trap Tests
 *
 * Verifies SMC instruction behavior:
 *   - SMC from EL1 traps to EL3 with EC=0x17
 *   - SMC immediate value is correctly captured in ISS
 *   - EL1→EL3 roundtrip via run_at_el works correctly
 */
#include "test_framework.h"
#include "sysreg_ops.h"

/* ============================================================
 * EL3-SMC-01: SMC from EL1 traps to EL3 with EC=0x17
 * ============================================================ */
TEST_REGISTER(test_el3_smc_trap_ec);
bool test_el3_smc_trap_ec(void)
{
    TEST_BEGIN("EL3-SMC-01: SMC from EL1 → trap to EL3 (EC=0x17)");

    EXPECT_TRAP(EC_SMC_AARCH64, asm volatile("smc #0"));

    TEST_ASSERT("trap target is EL3", trap_record.target_el == 3);
    TEST_ASSERT_EQ("exception class", trap_record.exception_class, EC_SMC_AARCH64);

    TEST_END();
}

/* ============================================================
 * EL3-SMC-02: SMC immediate value is captured in ISS
 * ============================================================ */
TEST_REGISTER(test_el3_smc_iss_value);
bool test_el3_smc_iss_value(void)
{
    TEST_BEGIN("EL3-SMC-02: SMC #0x55 → ISS captures imm16=0x55");

    EXPECT_TRAP(EC_SMC_AARCH64, asm volatile("smc #0x55"));

    uint32_t iss = (uint32_t)(trap_record.esr & ESR_ISS_MASK);
    TEST_ASSERT_EQ("ISS imm16", iss & 0xFFFF, 0x55);

    TEST_END();
}

/* ============================================================
 * EL3-SMC-03: run_at_el(EL3) roundtrip executes at EL3
 * ============================================================ */
static volatile uint32_t el3_observed_el;

static void check_el_at_el3(uint64_t arg)
{
    (void)arg;
    uint64_t current_el;
    asm volatile("mrs %0, CurrentEL" : "=r"(current_el));
    el3_observed_el = (uint32_t)((current_el >> 2) & 0x3);
}

TEST_REGISTER(test_el3_roundtrip_executes_at_el3);
bool test_el3_roundtrip_executes_at_el3(void)
{
    TEST_BEGIN("EL3-SMC-03: run_at_el(EL3) executes fn at EL3");

    el3_observed_el = 0xFF;
    run_at_el(EL3, check_el_at_el3, 0);

    TEST_ASSERT_EQ("observed EL", el3_observed_el, EL3);
    TEST_ASSERT_EQ("returned to EL1", get_current_el(), EL1);

    TEST_END();
}

/* ============================================================
 * EL3-SMC-04: run_at_el(EL3) passes argument correctly
 * ============================================================ */
static volatile uint64_t el3_received_arg;

static void capture_arg_at_el3(uint64_t arg)
{
    el3_received_arg = arg;
}

TEST_REGISTER(test_el3_roundtrip_passes_arg);
bool test_el3_roundtrip_passes_arg(void)
{
    TEST_BEGIN("EL3-SMC-04: run_at_el(EL3) passes argument");

    uint64_t test_val = 0xCAFEBABE87654321ULL;
    el3_received_arg = 0;
    run_at_el(EL3, capture_arg_at_el3, test_val);

    TEST_ASSERT_EQ("argument passed", el3_received_arg, test_val);

    TEST_END();
}
