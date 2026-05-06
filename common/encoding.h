/*
 * ARM Privilege Test Framework - Architecture Encoding Constants
 *
 * Exception levels, ESR encoding, exception class codes, etc.
 */
#ifndef _ENCODING_H_
#define _ENCODING_H_

/* ---- Exception Levels ---- */
#define EL0     0
#define EL1     1
#define EL2     2
#define EL3     3

/* ---- CurrentEL bit field ---- */
#define CURRENT_EL_SHIFT    2
#define CURRENT_EL_MASK     (0x3 << CURRENT_EL_SHIFT)

/* ---- SPSR bits ---- */
#define SPSR_MODE_EL0t      0x00
#define SPSR_MODE_EL1t      0x04
#define SPSR_MODE_EL1h      0x05
#define SPSR_MODE_EL2t      0x08
#define SPSR_MODE_EL2h      0x09
#define SPSR_MODE_EL3t      0x0C
#define SPSR_MODE_EL3h      0x0D
#define SPSR_MODE_MASK      0x0F

#define SPSR_F_BIT          BIT(6)
#define SPSR_I_BIT          BIT(7)
#define SPSR_A_BIT          BIT(8)
#define SPSR_D_BIT          BIT(9)
#define SPSR_DAIF_MASK      (SPSR_D_BIT | SPSR_A_BIT | SPSR_I_BIT | SPSR_F_BIT)

/* ---- ESR_ELx fields ---- */
#define ESR_EC_SHIFT        26
#define ESR_EC_MASK         (0x3FULL << ESR_EC_SHIFT)
#define ESR_ISS_MASK        0x1FFFFFF
#define ESR_IL_BIT          BIT(25)

/* ---- Exception Class (EC) values ---- */
#define EC_UNKNOWN          0x00    /* Unknown reason */
#define EC_WFI_WFE          0x01    /* Trapped WFI/WFE */
#define EC_MCR_MRC_CP15     0x03    /* Trapped MCR/MRC CP15 (AArch32) */
#define EC_MCRR_MRRC_CP15   0x04    /* Trapped MCRR/MRRC CP15 */
#define EC_MCR_MRC_CP14     0x05    /* Trapped MCR/MRC CP14 */
#define EC_LDC_STC_CP14     0x06    /* Trapped LDC/STC CP14 */
#define EC_FP_SIMD          0x07    /* Trapped FP/SIMD access */
#define EC_PAUTH            0x09    /* Trapped PAUTH */
#define EC_LD_ST_CP14       0x0C    /* Trapped LD/ST to CP14 */
#define EC_ILLEGAL_EXEC     0x0E    /* Illegal execution state */
#define EC_SVC_AARCH32      0x11    /* SVC from AArch32 */
#define EC_HVC_AARCH32      0x12    /* HVC from AArch32 */
#define EC_SMC_AARCH32      0x13    /* SMC from AArch32 */
#define EC_SVC_AARCH64      0x15    /* SVC from AArch64 */
#define EC_HVC_AARCH64      0x16    /* HVC from AArch64 */
#define EC_SMC_AARCH64      0x17    /* SMC from AArch64 */
#define EC_MSR_MRS_SYSTEM   0x18    /* MSR/MRS/System instruction trap */
#define EC_SVE              0x19    /* SVE access trap */
#define EC_PAC_FAIL         0x1C    /* PAC authentication failure */
#define EC_INST_ABORT_LOW   0x20    /* Instruction abort from lower EL */
#define EC_INST_ABORT_SAME  0x21    /* Instruction abort from same EL */
#define EC_PC_ALIGN          0x22   /* PC alignment fault */
#define EC_DATA_ABORT_LOW   0x24    /* Data abort from lower EL */
#define EC_DATA_ABORT_SAME  0x25    /* Data abort from same EL */
#define EC_SP_ALIGN         0x26    /* SP alignment fault */
#define EC_FP_AARCH32       0x28    /* Trapped FP exception AArch32 */
#define EC_FP_AARCH64       0x2C    /* Trapped FP exception AArch64 */
#define EC_SERROR           0x2F    /* SError interrupt */
#define EC_BRK_LOW          0x30    /* Breakpoint from lower EL */
#define EC_BRK_SAME         0x31    /* Breakpoint from same EL */
#define EC_SS_LOW           0x32    /* Software step from lower EL */
#define EC_SS_SAME          0x33    /* Software step from same EL */
#define EC_WATCHPT_LOW      0x34    /* Watchpoint from lower EL */
#define EC_WATCHPT_SAME     0x35    /* Watchpoint from same EL */
#define EC_BKPT_AARCH32     0x38    /* BKPT from AArch32 */
#define EC_BRK_AARCH64      0x3C    /* BRK from AArch64 */

/* ---- HCR_EL2 bits ---- */
#define HCR_RW_BIT          BIT(31)  /* EL1 is AArch64 */
#define HCR_TGE_BIT         BIT(27)  /* Trap general exceptions */
#define HCR_TVM_BIT         BIT(26)  /* Trap virtual memory controls */
#define HCR_TIDCP_BIT       BIT(20)  /* Trap impl-defined functionality */
#define HCR_TSC_BIT         BIT(19)  /* Trap SMC instruction */
#define HCR_TWE_BIT         BIT(14)  /* Trap WFE */
#define HCR_TWI_BIT         BIT(13)  /* Trap WFI */
#define HCR_AMO_BIT         BIT(5)   /* Physical SError routing */
#define HCR_IMO_BIT         BIT(4)   /* Physical IRQ routing */
#define HCR_FMO_BIT         BIT(3)   /* Physical FIQ routing */
#define HCR_SWIO_BIT        BIT(1)   /* Set/Way Invalidation Override */
#define HCR_VM_BIT          BIT(0)   /* Virtualization enable */

/* ---- SCR_EL3 bits ---- */
#define SCR_RW_BIT          BIT(10)  /* EL2 is AArch64 */
#define SCR_HCE_BIT         BIT(8)   /* HVC enable */
#define SCR_SMD_BIT         BIT(7)   /* SMC disable */
#define SCR_NS_BIT          BIT(0)   /* Non-secure */

/* ---- SCTLR_EL1 bits ---- */
#define SCTLR_M_BIT         BIT(0)   /* MMU enable */
#define SCTLR_A_BIT         BIT(1)   /* Alignment check */
#define SCTLR_C_BIT         BIT(2)   /* Data cache enable */
#define SCTLR_I_BIT         BIT(12)  /* Instruction cache enable */

/* ---- DAIF bits ---- */
#define DAIF_D_BIT           BIT(9)
#define DAIF_A_BIT           BIT(8)
#define DAIF_I_BIT           BIT(7)
#define DAIF_F_BIT           BIT(6)

/* ---- Exception vector offsets ---- */
#define VBAR_CUR_SP0_SYNC   0x000
#define VBAR_CUR_SP0_IRQ    0x080
#define VBAR_CUR_SP0_FIQ    0x100
#define VBAR_CUR_SP0_SERR   0x180
#define VBAR_CUR_SPX_SYNC   0x200
#define VBAR_CUR_SPX_IRQ    0x280
#define VBAR_CUR_SPX_FIQ    0x300
#define VBAR_CUR_SPX_SERR   0x380
#define VBAR_LOW_A64_SYNC   0x400
#define VBAR_LOW_A64_IRQ    0x480
#define VBAR_LOW_A64_FIQ    0x500
#define VBAR_LOW_A64_SERR   0x580
#define VBAR_LOW_A32_SYNC   0x600
#define VBAR_LOW_A32_IRQ    0x680
#define VBAR_LOW_A32_FIQ    0x700
#define VBAR_LOW_A32_SERR   0x780

#endif /* _ENCODING_H_ */
