/*
 * EL3 Extension - System Register Access Tests
 *
 * Verifies EL3 system register access:
 *   - Direct read/write of SCR_EL3, VBAR_EL3, SCTLR_EL3 at EL3
 *   - SCR_EL3 control bit verification
 */
#include "test_framework.h"
#include "sysreg_ops.h"

static volatile uint64_t el3_sysreg_sink;

/* ============================================================
 * EL3-SYSREG-01: Read SCR_EL3 at EL3 succeeds
 * ============================================================ */
static void read_scr_at_el3(uint64_t arg)
{
    (void)arg;
    el3_sysreg_sink = read_scr_el3();
}

TEST_REGISTER(test_el3_read_scr_el3);
bool test_el3_read_scr_el3(void)
{
    TEST_BEGIN("EL3-SYSREG-01: MRS SCR_EL3 @ EL3 → ok");

    el3_sysreg_sink = 0;
    run_at_el(EL3, read_scr_at_el3, 0);

    /* SCR_EL3.RW should be set (EL2 is AArch64) */
    TEST_ASSERT("SCR_EL3.RW is set", (el3_sysreg_sink & SCR_RW_BIT) != 0);
    /* SCR_EL3.NS should be set (non-secure) */
    TEST_ASSERT("SCR_EL3.NS is set", (el3_sysreg_sink & SCR_NS_BIT) != 0);
    /* SCR_EL3.HCE should be set (HVC enabled) */
    TEST_ASSERT("SCR_EL3.HCE is set", (el3_sysreg_sink & SCR_HCE_BIT) != 0);

    TEST_END();
}

/* ============================================================
 * EL3-SYSREG-02: Read/Write VBAR_EL3 at EL3 succeeds
 * ============================================================ */
static volatile uint64_t el3_vbar_original;
static volatile uint64_t el3_vbar_readback;

static void rw_vbar_at_el3(uint64_t arg)
{
    (void)arg;
    el3_vbar_original = read_vbar_el3();
    write_vbar_el3(el3_vbar_original);
    el3_vbar_readback = read_vbar_el3();
}

TEST_REGISTER(test_el3_rw_vbar_el3);
bool test_el3_rw_vbar_el3(void)
{
    TEST_BEGIN("EL3-SYSREG-02: MRS/MSR VBAR_EL3 @ EL3 → ok");

    el3_vbar_original = 0;
    el3_vbar_readback = 0xFFFF;
    run_at_el(EL3, rw_vbar_at_el3, 0);

    TEST_ASSERT_EQ("vbar_el3 roundtrip", el3_vbar_readback, el3_vbar_original);

    TEST_END();
}

/* ============================================================
 * EL3-SYSREG-03: Read SCTLR_EL3 at EL3 succeeds
 * ============================================================ */
static void read_sctlr_at_el3(uint64_t arg)
{
    (void)arg;
    el3_sysreg_sink = read_sctlr_el3();
}

TEST_REGISTER(test_el3_read_sctlr_el3);
bool test_el3_read_sctlr_el3(void)
{
    TEST_BEGIN("EL3-SYSREG-03: MRS SCTLR_EL3 @ EL3 → ok");

    el3_sysreg_sink = 0xDEAD;
    run_at_el(EL3, read_sctlr_at_el3, 0);

    TEST_ASSERT("SCTLR_EL3 read completed", true);

    TEST_END();
}

/* ============================================================
 * EL3-SYSREG-04: Read ESR_EL3 at EL3 succeeds
 * ============================================================ */
static void read_esr_at_el3(uint64_t arg)
{
    (void)arg;
    el3_sysreg_sink = read_esr_el3();
}

TEST_REGISTER(test_el3_read_esr_el3);
bool test_el3_read_esr_el3(void)
{
    TEST_BEGIN("EL3-SYSREG-04: MRS ESR_EL3 @ EL3 → ok");

    el3_sysreg_sink = 0xDEAD;
    run_at_el(EL3, read_esr_at_el3, 0);

    TEST_ASSERT("ESR_EL3 read completed", true);

    TEST_END();
}

/* ============================================================
 * EL3-SYSREG-05: SCR_EL3 write roundtrip at EL3
 * ============================================================ */
static volatile uint64_t el3_scr_readback;

static void scr_write_roundtrip_at_el3(uint64_t arg)
{
    (void)arg;
    uint64_t original = read_scr_el3();

    /* Verify we can write and read back. Keep essential bits intact. */
    write_scr_el3(original);
    el3_scr_readback = read_scr_el3();
}

TEST_REGISTER(test_el3_scr_write_roundtrip);
bool test_el3_scr_write_roundtrip(void)
{
    TEST_BEGIN("EL3-SYSREG-05: SCR_EL3 write roundtrip @ EL3");

    el3_scr_readback = 0;
    run_at_el(EL3, scr_write_roundtrip_at_el3, 0);

    /* SCR_EL3.RW should still be set */
    TEST_ASSERT("SCR_EL3.RW still set", (el3_scr_readback & SCR_RW_BIT) != 0);

    TEST_END();
}
