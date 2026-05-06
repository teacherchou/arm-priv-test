/*
 * ecc extension - Linker Script
 */
ENTRY(_entry)

MEMORY {
    RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 64M
}

SECTIONS {
    INCLUDE ../common/kernel_common.ld

    /* ECC test buffer: 64KB, isolated from code/stack */
    .ecc_test_buf (NOLOAD) : {
        . = ALIGN(0x10000);
        PROVIDE(__ecc_test_start = .);
        . += 64K;
        PROVIDE(__ecc_test_end = .);
    } > RAM
}
