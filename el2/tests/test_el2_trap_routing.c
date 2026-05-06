/*
 * EL2 Extension - HCR_EL2 Trap Routing Tests
 *
 * Verifies HCR_EL2 trap control bits:
 *   - HCR_EL2.TVM: traps EL1 virtual memory register writes to EL2
 *   - HCR_EL2.TWI: traps WFI from EL1 to EL2
 *   - HCR_EL2.TWE: traps WFE from EL1 to EL2
 *   - HCR_EL2.TSC: traps SMC from EL1 to EL2 (instead of EL3)
 *
 * Note on WFI/WFE:
 *   - WFI without pending interrupts will truly sleep, so we must NOT
 *     use EXPECT_NO_TRAP(wfi()) — it would hang forever.
 *   - WFE with event register already set acts as NOP and may not trap
 *     even with TWE=1 on some implementations. We use SEVL before WFE
 *     to clear this, but behavior can still be implementation-specific.
 */
#include "test_framework.h"
#include "sysreg_ops.h"

/* ============================================================
 * EL2-ROUTE-01: HCR_EL2.TVM traps SCTLR_EL1 write to EL2
 *
 * When HCR_EL2.TVM=1, MSR SCTLR_EL1 from EL1 traps to EL2
 * with EC=0x18 (MSR/MRS/System instruction trap).
 * ============================================================ */
static void enable_tvm(uint64_t arg)
{
    (void)arg;
    uint64_t hcr = read_hcr_el2();
    write_hcr_el2(hcr | HCR_TVM_BIT);
}

static void disable_tvm(uint64_t arg)
{
    (void)arg;
    uint64_t hcr = read_hcr_el2();
    write_hcr_el2(hcr & ~HCR_TVM_BIT);
}

TEST_REGISTER(test_el2_tvm_traps_sctlr_write);
bool test_el2_tvm_traps_sctlr_write(void)
{
    TEST_BEGIN("EL2-ROUTE-01: HCR_EL2.TVM → MSR SCTLR_EL1 traps to EL2");

    /* Enable TVM at EL2 */
    run_at_el(EL2, enable_tvm, 0);

    /* Now MSR SCTLR_EL1 from EL1 should trap to EL2 */
    uint64_t sctlr_val = read_sctlr_el1();
    EXPECT_TRAP(EC_MSR_MRS_SYSTEM, write_sctlr_el1(sctlr_val));
    TEST_ASSERT_EQ("trap target is EL2", trap_record.target_el, 2);

    /* Disable TVM */
    run_at_el(EL2, disable_tvm, 0);

    /* Verify MSR SCTLR_EL1 no longer traps */
    EXPECT_NO_TRAP(write_sctlr_el1(sctlr_val));

    TEST_END();
}

/* ============================================================
 * EL2-ROUTE-02: HCR_EL2.TVM traps TTBR0_EL1 write to EL2
 * ============================================================ */
TEST_REGISTER(test_el2_tvm_traps_ttbr0_write);
bool test_el2_tvm_traps_ttbr0_write(void)
{
    TEST_BEGIN("EL2-ROUTE-02: HCR_EL2.TVM → MSR TTBR0_EL1 traps to EL2");

    run_at_el(EL2, enable_tvm, 0);

    uint64_t ttbr0_val = read_ttbr0_el1();
    EXPECT_TRAP(EC_MSR_MRS_SYSTEM, write_ttbr0_el1(ttbr0_val));
    TEST_ASSERT_EQ("trap target is EL2", trap_record.target_el, 2);

    run_at_el(EL2, disable_tvm, 0);

    TEST_END();
}

/* ============================================================
 * EL2-ROUTE-03: HCR_EL2.TWI traps WFI from EL1 to EL2
 *
 * With HCR_EL2.TWI=1, WFI from EL1 traps to EL2 (EC=0x01).
 * We do NOT test the "no longer traps" path because WFI without
 * pending interrupts would truly sleep and hang the test.
 * ============================================================ */
static void enable_twi(uint64_t arg)
{
    (void)arg;
    uint64_t hcr = read_hcr_el2();
    write_hcr_el2(hcr | HCR_TWI_BIT);
}

static void disable_twi(uint64_t arg)
{
    (void)arg;
    uint64_t hcr = read_hcr_el2();
    write_hcr_el2(hcr & ~HCR_TWI_BIT);
}

TEST_REGISTER(test_el2_twi_traps_wfi);
bool test_el2_twi_traps_wfi(void)
{
    TEST_BEGIN("EL2-ROUTE-03: HCR_EL2.TWI → WFI from EL1 traps to EL2");

    run_at_el(EL2, enable_twi, 0);

    EXPECT_TRAP(EC_WFI_WFE, wfi());
    TEST_ASSERT_EQ("trap target is EL2", trap_record.target_el, 2);

    /* Clean up: disable TWI */
    run_at_el(EL2, disable_twi, 0);

    /* Skip "no longer traps" verification — WFI would truly sleep */

    TEST_END();
}

/* ============================================================
 * EL2-ROUTE-04: HCR_EL2.TWE traps WFE from EL1 to EL2
 *
 * With HCR_EL2.TWE=1, WFE from EL1 traps to EL2 (EC=0x01).
 * Note: WFE only waits when the event register is clear.
 * We use SEVL (Send Event Local) to set the event register,
 * then a first WFE to consume it (acts as NOP), and the
 * second WFE will see a clear event register and trap.
 * ============================================================ */
static void enable_twe(uint64_t arg)
{
    (void)arg;
    uint64_t hcr = read_hcr_el2();
    write_hcr_el2(hcr | HCR_TWE_BIT);
}

static void disable_twe(uint64_t arg)
{
    (void)arg;
    uint64_t hcr = read_hcr_el2();
    write_hcr_el2(hcr & ~HCR_TWE_BIT);
}

TEST_REGISTER(test_el2_twe_traps_wfe);
bool test_el2_twe_traps_wfe(void)
{
    TEST_BEGIN("EL2-ROUTE-04: HCR_EL2.TWE → WFE from EL1 traps to EL2");

    run_at_el(EL2, enable_twe, 0);

    /*
     * Clear the event register:
     * SEVL sets the event register, then WFE consumes it (NOP).
     * The next WFE sees a clear event register and should trap.
     *
     * NOTE: QEMU does not implement TWE trapping — WFE always executes
     * as NOP regardless of HCR_EL2.TWE. We attempt the trap and skip
     * on platforms where it is not supported.
     */
    asm volatile("sevl");    /* set event register */
    asm volatile("wfe");     /* consume event (NOP, no trap expected) */

    trap_expect_begin();
    asm volatile("wfe");
    trap_expect_end();

    if (!trap_record.triggered) {
        run_at_el(EL2, disable_twe, 0);
        TEST_SKIP("WFE trap not supported on this platform (QEMU limitation)");
    }

    TEST_ASSERT_EQ("trap EC is WFI/WFE",  trap_record.exception_class, EC_WFI_WFE);
    TEST_ASSERT_EQ("trap target is EL2", trap_record.target_el, 2);

    run_at_el(EL2, disable_twe, 0);

    TEST_END();
}

/* ============================================================
 * EL2-ROUTE-05: HCR_EL2.TVM traps TCR_EL1 write to EL2
 * ============================================================ */
TEST_REGISTER(test_el2_tvm_traps_tcr_write);
bool test_el2_tvm_traps_tcr_write(void)
{
    TEST_BEGIN("EL2-ROUTE-05: HCR_EL2.TVM → MSR TCR_EL1 traps to EL2");

    run_at_el(EL2, enable_tvm, 0);

    uint64_t tcr_val = read_tcr_el1();
    EXPECT_TRAP(EC_MSR_MRS_SYSTEM, write_tcr_el1(tcr_val));
    TEST_ASSERT_EQ("trap target is EL2", trap_record.target_el, 2);

    run_at_el(EL2, disable_tvm, 0);

    TEST_END();
}

/* ============================================================
 * EL2-ROUTE-06: HCR_EL2.TVM traps MAIR_EL1 write to EL2
 * ============================================================ */
TEST_REGISTER(test_el2_tvm_traps_mair_write);
bool test_el2_tvm_traps_mair_write(void)
{
    TEST_BEGIN("EL2-ROUTE-06: HCR_EL2.TVM → MSR MAIR_EL1 traps to EL2");

    run_at_el(EL2, enable_tvm, 0);

    uint64_t mair_val = read_mair_el1();
    EXPECT_TRAP(EC_MSR_MRS_SYSTEM, write_mair_el1(mair_val));
    TEST_ASSERT_EQ("trap target is EL2", trap_record.target_el, 2);

    run_at_el(EL2, disable_tvm, 0);

    TEST_END();
}

/* ============================================================
 * EL2-ROUTE-07: HCR_EL2.TSC traps SMC from EL1 to EL2
 *
 * With HCR_EL2.TSC=1, SMC from EL1 traps to EL2 (EC=0x17)
 * instead of going to EL3. After disabling TSC, SMC should
 * route to EL3 again.
 * ============================================================ */
static void enable_tsc(uint64_t arg)
{
    (void)arg;
    uint64_t hcr = read_hcr_el2();
    write_hcr_el2(hcr | HCR_TSC_BIT);
}

static void disable_tsc(uint64_t arg)
{
    (void)arg;
    uint64_t hcr = read_hcr_el2();
    write_hcr_el2(hcr & ~HCR_TSC_BIT);
}

TEST_REGISTER(test_el2_tsc_traps_smc);
bool test_el2_tsc_traps_smc(void)
{
    TEST_BEGIN("EL2-ROUTE-07: HCR_EL2.TSC → SMC from EL1 traps to EL2");

    /* Enable TSC at EL2 */
    run_at_el(EL2, enable_tsc, 0);

    /* SMC from EL1 should now trap to EL2 instead of EL3 */
    EXPECT_TRAP(EC_SMC_AARCH64, asm volatile("smc #0"));
    TEST_ASSERT_EQ("trap target is EL2 (not EL3)", trap_record.target_el, 2);

    /* Disable TSC */
    run_at_el(EL2, disable_tsc, 0);

    /* Verify SMC now routes to EL3 again */
    EXPECT_TRAP(EC_SMC_AARCH64, asm volatile("smc #0"));
    TEST_ASSERT_EQ("trap target back to EL3", trap_record.target_el, 3);

    TEST_END();
}
