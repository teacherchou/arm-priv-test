/*
 * ARM Privilege Test Framework - Exception Vector Tables & Context Save/Restore
 *
 * ARM64 requires vector tables aligned to 2KB (0x800).
 * Each vector entry is 128 bytes (32 instructions).
 *
 * Since full context save/restore + handler call exceeds 128 bytes,
 * each vector entry contains only a branch to the actual handler stub
 * located outside the vector table.
 */

/* Exception type constants */
.equ EXC_SYNC,     0
.equ EXC_IRQ,      1
.equ EXC_FIQ,      2
.equ EXC_SERROR,   3

/* Exception source constants */
.equ SRC_CUR_SP0,  0
.equ SRC_CUR_SPX,  1
.equ SRC_LOW_A64,  2
.equ SRC_LOW_A32,  3

.equ SPSR_MODE_EL0t, 0x00

/* ---- Vector entry: just branch to handler stub ---- */
.macro VENTRY label
.balign 0x80
    b       \label
.endm


/* =============================================================
 * EL1 Vector Table
 * ============================================================= */
.balign 0x800
.global _el1_vectors
_el1_vectors:
    VENTRY  _el1_sync_cur_sp0
    VENTRY  _el1_irq_cur_sp0
    VENTRY  _el1_fiq_cur_sp0
    VENTRY  _el1_serr_cur_sp0
    VENTRY  _el1_sync_cur_spx
    VENTRY  _el1_irq_cur_spx
    VENTRY  _el1_fiq_cur_spx
    VENTRY  _el1_serr_cur_spx
    VENTRY  _el1_sync_low_a64
    VENTRY  _el1_irq_low_a64
    VENTRY  _el1_fiq_low_a64
    VENTRY  _el1_serr_low_a64
    VENTRY  _el1_sync_low_a32
    VENTRY  _el1_irq_low_a32
    VENTRY  _el1_fiq_low_a32
    VENTRY  _el1_serr_low_a32


/* =============================================================
 * EL2 Vector Table
 * ============================================================= */
.balign 0x800
.global _el2_vectors
_el2_vectors:
    VENTRY  _el2_sync_cur_sp0
    VENTRY  _el2_irq_cur_sp0
    VENTRY  _el2_fiq_cur_sp0
    VENTRY  _el2_serr_cur_sp0
    VENTRY  _el2_sync_cur_spx
    VENTRY  _el2_irq_cur_spx
    VENTRY  _el2_fiq_cur_spx
    VENTRY  _el2_serr_cur_spx
    VENTRY  _el2_sync_low_a64
    VENTRY  _el2_irq_low_a64
    VENTRY  _el2_fiq_low_a64
    VENTRY  _el2_serr_low_a64
    VENTRY  _el2_sync_low_a32
    VENTRY  _el2_irq_low_a32
    VENTRY  _el2_fiq_low_a32
    VENTRY  _el2_serr_low_a32


/* =============================================================
 * EL3 Vector Table
 * ============================================================= */
.balign 0x800
.global _el3_vectors
_el3_vectors:
    VENTRY  _el3_sync_cur_sp0
    VENTRY  _el3_irq_cur_sp0
    VENTRY  _el3_fiq_cur_sp0
    VENTRY  _el3_serr_cur_sp0
    VENTRY  _el3_sync_cur_spx
    VENTRY  _el3_irq_cur_spx
    VENTRY  _el3_fiq_cur_spx
    VENTRY  _el3_serr_cur_spx
    VENTRY  _el3_sync_low_a64
    VENTRY  _el3_irq_low_a64
    VENTRY  _el3_fiq_low_a64
    VENTRY  _el3_serr_low_a64
    VENTRY  _el3_sync_low_a32
    VENTRY  _el3_irq_low_a32
    VENTRY  _el3_fiq_low_a32
    VENTRY  _el3_serr_low_a32


/* =============================================================
 * Handler stubs: save context, set params, call C, restore, eret
 *
 * These live outside the vector table so they have no size limit.
 * ============================================================= */

.macro HANDLER_STUB name, c_handler, exc_type, exc_source
\name:
    /* Save x0-x30 on stack */
    sub     sp, sp, #(32 * 8)
    stp     x0, x1, [sp, #(0 * 8)]
    stp     x2, x3, [sp, #(2 * 8)]
    stp     x4, x5, [sp, #(4 * 8)]
    stp     x6, x7, [sp, #(6 * 8)]
    stp     x8, x9, [sp, #(8 * 8)]
    stp     x10, x11, [sp, #(10 * 8)]
    stp     x12, x13, [sp, #(12 * 8)]
    stp     x14, x15, [sp, #(14 * 8)]
    stp     x16, x17, [sp, #(16 * 8)]
    stp     x18, x19, [sp, #(18 * 8)]
    stp     x20, x21, [sp, #(20 * 8)]
    stp     x22, x23, [sp, #(22 * 8)]
    stp     x24, x25, [sp, #(24 * 8)]
    stp     x26, x27, [sp, #(26 * 8)]
    stp     x28, x29, [sp, #(28 * 8)]
    str     x30, [sp, #(30 * 8)]

    /* Call C handler with (exc_type, exc_source) */
    mov     x0, #\exc_type
    mov     x1, #\exc_source
    bl      \c_handler

    /* Restore x0-x30 */
    ldp     x0, x1, [sp, #(0 * 8)]
    ldp     x2, x3, [sp, #(2 * 8)]
    ldp     x4, x5, [sp, #(4 * 8)]
    ldp     x6, x7, [sp, #(6 * 8)]
    ldp     x8, x9, [sp, #(8 * 8)]
    ldp     x10, x11, [sp, #(10 * 8)]
    ldp     x12, x13, [sp, #(12 * 8)]
    ldp     x14, x15, [sp, #(14 * 8)]
    ldp     x16, x17, [sp, #(16 * 8)]
    ldp     x18, x19, [sp, #(18 * 8)]
    ldp     x20, x21, [sp, #(20 * 8)]
    ldp     x22, x23, [sp, #(22 * 8)]
    ldp     x24, x25, [sp, #(24 * 8)]
    ldp     x26, x27, [sp, #(26 * 8)]
    ldp     x28, x29, [sp, #(28 * 8)]
    ldr     x30, [sp, #(30 * 8)]
    add     sp, sp, #(32 * 8)
    eret
.endm

/* ---- EL1 handler stubs ---- */
HANDLER_STUB _el1_sync_cur_sp0,  el1_trap_handler, EXC_SYNC,   SRC_CUR_SP0
HANDLER_STUB _el1_irq_cur_sp0,   el1_trap_handler, EXC_IRQ,    SRC_CUR_SP0
HANDLER_STUB _el1_fiq_cur_sp0,   el1_trap_handler, EXC_FIQ,    SRC_CUR_SP0
HANDLER_STUB _el1_serr_cur_sp0,  el1_trap_handler, EXC_SERROR, SRC_CUR_SP0
HANDLER_STUB _el1_sync_cur_spx,  el1_trap_handler, EXC_SYNC,   SRC_CUR_SPX
HANDLER_STUB _el1_irq_cur_spx,   el1_trap_handler, EXC_IRQ,    SRC_CUR_SPX
HANDLER_STUB _el1_fiq_cur_spx,   el1_trap_handler, EXC_FIQ,    SRC_CUR_SPX
HANDLER_STUB _el1_serr_cur_spx,  el1_trap_handler, EXC_SERROR, SRC_CUR_SPX
HANDLER_STUB _el1_sync_low_a64,  el1_trap_handler, EXC_SYNC,   SRC_LOW_A64
HANDLER_STUB _el1_irq_low_a64,   el1_trap_handler, EXC_IRQ,    SRC_LOW_A64
HANDLER_STUB _el1_fiq_low_a64,   el1_trap_handler, EXC_FIQ,    SRC_LOW_A64
HANDLER_STUB _el1_serr_low_a64,  el1_trap_handler, EXC_SERROR, SRC_LOW_A64
HANDLER_STUB _el1_sync_low_a32,  el1_trap_handler, EXC_SYNC,   SRC_LOW_A32
HANDLER_STUB _el1_irq_low_a32,   el1_trap_handler, EXC_IRQ,    SRC_LOW_A32
HANDLER_STUB _el1_fiq_low_a32,   el1_trap_handler, EXC_FIQ,    SRC_LOW_A32
HANDLER_STUB _el1_serr_low_a32,  el1_trap_handler, EXC_SERROR, SRC_LOW_A32

/* ---- EL2 handler stubs ---- */
HANDLER_STUB _el2_sync_cur_sp0,  el2_trap_handler, EXC_SYNC,   SRC_CUR_SP0
HANDLER_STUB _el2_irq_cur_sp0,   el2_trap_handler, EXC_IRQ,    SRC_CUR_SP0
HANDLER_STUB _el2_fiq_cur_sp0,   el2_trap_handler, EXC_FIQ,    SRC_CUR_SP0
HANDLER_STUB _el2_serr_cur_sp0,  el2_trap_handler, EXC_SERROR, SRC_CUR_SP0
HANDLER_STUB _el2_sync_cur_spx,  el2_trap_handler, EXC_SYNC,   SRC_CUR_SPX
HANDLER_STUB _el2_irq_cur_spx,   el2_trap_handler, EXC_IRQ,    SRC_CUR_SPX
HANDLER_STUB _el2_fiq_cur_spx,   el2_trap_handler, EXC_FIQ,    SRC_CUR_SPX
HANDLER_STUB _el2_serr_cur_spx,  el2_trap_handler, EXC_SERROR, SRC_CUR_SPX
HANDLER_STUB _el2_sync_low_a64,  el2_trap_handler, EXC_SYNC,   SRC_LOW_A64
HANDLER_STUB _el2_irq_low_a64,   el2_trap_handler, EXC_IRQ,    SRC_LOW_A64
HANDLER_STUB _el2_fiq_low_a64,   el2_trap_handler, EXC_FIQ,    SRC_LOW_A64
HANDLER_STUB _el2_serr_low_a64,  el2_trap_handler, EXC_SERROR, SRC_LOW_A64
HANDLER_STUB _el2_sync_low_a32,  el2_trap_handler, EXC_SYNC,   SRC_LOW_A32
HANDLER_STUB _el2_irq_low_a32,   el2_trap_handler, EXC_IRQ,    SRC_LOW_A32
HANDLER_STUB _el2_fiq_low_a32,   el2_trap_handler, EXC_FIQ,    SRC_LOW_A32
HANDLER_STUB _el2_serr_low_a32,  el2_trap_handler, EXC_SERROR, SRC_LOW_A32

/* ---- EL3 handler stubs ---- */
HANDLER_STUB _el3_sync_cur_sp0,  el3_trap_handler, EXC_SYNC,   SRC_CUR_SP0
HANDLER_STUB _el3_irq_cur_sp0,   el3_trap_handler, EXC_IRQ,    SRC_CUR_SP0
HANDLER_STUB _el3_fiq_cur_sp0,   el3_trap_handler, EXC_FIQ,    SRC_CUR_SP0
HANDLER_STUB _el3_serr_cur_sp0,  el3_trap_handler, EXC_SERROR, SRC_CUR_SP0
HANDLER_STUB _el3_sync_cur_spx,  el3_trap_handler, EXC_SYNC,   SRC_CUR_SPX
HANDLER_STUB _el3_irq_cur_spx,   el3_trap_handler, EXC_IRQ,    SRC_CUR_SPX
HANDLER_STUB _el3_fiq_cur_spx,   el3_trap_handler, EXC_FIQ,    SRC_CUR_SPX
HANDLER_STUB _el3_serr_cur_spx,  el3_trap_handler, EXC_SERROR, SRC_CUR_SPX
HANDLER_STUB _el3_sync_low_a64,  el3_trap_handler, EXC_SYNC,   SRC_LOW_A64
HANDLER_STUB _el3_irq_low_a64,   el3_trap_handler, EXC_IRQ,    SRC_LOW_A64
HANDLER_STUB _el3_fiq_low_a64,   el3_trap_handler, EXC_FIQ,    SRC_LOW_A64
HANDLER_STUB _el3_serr_low_a64,  el3_trap_handler, EXC_SERROR, SRC_LOW_A64
HANDLER_STUB _el3_sync_low_a32,  el3_trap_handler, EXC_SYNC,   SRC_LOW_A32
HANDLER_STUB _el3_irq_low_a32,   el3_trap_handler, EXC_IRQ,    SRC_LOW_A32
HANDLER_STUB _el3_fiq_low_a32,   el3_trap_handler, EXC_FIQ,    SRC_LOW_A32
HANDLER_STUB _el3_serr_low_a32,  el3_trap_handler, EXC_SERROR, SRC_LOW_A32


/* =============================================================
 * EL switch helper: goto_el0
 * Called from EL1; sets up SPSR_EL1 + ELR_EL1 and erets to EL0.
 * x0 = target function address, x1 = EL0 stack pointer
 * ============================================================= */
.global _goto_el0
_goto_el0:
    msr     sp_el0, x1              /* Set EL0 stack */
    mov     x2, #SPSR_MODE_EL0t     /* EL0t, no DAIF mask */
    msr     spsr_el1, x2
    msr     elr_el1, x0             /* Return to target function */
    eret
