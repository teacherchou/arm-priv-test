/*
 * EL2 Extension - EL1 Access to EL2 Registers Trap Tests
 *
 * Verifies that EL1 code attempting to directly access EL2 system
 * registers receives an exception (UNDEF / EC=0x00), since these
 * registers are only accessible from EL2 or higher.
 */
#include "test_framework.h"
#include "sysreg_ops.h"

static volatile uint64_t access_trap_sink;

/* ============================================================
 * EL2-ACCESS-01: MRS HCR_EL2 from EL1 should trap
 *
 * HCR_EL2 is only accessible from EL2+. Reading it from EL1
 * should produce an UNDEF trap (EC=0x00).
 * ============================================================ */
TEST_REGISTER(test_el2_access_hcr_from_el1);
bool test_el2_access_hcr_from_el1(void)
{
    TEST_BEGIN("EL2-ACCESS-01: MRS HCR_EL2 @ EL1 → trap");

    EXPECT_TRAP(EC_UNKNOWN, access_trap_sink = read_hcr_el2());
    TEST_ASSERT_EQ("trap target is EL1", trap_record.target_el, 1);

    TEST_END();
}

/* ============================================================
 * EL2-ACCESS-02: MRS VBAR_EL2 from EL1 should trap
 * ============================================================ */
TEST_REGISTER(test_el2_access_vbar_from_el1);
bool test_el2_access_vbar_from_el1(void)
{
    TEST_BEGIN("EL2-ACCESS-02: MRS VBAR_EL2 @ EL1 → trap");

    EXPECT_TRAP(EC_UNKNOWN, access_trap_sink = read_vbar_el2());
    TEST_ASSERT_EQ("trap target is EL1", trap_record.target_el, 1);

    TEST_END();
}

/* ============================================================
 * EL2-ACCESS-03: MRS ESR_EL2 from EL1 should trap
 * ============================================================ */
TEST_REGISTER(test_el2_access_esr_from_el1);
bool test_el2_access_esr_from_el1(void)
{
    TEST_BEGIN("EL2-ACCESS-03: MRS ESR_EL2 @ EL1 → trap");

    EXPECT_TRAP(EC_UNKNOWN, access_trap_sink = read_esr_el2());
    TEST_ASSERT_EQ("trap target is EL1", trap_record.target_el, 1);

    TEST_END();
}

/* ============================================================
 * EL2-ACCESS-04: MRS SCTLR_EL2 from EL1 should trap
 * ============================================================ */
TEST_REGISTER(test_el2_access_sctlr_from_el1);
bool test_el2_access_sctlr_from_el1(void)
{
    TEST_BEGIN("EL2-ACCESS-04: MRS SCTLR_EL2 @ EL1 → trap");

    EXPECT_TRAP(EC_UNKNOWN, access_trap_sink = read_sctlr_el2());
    TEST_ASSERT_EQ("trap target is EL1", trap_record.target_el, 1);

    TEST_END();
}
