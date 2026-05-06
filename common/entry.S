/*
 * ARM Privilege Test Framework - Boot Entry
 *
 * Boot sequence: EL3 (or EL2/EL1) → setup stack → clear BSS → call main
 * Only CPU0 proceeds; secondary cores enter wfi loop.
 */

.section .text.boot
.global _entry
_entry:
    /* Only let CPU0 (MPIDR_EL1.Aff0 == 0) proceed */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF
    cbnz    x0, _secondary_spin

    /* Determine current EL and branch accordingly */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    cmp     x0, #3
    b.eq    _from_el3
    cmp     x0, #2
    b.eq    _from_el2
    cmp     x0, #1
    b.eq    _from_el1
    b       _secondary_spin         /* EL0 should never be boot entry */

/* ---- EL3 → EL2 → EL1 setup ---- */
_from_el3:
    /* Running at EL3h: 'mov sp' sets SP_EL3 (current SP) */
    adrp    x0, __el3_stack_end
    add     x0, x0, :lo12:__el3_stack_end
    mov     sp, x0                  /* SP_EL3 = __el3_stack_end */

    /* Pre-set SP_EL2 for later EL2 trap handlers (S3_6_C4_C1_0 = SP_EL2) */
    adrp    x0, __el2_stack_end
    add     x0, x0, :lo12:__el2_stack_end
    msr     S3_6_C4_C1_0, x0       /* SP_EL2 = __el2_stack_end */

    /* Configure SCR_EL3: RW=1 (EL2 is AArch64), NS=1, HCE=1, SMD=0 (allow SMC) */
    mov     x0, #(1 << 10)         /* SCR_EL3.RW */
    orr     x0, x0, #(1 << 0)     /* SCR_EL3.NS */
    orr     x0, x0, #(1 << 8)     /* SCR_EL3.HCE */
    msr     scr_el3, x0

    /* Set SPSR_EL3: Return to EL2h with DAIF masked */
    mov     x0, #0x3C9             /* EL2h (0x9) | DAIF masked (0x3C0) */
    msr     spsr_el3, x0

    /* Set return address to _from_el2 */
    adr     x0, _from_el2
    msr     elr_el3, x0

    /* Install EL3 vector table (for later use if needed) */
    adr     x0, _el3_vectors
    msr     vbar_el3, x0

    eret

_from_el2:
    /* Running at EL2h: 'mov sp' sets SP_EL2 (current SP).
     * If we came from _from_el3, SP_EL2 was already pre-set via MSR,
     * but if QEMU boots directly at EL2, we must set it here. */
    adrp    x0, __el2_stack_end
    add     x0, x0, :lo12:__el2_stack_end
    mov     sp, x0                  /* SP_EL2 = __el2_stack_end */

    /* Configure HCR_EL2: RW=1 (EL1 is AArch64) */
    mov     x0, #(1 << 31)         /* HCR_EL2.RW */
    msr     hcr_el2, x0

    /* Disable EL2 MMU and caches */
    msr     sctlr_el2, xzr

    /* Set SPSR_EL2: Return to EL1h with DAIF masked */
    mov     x0, #0x3C5             /* EL1h (0x5) | DAIF masked (0x3C0) */
    msr     spsr_el2, x0

    /* Set return address to _from_el1 */
    adr     x0, _from_el1
    msr     elr_el2, x0

    /* Install EL2 vector table */
    adr     x0, _el2_vectors
    msr     vbar_el2, x0

    eret

_from_el1:
    /* Disable EL1 MMU and caches */
    msr     sctlr_el1, xzr

    /* Install EL1 vector table */
    adr     x0, _el1_vectors
    msr     vbar_el1, x0

    /* Setup stack pointer */
    adrp    x0, __stack_end
    add     x0, x0, :lo12:__stack_end
    mov     sp, x0

    /* Clear BSS section */
    adrp    x0, __bss_start
    add     x0, x0, :lo12:__bss_start
    adrp    x1, __bss_end
    add     x1, x1, :lo12:__bss_end
_clear_bss:
    cmp     x0, x1
    b.ge    _bss_done
    str     xzr, [x0], #8
    b       _clear_bss
_bss_done:

    /* Call weak platform init hook (extensions can override) */
    bl      _platform_init

    /* Jump to main */
    bl      main

    /* If main returns, halt */
    b       _halt_pass

/* ---- Secondary cores spin ---- */
_secondary_spin:
    wfe
    b       _secondary_spin

/* ---- Halt routines ---- */
.global _halt_pass
_halt_pass:
    /* Platform-specific halt will be linked in; fallback is wfi */
    mov     x0, #0
    b       _halt_common

.global _halt_fail
_halt_fail:
    mov     x0, #1
    b       _halt_common

_halt_common:
    /* x0 = 0 for pass, 1 for fail */
    /* For QEMU virt, write to sysreg finisher device */
    /* This will be overridden by platform_model.h macros in C code */
    wfi
    b       _halt_common

/* ---- Weak platform init (overridable by extensions) ---- */
.weak _platform_init
_platform_init:
    ret
