/*
 * irq extension - Main Entry
 *
 * Tests interrupt-related privilege controls across exception levels:
 *   - DAIF mask operations
 *   - GICv3 CPU interface registers (ICC_*_EL1)
 *   - Generic Timer registers
 */
#include "test_framework.h"
#include "uart.h"

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
    printf("  Extension: irq\n");
    printf("  Platform:  " PLATFORM_NAME "\n");
    printf("  Current EL: %d\n", get_current_el());
    printf("========================================\n\n");

    ext_reset();
    test_run_all();
    test_print_summary("irq");

    if (test_summary.failed > 0)
        PLATFORM_HALT_FAIL();
    else
        PLATFORM_HALT_PASS();

    return 0;
}
