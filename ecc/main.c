/*
 * ecc extension - Main Entry
 *
 * Tests ECC (Error Correcting Code) functionality:
 *   - Pure memory tests: direct bit-flip → read → verify (ECC_001/002/003)
 *   - ecc_inject tests: software-simulated injection layer (INJ_CE/UE)
 */
#include "test_framework.h"
#include "uart.h"
#include "ecc_inject.h"

__attribute__((weak))
void ext_reset(void)
{
    ecc_clear_status();
}

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

    ecc_inject_init();
    ext_reset();
    test_run_all();
    test_print_summary();

    if (test_summary.failed > 0)
        PLATFORM_HALT_FAIL();
    else
        PLATFORM_HALT_PASS();

    return 0;
}
