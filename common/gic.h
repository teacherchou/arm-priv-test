/*
 * ARM Privilege Test Framework - GICv3 Interface
 *
 * System register accessors for GICv3 CPU interface.
 * These registers are EL1-privileged and should trap from EL0.
 */
#ifndef _GIC_H_
#define _GIC_H_

#include "types.h"

/* ---- GICv3 CPU Interface System Registers ---- */

static inline uint64_t read_icc_iar1_el1(void)
{
    uint64_t val;
    asm volatile("mrs %0, S3_0_C12_C12_0" : "=r"(val)); /* ICC_IAR1_EL1 */
    return val;
}

static inline void write_icc_eoir1_el1(uint64_t val)
{
    asm volatile("msr S3_0_C12_C12_1, %0" :: "r"(val)); /* ICC_EOIR1_EL1 */
}

static inline uint64_t read_icc_pmr_el1(void)
{
    uint64_t val;
    asm volatile("mrs %0, S3_0_C4_C6_0" : "=r"(val)); /* ICC_PMR_EL1 */
    return val;
}

static inline void write_icc_pmr_el1(uint64_t val)
{
    asm volatile("msr S3_0_C4_C6_0, %0" :: "r"(val)); /* ICC_PMR_EL1 */
}

static inline uint64_t read_icc_bpr1_el1(void)
{
    uint64_t val;
    asm volatile("mrs %0, S3_0_C12_C12_3" : "=r"(val)); /* ICC_BPR1_EL1 */
    return val;
}

static inline void write_icc_bpr1_el1(uint64_t val)
{
    asm volatile("msr S3_0_C12_C12_3, %0" :: "r"(val)); /* ICC_BPR1_EL1 */
}

static inline uint64_t read_icc_igrpen1_el1(void)
{
    uint64_t val;
    asm volatile("mrs %0, S3_0_C12_C12_7" : "=r"(val)); /* ICC_IGRPEN1_EL1 */
    return val;
}

static inline void write_icc_igrpen1_el1(uint64_t val)
{
    asm volatile("msr S3_0_C12_C12_7, %0" :: "r"(val)); /* ICC_IGRPEN1_EL1 */
}

static inline uint64_t read_icc_hppir1_el1(void)
{
    uint64_t val;
    asm volatile("mrs %0, S3_0_C12_C12_2" : "=r"(val)); /* ICC_HPPIR1_EL1 */
    return val;
}

static inline void write_icc_sgi1r_el1(uint64_t val)
{
    asm volatile("msr S3_0_C12_C11_5, %0" :: "r"(val)); /* ICC_SGI1R_EL1 */
}

static inline uint64_t read_icc_ctlr_el1(void)
{
    uint64_t val;
    asm volatile("mrs %0, S3_0_C12_C12_4" : "=r"(val)); /* ICC_CTLR_EL1 */
    return val;
}

/* ---- Generic Timer Registers ---- */

static inline uint64_t read_cntp_ctl_el0(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntp_ctl_el0" : "=r"(val));
    return val;
}

static inline void write_cntp_ctl_el0(uint64_t val)
{
    asm volatile("msr cntp_ctl_el0, %0" :: "r"(val));
}

static inline uint64_t read_cntp_tval_el0(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntp_tval_el0" : "=r"(val));
    return val;
}

static inline void write_cntp_tval_el0(uint64_t val)
{
    asm volatile("msr cntp_tval_el0, %0" :: "r"(val));
}

static inline uint64_t read_cntv_ctl_el0(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntv_ctl_el0" : "=r"(val));
    return val;
}

static inline void write_cntv_ctl_el0(uint64_t val)
{
    asm volatile("msr cntv_ctl_el0, %0" :: "r"(val));
}

static inline uint64_t read_cntfrq_el0(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

static inline uint64_t read_cntvct_el0(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}

#endif /* _GIC_H_ */
