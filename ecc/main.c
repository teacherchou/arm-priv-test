/*
 * ecc extension - Main Entry
 *
 * Tests ECC (Error Correcting Code) functionality:
 *   - 1bit correctable error (CE) detection and correction
 *   - 2bit uncorrectable error (UE) detection
 *
 * All tests use pure memory operations (write → flip bits → read → verify).
 * No dependency on ecc_inject simulation layer.
 */
#include "test_framework.h"
#include "uart.h"

__attribute__((weak))
void ext_reset(void) { }

int main(void)
{
    uart_init();

    printf("\n");
    printf("========================================\n");
    printf("  ARM Privilege Test Framework\n");
    printf("  Extension: ecc (Error Correcting Code)\n");
    printf("  Platform:  " PLATFORM_NAME "\n");
    printf("  Current EL: %d\n", get_current_el());
    printf("========================================\n\n");

    ext_reset();
    test_run_all();
    test_print_summary();

    if (test_summary.failed > 0)
        PLATFORM_HALT_FAIL();
    else
        PLATFORM_HALT_PASS();

    return 0;
}
