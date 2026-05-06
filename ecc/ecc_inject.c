/*
 * ECC Error Injection - Software Simulation for QEMU
 *
 * Since QEMU has no real ECC hardware, this module simulates ECC
 * detection/correction using a shadow table:
 *
 *   - ecc_write_with_ecc: stores data + records original in shadow table
 *   - ecc_inject_error: flips 1 or 2 bits in the stored data
 *   - ecc_read_with_check: reads data, compares with original, detects errors
 *     - 1-bit difference → CE (correctable), returns corrected value
 *     - 2-bit difference → UE (uncorrectable)
 *
 * On real hardware, replace this file with MMIO register operations.
 */
#include "ecc_inject.h"
#include "uart.h"

#define ECC_MAX_ENTRIES  16

typedef struct {
    uintptr_t addr;
    uint64_t  original_data;
    uint64_t  stored_data;
    bool      in_use;
} ecc_entry_t;

static ecc_entry_t ecc_table[ECC_MAX_ENTRIES];
static ecc_status_t last_status;

static int popcount64(uint64_t value)
{
    int count = 0;
    while (value) {
        count += (int)(value & 1);
        value >>= 1;
    }
    return count;
}

static ecc_entry_t *find_entry(uintptr_t addr)
{
    for (int i = 0; i < ECC_MAX_ENTRIES; i++) {
        if (ecc_table[i].in_use && ecc_table[i].addr == addr)
            return &ecc_table[i];
    }
    return (ecc_entry_t *)0;
}

static ecc_entry_t *alloc_entry(uintptr_t addr)
{
    for (int i = 0; i < ECC_MAX_ENTRIES; i++) {
        if (!ecc_table[i].in_use) {
            ecc_table[i].in_use = true;
            ecc_table[i].addr = addr;
            return &ecc_table[i];
        }
    }
    return (ecc_entry_t *)0;
}

void ecc_inject_init(void)
{
    for (int i = 0; i < ECC_MAX_ENTRIES; i++)
        ecc_table[i].in_use = false;
    ecc_clear_status();
}

void ecc_clear_status(void)
{
    last_status.ce_detected = false;
    last_status.ue_detected = false;
    last_status.error_addr = 0;
    last_status.syndrome = 0;
    last_status.corrected_value = 0;
}

void ecc_write_with_ecc(uintptr_t addr, uint64_t data)
{
    *(volatile uint64_t *)addr = data;

    ecc_entry_t *entry = find_entry(addr);
    if (!entry)
        entry = alloc_entry(addr);
    if (entry) {
        entry->original_data = data;
        entry->stored_data = data;
    }
}

void ecc_inject_error(uintptr_t addr, int inject_type, int target_type)
{
    (void)target_type;

    ecc_entry_t *entry = find_entry(addr);
    if (!entry) {
        printf("  [ecc] WARNING: no ECC entry for addr 0x%lx\n",
               (unsigned long)addr);
        return;
    }

    uint64_t corrupted = entry->original_data;

    if (inject_type == ECC_INJECT_1BIT) {
        corrupted ^= (1ULL << 0);
    } else if (inject_type == ECC_INJECT_2BIT) {
        corrupted ^= (1ULL << 0) | (1ULL << 1);
    }

    *(volatile uint64_t *)addr = corrupted;
    entry->stored_data = corrupted;
}

uint64_t ecc_read_with_check(uintptr_t addr)
{
    ecc_clear_status();

    ecc_entry_t *entry = find_entry(addr);
    if (!entry)
        return *(volatile uint64_t *)addr;

    uint64_t raw = *(volatile uint64_t *)addr;
    uint64_t diff = raw ^ entry->original_data;
    int error_bits = popcount64(diff);

    last_status.error_addr = addr;

    if (error_bits == 0) {
        last_status.corrected_value = raw;
        return raw;
    } else if (error_bits == 1) {
        last_status.ce_detected = true;
        last_status.syndrome = (uint32_t)diff;
        last_status.corrected_value = entry->original_data;
        *(volatile uint64_t *)addr = entry->original_data;
        entry->stored_data = entry->original_data;
        return entry->original_data;
    } else {
        last_status.ue_detected = true;
        last_status.syndrome = (uint32_t)diff;
        last_status.corrected_value = raw;
        return raw;
    }
}

ecc_status_t ecc_get_status(void)
{
    return last_status;
}
