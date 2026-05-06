/*
 * Interrupt Privilege Tests - Generic Timer Register Access
 *
 * Timer registers named *_EL0 may still be access-controlled by
 * CNTKCTL_EL1 configuration. Tests verify both EL1 and EL0 behavior.
 */
#include "test_framework.h"
#include "gic.h"

static volatile uint64_t timer_sink;

/* ---- EL0 test functions ---- */

static void el0_read_cntp_ctl(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(timer_sink = read_cntp_ctl_el0());
}

static void el0_write_cntp_tval(uint64_t arg)
{
    EL_DO_TRAP(write_cntp_tval_el0(arg));
}

static void el0_read_cntv_ctl(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(timer_sink = read_cntv_ctl_el0());
}

static void el0_read_cntfrq(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(timer_sink = read_cntfrq_el0());
}

static void el0_read_cntvct(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(timer_sink = read_cntvct_el0());
}

/* ---- EL1 positive tests ---- */

TEST_REGISTER(test_timer_read_cntp_ctl_at_el1);
bool test_timer_read_cntp_ctl_at_el1(void)
{
    TEST_BEGIN("TMR-E1-01: MRS CNTP_CTL_EL0 @ EL1 → ok");
    EXPECT_NO_TRAP(timer_sink = read_cntp_ctl_el0());
    TEST_END();
}

TEST_REGISTER(test_timer_write_cntp_tval_at_el1);
bool test_timer_write_cntp_tval_at_el1(void)
{
    TEST_BEGIN("TMR-E1-02: MSR CNTP_TVAL_EL0 @ EL1 → ok");
    EXPECT_NO_TRAP(write_cntp_tval_el0(0x100000));
    /* Disable timer to avoid spurious interrupt */
    write_cntp_ctl_el0(0);
    TEST_END();
}

TEST_REGISTER(test_timer_read_cntfrq_at_el1);
bool test_timer_read_cntfrq_at_el1(void)
{
    TEST_BEGIN("TMR-E1-03: MRS CNTFRQ_EL0 @ EL1 → ok");
    EXPECT_NO_TRAP(timer_sink = read_cntfrq_el0());
    TEST_END();
}

TEST_REGISTER(test_timer_read_cntvct_at_el1);
bool test_timer_read_cntvct_at_el1(void)
{
    TEST_BEGIN("TMR-E1-04: MRS CNTVCT_EL0 @ EL1 → ok");
    EXPECT_NO_TRAP(timer_sink = read_cntvct_el0());
    TEST_END();
}

/* ---- EL0 access tests (behavior depends on CNTKCTL_EL1) ---- */

TEST_REGISTER(test_timer_read_cntp_ctl_at_el0);
bool test_timer_read_cntp_ctl_at_el0(void)
{
    TEST_BEGIN("TMR-E0-01: MRS CNTP_CTL_EL0 @ EL0 (config-dependent)");
    run_at_el(EL0, el0_read_cntp_ctl, 0);
    /*
     * Physical timer EL0 access is controlled by CNTKCTL_EL1.EL0PTEN.
     * Default on QEMU is typically disabled → should trap.
     */
    if (trap_record.triggered) {
        printf("(trapped) ");
    } else {
        printf("(allowed) ");
    }
    trap_record_reset();
    TEST_END();
}

TEST_REGISTER(test_timer_write_cntp_tval_at_el0);
bool test_timer_write_cntp_tval_at_el0(void)
{
    TEST_BEGIN("TMR-E0-02: MSR CNTP_TVAL_EL0 @ EL0 (config-dependent)");
    run_at_el(EL0, el0_write_cntp_tval, 0x100000);
    if (trap_record.triggered) {
        printf("(trapped) ");
    } else {
        printf("(allowed) ");
    }
    trap_record_reset();
    TEST_END();
}

TEST_REGISTER(test_timer_read_cntfrq_at_el0);
bool test_timer_read_cntfrq_at_el0(void)
{
    TEST_BEGIN("TMR-E0-03: MRS CNTFRQ_EL0 @ EL0 (usually allowed)");
    run_at_el(EL0, el0_read_cntfrq, 0);
    /* CNTFRQ_EL0 is typically readable from EL0 */
    if (trap_record.triggered) {
        printf("(trapped) ");
    } else {
        printf("(allowed as expected) ");
    }
    trap_record_reset();
    TEST_END();
}

TEST_REGISTER(test_timer_read_cntvct_at_el0);
bool test_timer_read_cntvct_at_el0(void)
{
    TEST_BEGIN("TMR-E0-04: MRS CNTVCT_EL0 @ EL0 (config-dependent)");
    run_at_el(EL0, el0_read_cntvct, 0);
    /*
     * CNTVCT_EL0 access is controlled by CNTKCTL_EL1.EL0VCTEN.
     * Record outcome without asserting specific behavior.
     */
    if (trap_record.triggered) {
        printf("(trapped) ");
    } else {
        printf("(allowed) ");
    }
    trap_record_reset();
    TEST_END();
}

TEST_REGISTER(test_timer_read_cntv_ctl_at_el0);
bool test_timer_read_cntv_ctl_at_el0(void)
{
    TEST_BEGIN("TMR-E0-05: MRS CNTV_CTL_EL0 @ EL0 (config-dependent)");
    run_at_el(EL0, el0_read_cntv_ctl, 0);
    if (trap_record.triggered) {
        printf("(trapped) ");
    } else {
        printf("(allowed) ");
    }
    trap_record_reset();
    TEST_END();
}
