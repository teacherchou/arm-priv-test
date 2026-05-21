/*
 * sysreg extension - Main Entry
 *
 * Tests system register access privileges across exception levels.
 * Verifies MRS/MSR, TLBI, IC, ERET behave correctly at EL1 and EL0.
 */
#include "test_framework.h"
#include "uart.h"

extern void ext_reset(void);

/* Weak definition: extensions can override for custom reset */
__attribute__((weak))
void ext_reset(void)
{
    /* No extension-specific reset for sysreg tests */
}

int main(void)
{
    uart_init();

    printf("\n");
    printf("========================================\n");
    printf("  ARM Privilege Test Framework\n");
    printf("  Extension: sysreg\n");
    printf("  Platform:  " PLATFORM_NAME "\n");
    printf("  Current EL: %d\n", get_current_el());
    printf("========================================\n\n");

    ext_reset();
    test_run_all();
    test_print_summary("sysreg");

    if (test_summary.failed > 0)
        PLATFORM_HALT_FAIL();
    else
        PLATFORM_HALT_PASS();

    return 0;
}
