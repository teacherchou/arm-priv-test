/*
 * ECC Extension - Correctable Error (CE) Tests via ecc_inject API
 *
 * Uses the software-simulated ecc_inject layer to test CE detection
 * and correction. The ecc_inject module maintains a shadow table of
 * original data and simulates ECC check on read.
 */
#include "test_framework.h"
#include "ecc_inject.h"

extern uintptr_t __ecc_test_start;

/* ============================================================
 * INJ_CE_001: 1bit data error → CE via ecc_inject
 * ============================================================ */
TEST_REGISTER(test_inject_ce_001_data);
bool test_inject_ce_001_data(void)
{
    TEST_BEGIN("INJ_CE_001: ecc_inject 1bit data → CE corrected");

    uintptr_t test_addr = (uintptr_t)&__ecc_test_start + 0x1000;
    uint64_t pattern = 0xA5A5A5A55A5A5A5AULL;

    ecc_write_with_ecc(test_addr, pattern);
    ecc_inject_error(test_addr, ECC_INJECT_1BIT, ECC_TARGET_DATA);
    uint64_t readback = ecc_read_with_check(test_addr);

    TEST_ASSERT_EQ("corrected == original", readback, pattern);

    ecc_status_t status = ecc_get_status();
    TEST_ASSERT("CE detected", status.ce_detected);
    TEST_ASSERT("no UE", !status.ue_detected);
    TEST_ASSERT_EQ("error addr", status.error_addr, test_addr);
    TEST_ASSERT("syndrome non-zero", status.syndrome != 0);

    TEST_END();
}

/* ============================================================
 * INJ_CE_002: 1bit ecc-bit error → CE via ecc_inject
 * ============================================================ */
TEST_REGISTER(test_inject_ce_002_eccbit);
bool test_inject_ce_002_eccbit(void)
{
    TEST_BEGIN("INJ_CE_002: ecc_inject 1bit ecc-bit → CE corrected");

    uintptr_t test_addr = (uintptr_t)&__ecc_test_start + 0x1100;
    uint64_t pattern = 0x0123456789ABCDEFULL;

    ecc_write_with_ecc(test_addr, pattern);
    ecc_inject_error(test_addr, ECC_INJECT_1BIT, ECC_TARGET_ECC);
    uint64_t readback = ecc_read_with_check(test_addr);

    TEST_ASSERT_EQ("corrected == original", readback, pattern);

    ecc_status_t status = ecc_get_status();
    TEST_ASSERT("CE detected", status.ce_detected);
    TEST_ASSERT("no UE", !status.ue_detected);
    TEST_ASSERT("syndrome valid", status.syndrome != 0);

    TEST_END();
}
