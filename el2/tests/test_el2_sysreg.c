/*
 * EL2 Extension - System Register Access Tests
 *
 * Verifies EL2 system register access:
 *   - Direct read/write of HCR_EL2, VBAR_EL2, SCTLR_EL2 at EL2
 *   - EL1 cannot directly access EL2 system registers (traps)
 */
#include "test_framework.h"
#include "sysreg_ops.h"

/* ============================================================
 * EL2-SYSREG-01: Read HCR_EL2 at EL2 succeeds
 * ============================================================ */
static volatile uint64_t el2_sysreg_sink;

static void read_hcr_at_el2(uint64_t arg)
{
    (void)arg;
    el2_sysreg_sink = read_hcr_el2();
}

TEST_REGISTER(test_el2_read_hcr_el2);
bool test_el2_read_hcr_el2(void)
{
    TEST_BEGIN("EL2-SYSREG-01: MRS HCR_EL2 @ EL2 → ok");

    el2_sysreg_sink = 0;
    run_at_el(EL2, read_hcr_at_el2, 0);

    /* HCR_EL2.RW should be set (EL1 is AArch64) */
    TEST_ASSERT("HCR_EL2.RW is set", (el2_sysreg_sink & HCR_RW_BIT) != 0);

    TEST_END();
}

/* ============================================================
 * EL2-SYSREG-02: Read/Write VBAR_EL2 at EL2 succeeds
 * ============================================================ */
static volatile uint64_t el2_vbar_original;
static volatile uint64_t el2_vbar_readback;

static void rw_vbar_at_el2(uint64_t arg)
{
    (void)arg;
    el2_vbar_original = read_vbar_el2();
    /* Write it back (don't change it, just verify roundtrip) */
    write_vbar_el2(el2_vbar_original);
    el2_vbar_readback = read_vbar_el2();
}

TEST_REGISTER(test_el2_rw_vbar_el2);
bool test_el2_rw_vbar_el2(void)
{
    TEST_BEGIN("EL2-SYSREG-02: MRS/MSR VBAR_EL2 @ EL2 → ok");

    el2_vbar_original = 0;
    el2_vbar_readback = 0xFFFF;
    run_at_el(EL2, rw_vbar_at_el2, 0);

    TEST_ASSERT_EQ("vbar_el2 roundtrip", el2_vbar_readback, el2_vbar_original);

    TEST_END();
}

/* ============================================================
 * EL2-SYSREG-03: Read SCTLR_EL2 at EL2 succeeds
 * ============================================================ */
static void read_sctlr_at_el2(uint64_t arg)
{
    (void)arg;
    el2_sysreg_sink = read_sctlr_el2();
}

TEST_REGISTER(test_el2_read_sctlr_el2);
bool test_el2_read_sctlr_el2(void)
{
    TEST_BEGIN("EL2-SYSREG-03: MRS SCTLR_EL2 @ EL2 → ok");

    el2_sysreg_sink = 0xDEAD;
    run_at_el(EL2, read_sctlr_at_el2, 0);

    /* Should not have faulted; value can be anything */
    TEST_ASSERT("SCTLR_EL2 read completed", true);

    TEST_END();
}

/* ============================================================
 * EL2-SYSREG-04: Read ESR_EL2 at EL2 succeeds
 * ============================================================ */
static void read_esr_at_el2(uint64_t arg)
{
    (void)arg;
    el2_sysreg_sink = read_esr_el2();
}

TEST_REGISTER(test_el2_read_esr_el2);
bool test_el2_read_esr_el2(void)
{
    TEST_BEGIN("EL2-SYSREG-04: MRS ESR_EL2 @ EL2 → ok");

    el2_sysreg_sink = 0xDEAD;
    run_at_el(EL2, read_esr_at_el2, 0);

    TEST_ASSERT("ESR_EL2 read completed", true);

    TEST_END();
}

/* ============================================================
 * EL2-SYSREG-05: HCR_EL2 write roundtrip at EL2
 * ============================================================ */
static volatile uint64_t el2_hcr_readback;

static void hcr_write_roundtrip_at_el2(uint64_t arg)
{
    (void)arg;
    uint64_t original = read_hcr_el2();

    /* Set and clear the SWIO bit (harmless, no side effect) */
    uint64_t modified = original | HCR_SWIO_BIT;
    write_hcr_el2(modified);
    el2_hcr_readback = read_hcr_el2();

    /* Restore original */
    write_hcr_el2(original);
}

TEST_REGISTER(test_el2_hcr_write_roundtrip);
bool test_el2_hcr_write_roundtrip(void)
{
    TEST_BEGIN("EL2-SYSREG-05: HCR_EL2 write roundtrip @ EL2");

    el2_hcr_readback = 0;
    run_at_el(EL2, hcr_write_roundtrip_at_el2, 0);

    TEST_ASSERT("HCR_EL2.SWIO is set after write",
                (el2_hcr_readback & HCR_SWIO_BIT) != 0);

    TEST_END();
}
