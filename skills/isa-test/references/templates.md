# ARM AArch64/ARMv9 测试模板

## 1. EL0 访问 EL1 系统寄存器应 trap

```c
#include "test_framework.h"
#include "sysreg_ops.h"

static volatile uint64_t sink;

static void el0_read_xxx(uint64_t arg) {
    (void)arg;
    EL_DO_TRAP(sink = read_xxx_el1());
}

TEST_REGISTER(test_xxx_el0_trap);
bool test_xxx_el0_trap(void) {
    TEST_BEGIN("XXX: MRS XXX_EL1 at EL0 traps");

    run_at_el(EL0, el0_read_xxx, 0);
    CHECK_TRAP("XXX trapped from EL0", EC_UNKNOWN);

    TEST_END();
}
```

## 2. EL1 正向访问系统寄存器

```c
TEST_REGISTER(test_xxx_el1_ok);
bool test_xxx_el1_ok(void) {
    TEST_BEGIN("XXX: MRS XXX_EL1 at EL1 succeeds");

    volatile uint64_t value;
    EXPECT_NO_TRAP(value = read_xxx_el1());
    (void)value;

    TEST_END();
}
```

## 3. EL2 trap routing

```c
static void el2_set_tvm(uint64_t enable) {
    uint64_t hcr = read_hcr_el2();
    if (enable) {
        hcr |= HCR_TVM;
    } else {
        hcr &= ~HCR_TVM;
    }
    write_hcr_el2(hcr);
}

TEST_REGISTER(test_hcr_tvm_trap);
bool test_hcr_tvm_trap(void) {
    TEST_BEGIN("EL2: HCR_EL2.TVM traps EL1 TCR_EL1 write");

    run_at_el(EL2, el2_set_tvm, 1);
    EXPECT_TRAP(EC_MSR_MRS_SYSTEM, write_tcr_el1(0));
    run_at_el(EL2, el2_set_tvm, 0);

    TEST_END();
}
```

## 4. HVC / SMC 指令测试

```c
TEST_REGISTER(test_hvc_to_el2);
bool test_hvc_to_el2(void) {
    TEST_BEGIN("EL1: HVC traps to EL2");

    EXPECT_TRAP(EC_HVC, asm volatile("hvc #0"));

    TEST_END();
}
```

```c
TEST_REGISTER(test_smc_to_el3);
bool test_smc_to_el3(void) {
    TEST_BEGIN("EL1: SMC traps to EL3");

    EXPECT_TRAP(EC_SMC, asm volatile("smc #0"));

    TEST_END();
}
```

## 5. 中断类测试结构

```c
TEST_REGISTER(test_irq_masked);
bool test_irq_masked(void) {
    TEST_BEGIN("IRQ: masked interrupt is not taken");

    mask_irq_with_daif();
    inject_timer_irq();
    TEST_ASSERT("IRQ not taken", !irq_was_taken());
    unmask_irq_with_daif();

    TEST_END();
}
```

## 6. ECC 类测试结构

```c
TEST_REGISTER(test_ecc_ce);
bool test_ecc_ce(void) {
    TEST_BEGIN("ECC: 1-bit correctable error");

    volatile uint64_t *ptr = get_ecc_test_addr();
    uint64_t pattern = 0xA5A5A5A55A5A5A5AULL;
    *ptr = pattern;
    inject_ecc_1bit_error(ptr);
    uint64_t value = *ptr;

    TEST_ASSERT_EQ("corrected value", value, pattern);

    TEST_END();
}
```
