/*
 * ECC Error Injection API - Software Simulation
 *
 * Provides a software-simulated ECC layer for platforms without
 * hardware ECC injection support (e.g. QEMU).
 *
 * Simulation approach:
 *   - ecc_write_with_ecc(): writes data AND records original value
 *   - ecc_inject_error(): flips 1 or 2 bits in the stored data
 *   - ecc_read_with_check(): reads data, compares with original,
 *     detects CE (1-bit) or UE (2-bit)
 *   - CE: corrects the data and returns the corrected value
 *   - UE: records the error; caller should check status
 *
 * On real hardware, replace this file with MMIO register operations
 * targeting the SoC's ECC controller.
 */
#ifndef _ECC_INJECT_H_
#define _ECC_INJECT_H_

#include "types.h"

/* Injection type */
#define ECC_INJECT_1BIT     0
#define ECC_INJECT_2BIT     1

/* Injection target */
#define ECC_TARGET_DATA     0
#define ECC_TARGET_ECC      1

/* Error status collected after a read */
typedef struct {
    bool     ce_detected;
    bool     ue_detected;
    uint64_t error_addr;
    uint32_t syndrome;
    uint64_t corrected_value;
} ecc_status_t;

/* ---- API ---- */
void         ecc_inject_init(void);
void         ecc_clear_status(void);
void         ecc_write_with_ecc(uintptr_t addr, uint64_t data);
void         ecc_inject_error(uintptr_t addr, int inject_type, int target_type);
uint64_t     ecc_read_with_check(uintptr_t addr);
ecc_status_t ecc_get_status(void);

#endif /* _ECC_INJECT_H_ */
