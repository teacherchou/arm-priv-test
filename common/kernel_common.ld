/*
 * ARM Privilege Test Framework - Common Linker Script Segments
 * Included by each extension's kernel.ld
 */

.text : {
    *(.text.boot)
    *(.text*)
} > RAM

.rodata : {
    *(.rodata*)
} > RAM

.test_table : {
    . = ALIGN(8);
    _test_table = .;
    KEEP(*(.test_table))
    _test_table_end = .;
} > RAM

.data : {
    *(.data*)
} > RAM

.bss (NOLOAD) : {
    . = ALIGN(16);
    __bss_start = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(16);
    __bss_end = .;
} > RAM

.stack (NOLOAD) : {
    . = ALIGN(16);
    __stack_start = .;
    . += 0x4000;        /* 16KB EL1 stack (main) */
    __stack_end = .;
} > RAM

.el0_stack (NOLOAD) : {
    . = ALIGN(16);
    . += 0x1000;        /* 4KB EL0 stack */
} > RAM

.el2_stack (NOLOAD) : {
    . = ALIGN(16);
    __el2_stack_start = .;
    . += 0x2000;        /* 8KB EL2 stack (for trap handlers) */
    __el2_stack_end = .;
} > RAM

.el3_stack (NOLOAD) : {
    . = ALIGN(16);
    __el3_stack_start = .;
    . += 0x2000;        /* 8KB EL3 stack (for trap handlers) */
    __el3_stack_end = .;
} > RAM
