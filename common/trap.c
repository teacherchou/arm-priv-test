/*
 * ARM Privilege Test Framework - Trap Handler Implementation
 *
 * Core mechanism: when trap_record.armed is true, the handler records
 * exception details (ESR, ELR, FAR, EC) and advances past the
 * faulting instruction so execution can resume.
 *
 * ARM distinguishes "called" vs "faulting" exceptions for ELR:
 *   - Called exceptions reach their natural target EL; ELR already
 *     points past the instruction (SVC→EL1, HVC→EL2, SMC→EL3).
 *   - Faulting / routed exceptions (e.g. SMC trapped to EL2 by TSC)
 *     have ELR pointing at the instruction; handler must advance +4.
 *
 * When not armed, unexpected traps print diagnostics and halt.
 */
#include "trap.h"
#include "privilege.h"
#include "uart.h"
#include "encoding.h"

/* Global trap record */
trap_record_t trap_record;

void trap_expect_begin(void)
{
    trap_record.armed = true;
    trap_record.triggered = false;
    trap_record.esr = 0;
    trap_record.elr = 0;
    trap_record.far_el = 0;
    trap_record.exception_class = 0;
    trap_record.target_el = 0;
    trap_record.exc_type = 0;
    trap_record.exc_source = 0;
}

void trap_expect_end(void)
{
    trap_record.armed = false;
}

void trap_record_reset(void)
{
    trap_record.armed = false;
    trap_record.triggered = false;
    trap_record.esr = 0;
    trap_record.elr = 0;
    trap_record.far_el = 0;
    trap_record.exception_class = 0;
    trap_record.target_el = 0;
    trap_record.exc_type = 0;
    trap_record.exc_source = 0;
}

static const char *exc_type_name(uint32_t type)
{
    switch (type) {
    case EXC_SYNC:   return "Synchronous";
    case EXC_IRQ:    return "IRQ";
    case EXC_FIQ:    return "FIQ";
    case EXC_SERROR: return "SError";
    default:         return "Unknown";
    }
}

static const char *exc_source_name(uint32_t source)
{
    switch (source) {
    case SRC_CUR_SP0: return "Current EL SP0";
    case SRC_CUR_SPX: return "Current EL SPx";
    case SRC_LOW_A64: return "Lower EL AArch64";
    case SRC_LOW_A32: return "Lower EL AArch32";
    default:          return "Unknown";
    }
}

static const char *ec_name(uint32_t ec)
{
    switch (ec) {
    case EC_UNKNOWN:        return "Unknown";
    case EC_WFI_WFE:        return "WFI/WFE trap";
    case EC_SVC_AARCH64:    return "SVC (AArch64)";
    case EC_HVC_AARCH64:    return "HVC (AArch64)";
    case EC_SMC_AARCH64:    return "SMC (AArch64)";
    case EC_MSR_MRS_SYSTEM: return "MSR/MRS/System instruction";
    case EC_INST_ABORT_LOW: return "Instruction abort (lower EL)";
    case EC_DATA_ABORT_LOW: return "Data abort (lower EL)";
    case EC_ILLEGAL_EXEC:   return "Illegal execution state";
    case EC_PC_ALIGN:       return "PC alignment fault";
    case EC_SP_ALIGN:       return "SP alignment fault";
    default:                return "Other";
    }
}

/*
 * Read ESR/ELR/FAR from the specified handler EL.
 */
static void read_exception_regs(uint32_t handler_el,
                                uint64_t *esr, uint64_t *elr, uint64_t *far_val)
{
    switch (handler_el) {
    case 1:
        asm volatile("mrs %0, esr_el1" : "=r"(*esr));
        asm volatile("mrs %0, elr_el1" : "=r"(*elr));
        asm volatile("mrs %0, far_el1" : "=r"(*far_val));
        break;
    case 2:
        asm volatile("mrs %0, esr_el2" : "=r"(*esr));
        asm volatile("mrs %0, elr_el2" : "=r"(*elr));
        asm volatile("mrs %0, far_el2" : "=r"(*far_val));
        break;
    case 3:
        asm volatile("mrs %0, esr_el3" : "=r"(*esr));
        asm volatile("mrs %0, elr_el3" : "=r"(*elr));
        asm volatile("mrs %0, far_el3" : "=r"(*far_val));
        break;
    default:
        *esr = 0; *elr = 0; *far_val = 0;
        break;
    }
}

/*
 * Set ELR for the specified handler EL.
 */
static void write_elr(uint32_t handler_el, uint64_t elr)
{
    switch (handler_el) {
    case 1: asm volatile("msr elr_el1, %0" :: "r"(elr)); break;
    case 2: asm volatile("msr elr_el2, %0" :: "r"(elr)); break;
    case 3: asm volatile("msr elr_el3, %0" :: "r"(elr)); break;
    }
}

/*
 * Set SPSR for the specified handler EL to return to EL1h.
 */
static void set_spsr_to_el1h(uint32_t handler_el)
{
    uint64_t spsr;
    switch (handler_el) {
    case 1:
        asm volatile("mrs %0, spsr_el1" : "=r"(spsr));
        spsr = (spsr & ~0xFULL) | SPSR_MODE_EL1h;
        asm volatile("msr spsr_el1, %0" :: "r"(spsr));
        break;
    case 2:
        asm volatile("mrs %0, spsr_el2" : "=r"(spsr));
        spsr = (spsr & ~0xFULL) | SPSR_MODE_EL1h;
        asm volatile("msr spsr_el2, %0" :: "r"(spsr));
        break;
    case 3:
        asm volatile("mrs %0, spsr_el3" : "=r"(spsr));
        spsr = (spsr & ~0xFULL) | SPSR_MODE_EL1h;
        asm volatile("msr spsr_el3, %0" :: "r"(spsr));
        break;
    }
}

/*
 * Common trap handling logic for any EL.
 * handler_el: the EL that is handling this exception (1, 2, or 3).
 *
 * Priority order:
 *   1a. SVC #1 (EC=0x15) → EL0→EL1 return protocol
 *   1b. HVC #2 (EC=0x16) → EL1→EL2 roundtrip protocol (run fn at EL2, ERET back)
 *   1c. SMC #3 (EC=0x17) → EL1→EL3 roundtrip protocol (run fn at EL3, ERET back)
 *   2.  Armed trap → record and skip instruction
 *   3.  Unexpected → diagnose and halt
 */
static void handle_trap_common(uint32_t handler_el,
                               uint64_t exc_type,
                               uint64_t exc_source)
{
    uint64_t esr, elr, far_val;

    read_exception_regs(handler_el, &esr, &elr, &far_val);

    uint32_t exception_class = (uint32_t)((esr >> ESR_EC_SHIFT) & 0x3F);
    uint32_t iss = (uint32_t)(esr & ESR_ISS_MASK);

    /*
     * Priority 1a: SVC #1 — EL0→EL1 return protocol.
     * When EL0 code calls "svc #0x0001", we return to saved EL1 context.
     */
    if (exc_type == EXC_SYNC && exception_class == EC_SVC_AARCH64
        && (iss & 0xFFFF) == SVC_RETURN_TO_EL1) {
        write_elr(handler_el, _el1_return_addr);
        set_spsr_to_el1h(handler_el);
        return;
    }

    /*
     * Priority 1b: HVC #2 — EL1→EL2 roundtrip protocol.
     * EL1 code calls "hvc #0x0002" to enter EL2. If _trampoline_fn is set,
     * call it at EL2, then ERET back to EL1 (ELR already points past HVC).
     */
    if (exc_type == EXC_SYNC && exception_class == EC_HVC_AARCH64
        && handler_el == 2 && (iss & 0xFFFF) == HVC_RETURN_TO_EL1) {
        if (_trampoline_fn) {
            void (*fn)(uint64_t) = (void (*)(uint64_t))_trampoline_fn;
            uint64_t arg = _trampoline_arg;
            fn(arg);
        }
        /* ELR_EL2 already points to the instruction after HVC, just ERET */
        return;
    }

    /*
     * Priority 1c: SMC #3 — EL1→EL3 roundtrip protocol.
     * EL1 code calls "smc #0x0003" to enter EL3. Same pattern as HVC.
     */
    if (exc_type == EXC_SYNC && exception_class == EC_SMC_AARCH64
        && handler_el == 3 && (iss & 0xFFFF) == SMC_RETURN_TO_EL1) {
        if (_trampoline_fn) {
            void (*fn)(uint64_t) = (void (*)(uint64_t))_trampoline_fn;
            uint64_t arg = _trampoline_arg;
            fn(arg);
        }
        /* ELR_EL3 already points to the instruction after SMC, just ERET */
        return;
    }

    /*
     * Priority 2: Armed trap — test expects an exception here.
     * Record details and skip the faulting instruction.
     *
     * ARM distinguishes "called" vs "faulting" exceptions for ELR:
     *   - Called exceptions: ELR points PAST the instruction → no skip.
     *     Only when the exception reaches its *natural* target EL:
     *       · HVC → EL2 (called), but HVC trapped elsewhere → faulting
     *       · SMC → EL3 (called), but SMC trapped to EL2 via TSC → faulting
     *       · SVC → EL1 (always called)
     *   - Faulting exceptions: ELR points AT the instruction → skip +4.
     */
    if (trap_record.armed) {
        trap_record.triggered = true;
        trap_record.esr = esr;
        trap_record.elr = elr;
        trap_record.far_el = far_val;
        trap_record.exception_class = exception_class;
        trap_record.target_el = handler_el;
        trap_record.exc_type = (uint32_t)exc_type;
        trap_record.exc_source = (uint32_t)exc_source;

        if (exc_type == EXC_SYNC) {
            /*
             * Called exception = reaches its natural target EL:
             *   SVC  → always called (target EL1)
             *   HVC  → called only when handler_el == 2
             *   SMC  → called only when handler_el == 3
             * If routed elsewhere (e.g. SMC trapped to EL2 by TSC),
             * it is a faulting exception and ELR needs +4.
             */
            bool is_called_exception =
                (exception_class == EC_SVC_AARCH64) ||
                (exception_class == EC_HVC_AARCH64 && handler_el == 2) ||
                (exception_class == EC_SMC_AARCH64 && handler_el == 3);
            if (!is_called_exception) {
                write_elr(handler_el, elr + 4);
            }
        }
        return;
    }

    /* Priority 3: Unexpected trap — print diagnostics and halt */
    printf("\n!!! UNEXPECTED EXCEPTION at EL%d !!!\n", handler_el);
    printf("  Type:   %s\n", exc_type_name((uint32_t)exc_type));
    printf("  Source: %s\n", exc_source_name((uint32_t)exc_source));
    printf("  ESR:    0x%lx\n", esr);
    printf("  EC:     0x%x (%s)\n", exception_class, ec_name(exception_class));
    printf("  ELR:    0x%lx\n", elr);
    printf("  FAR:    0x%lx\n", far_val);

    PLATFORM_HALT_FAIL();
}

/* Per-EL handlers called from trap_asm.S */
void el1_trap_handler(uint64_t exc_type, uint64_t exc_source)
{
    handle_trap_common(1, exc_type, exc_source);
}

void el2_trap_handler(uint64_t exc_type, uint64_t exc_source)
{
    handle_trap_common(2, exc_type, exc_source);
}

void el3_trap_handler(uint64_t exc_type, uint64_t exc_source)
{
    handle_trap_common(3, exc_type, exc_source);
}
