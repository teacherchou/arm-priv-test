/*
 * Interrupt Privilege Tests - GICv3 System Register Access
 *
 * GICv3 CPU interface registers (ICC_*_EL1) are EL1-privileged.
 * EL0 access produces EC=0x00 (UNDEF) since the instructions are
 * undefined at EL0 (EC=0x18 only applies to configured trap routing).
 */
#include "test_framework.h"
#include "gic.h"

static volatile uint64_t gic_sink;

/* ---- EL0 test functions ---- */

static void el0_read_icc_pmr(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(gic_sink = read_icc_pmr_el1());
}

static void el0_write_icc_pmr(uint64_t arg)
{
    EL_DO_TRAP(write_icc_pmr_el1(arg));
}

static void el0_read_icc_iar1(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(gic_sink = read_icc_iar1_el1());
}

static void el0_write_icc_eoir1(uint64_t arg)
{
    EL_DO_TRAP(write_icc_eoir1_el1(arg));
}

static void el0_read_icc_igrpen1(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(gic_sink = read_icc_igrpen1_el1());
}

static void el0_write_icc_sgi1r(uint64_t arg)
{
    EL_DO_TRAP(write_icc_sgi1r_el1(arg));
}

static void el0_read_icc_ctlr(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(gic_sink = read_icc_ctlr_el1());
}

static void el0_read_icc_hppir1(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(gic_sink = read_icc_hppir1_el1());
}

static void el0_read_icc_bpr1(uint64_t arg)
{
    (void)arg;
    EL_DO_TRAP(gic_sink = read_icc_bpr1_el1());
}

/* ---- EL1 positive tests ---- */

TEST_REGISTER(test_gicv3_read_pmr_at_el1);
bool test_gicv3_read_pmr_at_el1(void)
{
    TEST_BEGIN("GIC-E1-01: MRS ICC_PMR_EL1 @ EL1 → ok");
    EXPECT_NO_TRAP(gic_sink = read_icc_pmr_el1());
    TEST_END();
}

TEST_REGISTER(test_gicv3_write_pmr_at_el1);
bool test_gicv3_write_pmr_at_el1(void)
{
    TEST_BEGIN("GIC-E1-02: MSR ICC_PMR_EL1 @ EL1 → ok");
    uint64_t orig = read_icc_pmr_el1();
    EXPECT_NO_TRAP(write_icc_pmr_el1(0xFF));
    write_icc_pmr_el1(orig);
    TEST_END();
}

TEST_REGISTER(test_gicv3_read_igrpen1_at_el1);
bool test_gicv3_read_igrpen1_at_el1(void)
{
    TEST_BEGIN("GIC-E1-03: MRS ICC_IGRPEN1_EL1 @ EL1 → ok");
    EXPECT_NO_TRAP(gic_sink = read_icc_igrpen1_el1());
    TEST_END();
}

TEST_REGISTER(test_gicv3_read_ctlr_at_el1);
bool test_gicv3_read_ctlr_at_el1(void)
{
    TEST_BEGIN("GIC-E1-04: MRS ICC_CTLR_EL1 @ EL1 → ok");
    EXPECT_NO_TRAP(gic_sink = read_icc_ctlr_el1());
    TEST_END();
}

TEST_REGISTER(test_gicv3_read_bpr1_at_el1);
bool test_gicv3_read_bpr1_at_el1(void)
{
    TEST_BEGIN("GIC-E1-05: MRS ICC_BPR1_EL1 @ EL1 → ok");
    EXPECT_NO_TRAP(gic_sink = read_icc_bpr1_el1());
    TEST_END();
}

/* ---- EL0 negative tests ---- */

TEST_REGISTER(test_gicv3_read_pmr_at_el0);
bool test_gicv3_read_pmr_at_el0(void)
{
    TEST_BEGIN("GIC-E0-01: MRS ICC_PMR_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_icc_pmr, 0);
    CHECK_TRAP("icc_pmr read from EL0", EC_UNKNOWN);
    TEST_END();
}

TEST_REGISTER(test_gicv3_write_pmr_at_el0);
bool test_gicv3_write_pmr_at_el0(void)
{
    TEST_BEGIN("GIC-E0-02: MSR ICC_PMR_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_write_icc_pmr, 0xFF);
    CHECK_TRAP("icc_pmr write from EL0", EC_UNKNOWN);
    TEST_END();
}

TEST_REGISTER(test_gicv3_read_iar1_at_el0);
bool test_gicv3_read_iar1_at_el0(void)
{
    TEST_BEGIN("GIC-E0-03: MRS ICC_IAR1_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_icc_iar1, 0);
    CHECK_TRAP("icc_iar1 read from EL0", EC_UNKNOWN);
    TEST_END();
}

TEST_REGISTER(test_gicv3_write_eoir1_at_el0);
bool test_gicv3_write_eoir1_at_el0(void)
{
    TEST_BEGIN("GIC-E0-04: MSR ICC_EOIR1_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_write_icc_eoir1, 0);
    CHECK_TRAP("icc_eoir1 write from EL0", EC_UNKNOWN);
    TEST_END();
}

TEST_REGISTER(test_gicv3_read_igrpen1_at_el0);
bool test_gicv3_read_igrpen1_at_el0(void)
{
    TEST_BEGIN("GIC-E0-05: MRS ICC_IGRPEN1_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_icc_igrpen1, 0);
    CHECK_TRAP("icc_igrpen1 read from EL0", EC_UNKNOWN);
    TEST_END();
}

TEST_REGISTER(test_gicv3_write_sgi1r_at_el0);
bool test_gicv3_write_sgi1r_at_el0(void)
{
    TEST_BEGIN("GIC-E0-06: MSR ICC_SGI1R_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_write_icc_sgi1r, 0);
    CHECK_TRAP("icc_sgi1r write from EL0", EC_UNKNOWN);
    TEST_END();
}

TEST_REGISTER(test_gicv3_read_ctlr_at_el0);
bool test_gicv3_read_ctlr_at_el0(void)
{
    TEST_BEGIN("GIC-E0-07: MRS ICC_CTLR_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_icc_ctlr, 0);
    CHECK_TRAP("icc_ctlr read from EL0", EC_UNKNOWN);
    TEST_END();
}

TEST_REGISTER(test_gicv3_read_hppir1_at_el0);
bool test_gicv3_read_hppir1_at_el0(void)
{
    TEST_BEGIN("GIC-E0-08: MRS ICC_HPPIR1_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_icc_hppir1, 0);
    CHECK_TRAP("icc_hppir1 read from EL0", EC_UNKNOWN);
    TEST_END();
}

TEST_REGISTER(test_gicv3_read_bpr1_at_el0);
bool test_gicv3_read_bpr1_at_el0(void)
{
    TEST_BEGIN("GIC-E0-09: MRS ICC_BPR1_EL1 @ EL0 → trap");
    run_at_el(EL0, el0_read_icc_bpr1, 0);
    CHECK_TRAP("icc_bpr1 read from EL0", EC_UNKNOWN);
    TEST_END();
}
