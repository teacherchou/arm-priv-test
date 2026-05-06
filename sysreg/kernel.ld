/*
 * sysreg extension - Linker Script
 */
ENTRY(_entry)

MEMORY {
    RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 64M
}

SECTIONS {
    INCLUDE ../common/kernel_common.ld
}
