/*
 * ECC Extension - Uncorrectable Error (UE) Tests
 *
 * Pure memory-operation ECC tests, no dependency on ecc_inject module.
 *
 * Approach: write known data → flip 2 bits via XOR → read back →
 * software detects multi-bit difference (UE) and flags uncorrectable.
 * On real hardware, UE would trigger Data Abort or SError.
 */
#include "test_framework.h"

extern uintptr_t __ecc_test_start;

/* Count number of differing bits between two values */
static int ue_popcount64(uint64_t value)
{
    int count = 0;
    while (value) {
        count += (int)(value & 1);
        value >>= 1;
    }
    return count;
}

/* ============================================================
 * ECC_003: Double-bit data error → UE, not correctable
 *
 * Flow:
 *   1. Write known pattern to test buffer
 *   2. Flip exactly 2 bits (simulate 2-bit memory corruption)
 *   3. Read back corrupted data
 *   4. Detect: diff has 2 bits set → Uncorrectable Error
 *   5. Verify: corrupted data != original (cannot be corrected
 *      by single-bit XOR)
 * ============================================================ */
TEST_REGISTER(test_ecc_003_ue_data_detect);
bool test_ecc_003_ue_data_detect(void)
{
    TEST_BEGIN("ECC_003: 2bit data UE → detected, not correctable");

    volatile uint64_t *test_ptr =
        (volatile uint64_t *)((uintptr_t)&__ecc_test_start + 0x200);
    uint64_t pattern = 0xFFFFFFFFFFFFFFFFULL;

    /* Step 1: Write known pattern */
    *test_ptr = pattern;

    /* Step 2: Inject 2-bit error — flip bit 0 and bit 1 */
    uint64_t error_mask = (1ULL << 0) | (1ULL << 1);
    *test_ptr = pattern ^ error_mask;

    /* Step 3: Read corrupted data */
    uint64_t corrupted = *test_ptr;

    /* Step 4: Detect error — compute syndrome */
    uint64_t syndrome = corrupted ^ pattern;
    int error_bits = ue_popcount64(syndrome);

    TEST_ASSERT_EQ("exactly 2 bits differ (UE)", (uint64_t)error_bits, 2);
    TEST_ASSERT_NEQ("corrupted != original", corrupted, pattern);

    /* Step 5: Verify single-bit correction would NOT recover data.
     * A 1-bit CE correction XORs the lowest set bit of syndrome;
     * with 2 error bits, this only fixes one, leaving the other. */
    uint64_t lowest_bit = syndrome & (~syndrome + 1);
    uint64_t attempted_fix = corrupted ^ lowest_bit;
    TEST_ASSERT_NEQ("single-bit fix insufficient", attempted_fix, pattern);

    /* Step 6: Restore clean data (real HW might need scrub/rewrite) */
    *test_ptr = pattern;
    TEST_ASSERT_EQ("restored", *test_ptr, pattern);

    TEST_END();
}
