/*
 * el2 extension - Main Entry
 *
 * Tests EL2 (Hypervisor) functionality:
 *   - HVC trap behavior
 *   - EL2 system register access
 *   - HCR_EL2 trap routing control
 */
#include "test_framework.h"
#include "uart.h"

extern void ext_reset(void);

__attribute__((weak))
void ext_reset(void)
{
}

int main(void)
{
    uart_init();

    printf("\n");
    printf("========================================\n");
    printf("  ARM Privilege Test Framework\n");
    printf("  Extension: el2 (Hypervisor)\n");
    printf("  Platform:  " PLATFORM_NAME "\n");
    printf("  Current EL: %d\n", get_current_el());
    printf("========================================\n\n");

    ext_reset();
    test_run_all();
    test_print_summary("el2");

    if (test_summary.failed > 0)
        PLATFORM_HALT_FAIL();
    else
        PLATFORM_HALT_PASS();

    return 0;
}
