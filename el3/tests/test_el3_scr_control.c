/*
 * EL3 Extension - SCR_EL3 Control Bit Tests
 *
 * Verifies SCR_EL3 control bits:
 *   - SCR_EL3.SMD: disables SMC instruction
 *   - SCR_EL3.HCE: controls HVC enable/disable
 */
#include "test_framework.h"
#include "sysreg_ops.h"

/* ============================================================
 * EL3-SCR-01: SCR_EL3.HCE controls HVC availability
 *
 * When SCR_EL3.HCE=0, HVC from EL1 should cause an UNDEF
 * exception (EC=0x00) instead of an HVC trap (EC=0x16).
 * ============================================================ */
static void disable_hce(uint64_t arg)
{
    (void)arg;
    uint64_t scr = read_scr_el3();
    write_scr_el3(scr & ~SCR_HCE_BIT);
}

static void enable_hce(uint64_t arg)
{
    (void)arg;
    uint64_t scr = read_scr_el3();
    write_scr_el3(scr | SCR_HCE_BIT);
}

TEST_REGISTER(test_el3_scr_hce_disable);
bool test_el3_scr_hce_disable(void)
{
    TEST_BEGIN("EL3-SCR-01: SCR_EL3.HCE=0 → HVC causes UNDEF");

    /* Disable HCE at EL3 */
    run_at_el(EL3, disable_hce, 0);

    /*
     * With HCE=0, HVC from EL1 should trap as UNDEF (EC=0x00)
     * rather than as HVC (EC=0x16).
     * The trap goes to EL1 since there's no valid hypervisor.
     */
    EXPECT_TRAP(EC_UNKNOWN, execute_hvc(0));

    /* Re-enable HCE */
    run_at_el(EL3, enable_hce, 0);

    /* Verify HVC works again */
    EXPECT_TRAP(EC_HVC_AARCH64, execute_hvc(0));
    TEST_ASSERT_EQ("trap target is EL2", trap_record.target_el, 2);

    TEST_END();
}

/* ============================================================
 * EL3-SCR-02: SCR_EL3.NS bit verification
 *
 * Verify that SCR_EL3.NS is set (we run in non-secure world).
 * ============================================================ */
static volatile uint64_t scr_value;

static void read_scr_for_ns(uint64_t arg)
{
    (void)arg;
    scr_value = read_scr_el3();
}

TEST_REGISTER(test_el3_scr_ns_set);
bool test_el3_scr_ns_set(void)
{
    TEST_BEGIN("EL3-SCR-02: SCR_EL3.NS is set (non-secure world)");

    scr_value = 0;
    run_at_el(EL3, read_scr_for_ns, 0);

    TEST_ASSERT("SCR_EL3.NS is set", (scr_value & SCR_NS_BIT) != 0);

    TEST_END();
}

/* ============================================================
 * EL3-SCR-03: SCR_EL3.RW bit verification
 *
 * Verify that SCR_EL3.RW is set (EL2 executes in AArch64).
 * ============================================================ */
TEST_REGISTER(test_el3_scr_rw_set);
bool test_el3_scr_rw_set(void)
{
    TEST_BEGIN("EL3-SCR-03: SCR_EL3.RW is set (EL2 is AArch64)");

    scr_value = 0;
    run_at_el(EL3, read_scr_for_ns, 0);

    TEST_ASSERT("SCR_EL3.RW is set", (scr_value & SCR_RW_BIT) != 0);

    TEST_END();
}

/* ============================================================
 * EL3-SCR-04: SCR_EL3.SMD controls SMC disable
 *
 * When SCR_EL3.SMD=1, SMC from EL1 should cause UNDEF (EC=0x00).
 * Note: This is tricky because we use SMC to enter EL3.
 * We verify it indirectly by reading the SMD bit.
 * ============================================================ */
static volatile uint64_t scr_smd_check;

static void check_smd_clear(uint64_t arg)
{
    (void)arg;
    scr_smd_check = read_scr_el3();
}

TEST_REGISTER(test_el3_scr_smd_clear);
bool test_el3_scr_smd_clear(void)
{
    TEST_BEGIN("EL3-SCR-04: SCR_EL3.SMD=0 (SMC not disabled)");

    scr_smd_check = 0xFFFF;
    run_at_el(EL3, check_smd_clear, 0);

    /* SMD should be 0 (SMC allowed) since we successfully used SMC */
    TEST_ASSERT("SCR_EL3.SMD is clear", (scr_smd_check & SCR_SMD_BIT) == 0);

    TEST_END();
}
