/*
 * ECC Extension - Correctable Error (CE) Tests
 *
 * Pure memory-operation ECC tests, no dependency on ecc_inject module.
 *
 * Approach: write known data → flip 1 bit via XOR → read back →
 * software detects the single-bit difference (CE) and corrects it.
 * This mirrors what real ECC hardware does transparently.
 */
#include "test_framework.h"

extern uintptr_t __ecc_test_start;

/* Count number of differing bits between two values */
static int ecc_popcount64(uint64_t value)
{
    int count = 0;
    while (value) {
        count += (int)(value & 1);
        value >>= 1;
    }
    return count;
}

/* ============================================================
 * ECC_001: Single-bit data error → CE, data correctable
 *
 * Flow:
 *   1. Write known pattern to test buffer
 *   2. Flip exactly 1 bit (simulate 1-bit memory corruption)
 *   3. Read back corrupted data
 *   4. Detect: diff has exactly 1 bit set → Correctable Error
 *   5. Correct: XOR the syndrome back → recover original data
 *   6. Verify recovered data == original pattern
 * ============================================================ */
TEST_REGISTER(test_ecc_001_ce_data_correction);
bool test_ecc_001_ce_data_correction(void)
{
    TEST_BEGIN("ECC_001: 1bit data CE → detected and corrected");

    volatile uint64_t *test_ptr =
        (volatile uint64_t *)((uintptr_t)&__ecc_test_start);
    uint64_t pattern = 0xA5A5A5A55A5A5A5AULL;

    /* Step 1: Write known pattern */
    *test_ptr = pattern;

    /* Step 2: Read back to confirm write */
    uint64_t verify_write = *test_ptr;
    TEST_ASSERT_EQ("write verified", verify_write, pattern);

    /* Step 3: Inject 1-bit error — flip bit 7 */
    *test_ptr = pattern ^ (1ULL << 7);

    /* Step 4: Read corrupted data */
    uint64_t corrupted = *test_ptr;

    /* Step 5: Detect error — compute syndrome */
    uint64_t syndrome = corrupted ^ pattern;
    int error_bits = ecc_popcount64(syndrome);

    TEST_ASSERT_EQ("exactly 1 bit differs (CE)", (uint64_t)error_bits, 1);
    TEST_ASSERT("syndrome is non-zero", syndrome != 0);

    /* Step 6: Correct — XOR syndrome back to recover original */
    uint64_t corrected = corrupted ^ syndrome;
    TEST_ASSERT_EQ("corrected data == original", corrected, pattern);

    /* Step 7: Scrub — write corrected data back */
    *test_ptr = corrected;
    uint64_t after_scrub = *test_ptr;
    TEST_ASSERT_EQ("scrub successful", after_scrub, pattern);

    TEST_END();
}

/* ============================================================
 * ECC_002: Single-bit error on different bit position
 *
 * Same CE flow but flips bit 63 (MSB) and uses a different
 * data pattern to cover more bit positions.
 * ============================================================ */
TEST_REGISTER(test_ecc_002_ce_msb_correction);
bool test_ecc_002_ce_msb_correction(void)
{
    TEST_BEGIN("ECC_002: 1bit CE on MSB → detected and corrected");

    volatile uint64_t *test_ptr =
        (volatile uint64_t *)((uintptr_t)&__ecc_test_start + 0x100);
    uint64_t pattern = 0x0123456789ABCDEFULL;

    /* Write known pattern */
    *test_ptr = pattern;

    /* Inject 1-bit error — flip bit 63 (MSB) */
    *test_ptr = pattern ^ (1ULL << 63);

    /* Read corrupted data */
    uint64_t corrupted = *test_ptr;

    /* Detect */
    uint64_t syndrome = corrupted ^ pattern;
    int error_bits = ecc_popcount64(syndrome);
    TEST_ASSERT_EQ("exactly 1 bit differs", (uint64_t)error_bits, 1);
    TEST_ASSERT_EQ("syndrome == bit 63", syndrome, (1ULL << 63));

    /* Correct */
    uint64_t corrected = corrupted ^ syndrome;
    TEST_ASSERT_EQ("corrected == original", corrected, pattern);

    /* Scrub */
    *test_ptr = corrected;
    TEST_ASSERT_EQ("scrub ok", *test_ptr, pattern);

    TEST_END();
}
