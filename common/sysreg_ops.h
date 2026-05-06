/*
 * ARM Privilege Test Framework - System Register Operation Helpers
 *
 * Inline assembly wrappers for MRS/MSR operations.
 * Each function executes exactly one A64 instruction (4 bytes),
 * so the trap handler can reliably skip it via ELR += 4.
 */
#ifndef _SYSREG_OPS_H_
#define _SYSREG_OPS_H_

#include "types.h"

/*
 * Generic MRS/MSR macros.
 * ARM inline asm requires system register names as string literals,
 * so we use macros rather than functions for the actual access.
 */
#define SYSREG_READ(reg, dest) \
    asm volatile("mrs %0, " #reg : "=r"(dest))

#define SYSREG_WRITE(reg, val) \
    asm volatile("msr " #reg ", %0" :: "r"(val))

/* ---- EL1 system registers ---- */

static inline uint64_t read_sctlr_el1(void)
{
    uint64_t val;
    SYSREG_READ(sctlr_el1, val);
    return val;
}

static inline void write_sctlr_el1(uint64_t val)
{
    SYSREG_WRITE(sctlr_el1, val);
}

static inline uint64_t read_ttbr0_el1(void)
{
    uint64_t val;
    SYSREG_READ(ttbr0_el1, val);
    return val;
}

static inline void write_ttbr0_el1(uint64_t val)
{
    SYSREG_WRITE(ttbr0_el1, val);
}

static inline uint64_t read_ttbr1_el1(void)
{
    uint64_t val;
    SYSREG_READ(ttbr1_el1, val);
    return val;
}

static inline uint64_t read_vbar_el1(void)
{
    uint64_t val;
    SYSREG_READ(vbar_el1, val);
    return val;
}

static inline void write_vbar_el1(uint64_t val)
{
    SYSREG_WRITE(vbar_el1, val);
}

static inline uint64_t read_tcr_el1(void)
{
    uint64_t val;
    SYSREG_READ(tcr_el1, val);
    return val;
}

static inline void write_tcr_el1(uint64_t val)
{
    SYSREG_WRITE(tcr_el1, val);
}

static inline uint64_t read_mair_el1(void)
{
    uint64_t val;
    SYSREG_READ(mair_el1, val);
    return val;
}

static inline void write_mair_el1(uint64_t val)
{
    SYSREG_WRITE(mair_el1, val);
}

static inline uint64_t read_esr_el1(void)
{
    uint64_t val;
    SYSREG_READ(esr_el1, val);
    return val;
}

static inline uint64_t read_elr_el1(void)
{
    uint64_t val;
    SYSREG_READ(elr_el1, val);
    return val;
}

static inline uint64_t read_spsr_el1(void)
{
    uint64_t val;
    SYSREG_READ(spsr_el1, val);
    return val;
}

static inline uint64_t read_far_el1(void)
{
    uint64_t val;
    SYSREG_READ(far_el1, val);
    return val;
}

/* ---- EL2 system registers ---- */

static inline uint64_t read_hcr_el2(void)
{
    uint64_t val;
    SYSREG_READ(hcr_el2, val);
    return val;
}

static inline void write_hcr_el2(uint64_t val)
{
    SYSREG_WRITE(hcr_el2, val);
}

static inline uint64_t read_sctlr_el2(void)
{
    uint64_t val;
    SYSREG_READ(sctlr_el2, val);
    return val;
}

static inline void write_sctlr_el2(uint64_t val)
{
    SYSREG_WRITE(sctlr_el2, val);
}

static inline uint64_t read_vbar_el2(void)
{
    uint64_t val;
    SYSREG_READ(vbar_el2, val);
    return val;
}

static inline void write_vbar_el2(uint64_t val)
{
    SYSREG_WRITE(vbar_el2, val);
}

static inline uint64_t read_esr_el2(void)
{
    uint64_t val;
    SYSREG_READ(esr_el2, val);
    return val;
}

static inline uint64_t read_elr_el2(void)
{
    uint64_t val;
    SYSREG_READ(elr_el2, val);
    return val;
}

static inline uint64_t read_spsr_el2(void)
{
    uint64_t val;
    SYSREG_READ(spsr_el2, val);
    return val;
}

static inline uint64_t read_far_el2(void)
{
    uint64_t val;
    SYSREG_READ(far_el2, val);
    return val;
}

static inline uint64_t read_vtcr_el2(void)
{
    uint64_t val;
    SYSREG_READ(vtcr_el2, val);
    return val;
}

static inline uint64_t read_vttbr_el2(void)
{
    uint64_t val;
    SYSREG_READ(vttbr_el2, val);
    return val;
}

/* ---- EL3 system registers ---- */

static inline uint64_t read_scr_el3(void)
{
    uint64_t val;
    SYSREG_READ(scr_el3, val);
    return val;
}

static inline void write_scr_el3(uint64_t val)
{
    SYSREG_WRITE(scr_el3, val);
}

static inline uint64_t read_sctlr_el3(void)
{
    uint64_t val;
    SYSREG_READ(sctlr_el3, val);
    return val;
}

static inline void write_sctlr_el3(uint64_t val)
{
    SYSREG_WRITE(sctlr_el3, val);
}

static inline uint64_t read_vbar_el3(void)
{
    uint64_t val;
    SYSREG_READ(vbar_el3, val);
    return val;
}

static inline void write_vbar_el3(uint64_t val)
{
    SYSREG_WRITE(vbar_el3, val);
}

static inline uint64_t read_esr_el3(void)
{
    uint64_t val;
    SYSREG_READ(esr_el3, val);
    return val;
}

static inline uint64_t read_elr_el3(void)
{
    uint64_t val;
    SYSREG_READ(elr_el3, val);
    return val;
}

static inline uint64_t read_spsr_el3(void)
{
    uint64_t val;
    SYSREG_READ(spsr_el3, val);
    return val;
}

static inline uint64_t read_far_el3(void)
{
    uint64_t val;
    SYSREG_READ(far_el3, val);
    return val;
}

/* ---- EL0-accessible registers ---- */

static inline uint64_t read_current_el(void)
{
    uint64_t val;
    SYSREG_READ(CurrentEL, val);
    return (val >> 2) & 0x3;
}

static inline uint64_t read_daif(void)
{
    uint64_t val;
    SYSREG_READ(daif, val);
    return val;
}

static inline uint64_t read_tpidr_el0(void)
{
    uint64_t val;
    SYSREG_READ(tpidr_el0, val);
    return val;
}

static inline void write_tpidr_el0(uint64_t val)
{
    SYSREG_WRITE(tpidr_el0, val);
}

/* ---- DAIF mask operations ---- */

static inline void daif_set(void)
{
    asm volatile("msr daifset, #0xf");
}

static inline void daif_clr(void)
{
    asm volatile("msr daifclr, #0xf");
}

/* ---- TLB / Cache maintenance (privileged) ---- */

static inline void tlbi_vmalle1(void)
{
    asm volatile("tlbi vmalle1");
}

static inline void ic_iallu(void)
{
    asm volatile("ic iallu");
}

static inline void dc_civac(uint64_t addr)
{
    asm volatile("dc civac, %0" :: "r"(addr));
}

/* ---- Special instructions ---- */

static inline void wfi(void)
{
    asm volatile("wfi");
}

static inline void wfe(void)
{
    asm volatile("wfe");
}

static inline void execute_eret(void)
{
    asm volatile("eret");
}

static inline void execute_svc(uint16_t imm)
{
    asm volatile("svc %0" :: "i"(imm));
}

static inline void execute_hvc(uint16_t imm)
{
    asm volatile("hvc %0" :: "i"(imm));
}

#endif /* _SYSREG_OPS_H_ */
