/*
 * ECC Extension - Uncorrectable Error (UE) Tests via ecc_inject API
 *
 * Uses the software-simulated ecc_inject layer to test UE detection.
 */
#include "test_framework.h"
#include "ecc_inject.h"

extern uintptr_t __ecc_test_start;

/* ============================================================
 * INJ_UE_001: 2bit data error → UE via ecc_inject
 * ============================================================ */
TEST_REGISTER(test_inject_ue_001_data);
bool test_inject_ue_001_data(void)
{
    TEST_BEGIN("INJ_UE_001: ecc_inject 2bit data → UE detected");

    uintptr_t test_addr = (uintptr_t)&__ecc_test_start + 0x1200;
    uint64_t pattern = 0xFFFFFFFFFFFFFFFFULL;

    ecc_write_with_ecc(test_addr, pattern);
    ecc_inject_error(test_addr, ECC_INJECT_2BIT, ECC_TARGET_DATA);
    uint64_t readback = ecc_read_with_check(test_addr);

    TEST_ASSERT_NEQ("corrupted != original", readback, pattern);

    ecc_status_t status = ecc_get_status();
    TEST_ASSERT("UE detected", status.ue_detected);
    TEST_ASSERT("no CE", !status.ce_detected);
    TEST_ASSERT_EQ("error addr", status.error_addr, test_addr);
    TEST_ASSERT("syndrome non-zero", status.syndrome != 0);

    TEST_END();
}
