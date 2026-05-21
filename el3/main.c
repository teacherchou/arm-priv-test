/*
 * el3 extension - Main Entry
 *
 * Tests EL3 (Secure Monitor) functionality:
 *   - SMC trap behavior
 *   - EL3 system register access
 *   - SCR_EL3 control bit verification
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
    printf("  Extension: el3 (Secure Monitor)\n");
    printf("  Platform:  " PLATFORM_NAME "\n");
    printf("  Current EL: %d\n", get_current_el());
    printf("========================================\n\n");

    ext_reset();
    test_run_all();
    test_print_summary("el3");

    if (test_summary.failed > 0)
        PLATFORM_HALT_FAIL();
    else
        PLATFORM_HALT_PASS();

    return 0;
}
