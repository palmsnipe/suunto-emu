#ifndef SEMU_ARMV7M_INTERNAL_H
#define SEMU_ARMV7M_INTERNAL_H

#include "semu/cpu.h"
#include "../../core/scheduler_internal.h"
#include "../../core/snapshot_io.h"

#define ARMV7M_IRQ_COUNT 256u
#define ARMV7M_IMPLEMENTED_IRQ_COUNT 240u
#define ARMV7M_IRQ_SOURCE_WORDS 4u
#define ARMV7M_SCS_BASE 0xe000e000u
#define ARMV7M_SCS_SIZE 0x1000u
#define ARMV7M_XPSR_N (1u << 31)
#define ARMV7M_XPSR_Z (1u << 30)
#define ARMV7M_XPSR_C (1u << 29)
#define ARMV7M_XPSR_V (1u << 28)
#define ARMV7M_XPSR_STACK_ALIGN (1u << 9)
#define ARMV7M_XPSR_T (1u << 24)
#define ARMV7M_XPSR_APSR_MASK 0xf80f0000u
#define ARMV7M_XPSR_EPSR_MASK 0x0700fc00u
#define ARMV7M_XPSR_IPSR_MASK 0x000001ffu
#define ARMV7M_XPSR_LIVE_MASK (ARMV7M_XPSR_APSR_MASK | \
                               ARMV7M_XPSR_EPSR_MASK | \
                               ARMV7M_XPSR_IPSR_MASK)
#define ARMV7M_SLEEP_NONE 0u
#define ARMV7M_SLEEP_WFI 1u
#define ARMV7M_SLEEP_WFE 2u
#define ARMV7M_NVIC_PRIORITY_MASK 0xe0u
#define ARMV7M_FPCCR_LSPACT (1u << 0)
#define ARMV7M_FPCCR_USER (1u << 1)
#define ARMV7M_FPCCR_THREAD (1u << 3)
#define ARMV7M_FPCCR_HFRDY (1u << 4)
#define ARMV7M_FPCCR_MMRDY (1u << 5)
#define ARMV7M_FPCCR_BFRDY (1u << 6)
#define ARMV7M_FPCCR_MONRDY (1u << 8)
#define ARMV7M_FPCCR_LSPEN (1u << 30)
#define ARMV7M_FPCCR_ASPEN (1u << 31)
#define ARMV7M_FPCCR_CONTROL_MASK (ARMV7M_FPCCR_ASPEN | \
                                   ARMV7M_FPCCR_LSPEN)
#define ARMV7M_FPCCR_STATUS_MASK (ARMV7M_FPCCR_LSPACT | \
                                  ARMV7M_FPCCR_USER | \
                                  ARMV7M_FPCCR_THREAD | \
                                  ARMV7M_FPCCR_HFRDY | \
                                  ARMV7M_FPCCR_MMRDY | \
                                  ARMV7M_FPCCR_BFRDY | \
                                  ARMV7M_FPCCR_MONRDY)
#define ARMV7M_FPCCR_READ_MASK (ARMV7M_FPCCR_CONTROL_MASK | \
                                ARMV7M_FPCCR_STATUS_MASK)
#define ARMV7M_CFSR_MUNSTKERR (1u << 3)
#define ARMV7M_CFSR_MSTKERR (1u << 4)
#define ARMV7M_CFSR_MLSPERR (1u << 5)
#define ARMV7M_CFSR_BFSR_IBUSERR (1u << 8)
#define ARMV7M_CFSR_BFSR_PRECISERR (1u << 9)
#define ARMV7M_CFSR_BFSR_IMPRECISERR (1u << 10)
#define ARMV7M_CFSR_BFSR_UNSTKERR (1u << 11)
#define ARMV7M_CFSR_BFSR_STKERR (1u << 12)
#define ARMV7M_CFSR_BFSR_LSPERR (1u << 13)
#define ARMV7M_CFSR_BFSR_BFARVALID (1u << 15)
#define ARMV7M_CFSR_UFSR_DIVBYZERO (1u << 16)
#define ARMV7M_CFSR_UFSR_UNALIGNED (1u << 17)
#define ARMV7M_CFSR_UFSR_INVPC (1u << 18)
#define ARMV7M_CFSR_UFSR_INVSTATE (1u << 19)
#define ARMV7M_CFSR_UFSR_UNDEFINSTR (1u << 20)

struct semu_cpu {
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_cpu_state state;
    semu_stop_reason stop_reason;
    uint32_t fault_instruction;
    uint32_t fault_address;
    uint8_t has_fault_address;
    uint32_t vector_table;
    uint8_t irq_level[ARMV7M_IRQ_COUNT];
    uint8_t irq_enabled[ARMV7M_IRQ_COUNT];
    uint8_t irq_pending[ARMV7M_IRQ_COUNT];
    uint8_t irq_active[ARMV7M_IRQ_COUNT];
    uint8_t irq_priority[ARMV7M_IRQ_COUNT];
    /* One bit per IRQ that has an asserted line or pending latch. */
    uint64_t irq_source_bits[ARMV7M_IRQ_SOURCE_WORDS];
    uint8_t system_priority[16];
    uint8_t system_pending[16];
    uint8_t system_active[16];
    uint16_t pending_source_count;
    uint8_t exception_depth;
    uint8_t prigroup;
    uint32_t scr;
    uint32_t ccr;
    uint32_t shcsr;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t mmfar;
    uint32_t bfar;
    uint32_t cpacr;
    uint32_t fpccr;
    uint32_t fpcar;
    uint32_t fpdscr;
    uint8_t fpca;
    uint8_t fp_context_fault;
    uint8_t stack_fault_active;
    uint8_t bus_fault_active;
    uint8_t itstate;
    uint8_t event_register;
    uint8_t sleep_mode;
    uint8_t sleep_wake_source;
    uint8_t reset_requested;
    uint32_t systick_control;
    uint32_t systick_reload;
    uint32_t systick_current;
    uint32_t systick_calibration;
    uint64_t systick_last_time;
    uint8_t systick_countflag;
    semu_event_id systick_event;
    uint8_t systick_event_valid;
    uint8_t stack_align;
    uint8_t exclusive_valid;
    uint32_t exclusive_address;
    unsigned exclusive_width;
};

/*
 * Forward declarations used by the inline helpers defined below.
 * armv7m_request_fault is defined in scb.c; semu_bus_read_u16 is
 * defined in core/bus_access.c.
 */
semu_status armv7m_request_fault(semu_cpu *cpu, unsigned exception,
                                 uint32_t status_bits, uint32_t address,
                                 int address_valid, semu_error *error);
semu_status semu_bus_read_u16(semu_bus *bus, uint32_t address,
                              uint32_t *value, semu_error *error);

semu_status semu_cpu_snapshot_write(const semu_cpu *cpu,
                                    semu_snapshot_writer *writer,
                                    semu_error *error);
semu_status semu_cpu_snapshot_read(semu_cpu *cpu,
                                   semu_snapshot_reader *reader,
                                   semu_error *error);
semu_status semu_cpu_snapshot_resolve_event(
    semu_cpu *cpu, uint32_t kind, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error);
semu_status semu_cpu_snapshot_event_id_matches(
    const semu_cpu *cpu, uint32_t kind, uint32_t subject,
    semu_event_id event_id, semu_error *error);
semu_status semu_cpu_snapshot_event_links_match(
    const semu_cpu *cpu, const semu_scheduled_event_state *events,
    size_t count, semu_error *error);

/*
 * Hot-path helpers defined as static inline so the compiler can eliminate
 * call/return overhead on the per-instruction execution path.  These were
 * previously regular functions in support.c; they are moved here because
 * they are called from many translation units on the inner loop.
 */

static inline uint32_t armv7m_reg(const semu_cpu *cpu, unsigned reg,
                                  uint32_t pc)
{
    if (reg == 15u) {
        return pc + 4u;
    }
    return cpu->state.r[reg];
}

static inline void armv7m_set_sp(semu_cpu *cpu, uint32_t value)
{
    cpu->state.r[13] = value;
    if ((cpu->state.control & 2u) != 0u &&
        (cpu->state.xpsr & 0x1ffu) == 0u) {
        cpu->state.psp = value;
    } else {
        cpu->state.msp = value;
    }
}

static inline void armv7m_set_nz(semu_cpu *cpu, uint32_t value)
{
    cpu->state.xpsr &= ~(ARMV7M_XPSR_N | ARMV7M_XPSR_Z);
    if ((value & 0x80000000u) != 0u) {
        cpu->state.xpsr |= ARMV7M_XPSR_N;
    }
    if (value == 0u) {
        cpu->state.xpsr |= ARMV7M_XPSR_Z;
    }
}

static inline uint32_t armv7m_add(semu_cpu *cpu, uint32_t left,
                                  uint32_t right, unsigned carry,
                                  int update_flags)
{
    uint64_t wide = (uint64_t)left + (uint64_t)right + (uint64_t)carry;
    uint32_t result = (uint32_t)wide;
    uint32_t overflow = (~(left ^ right) & (left ^ result)) >> 31;

    if (update_flags) {
        armv7m_set_nz(cpu, result);
        cpu->state.xpsr &= ~(ARMV7M_XPSR_C | ARMV7M_XPSR_V);
        if ((wide >> 32) != 0u) {
            cpu->state.xpsr |= ARMV7M_XPSR_C;
        }
        if (overflow != 0u) {
            cpu->state.xpsr |= ARMV7M_XPSR_V;
        }
    }
    return result;
}

static inline int32_t armv7m_sign_extend(uint32_t value, unsigned bits)
{
    uint32_t sign = 1u << (bits - 1u);
    uint32_t mask = (1u << bits) - 1u;

    value &= mask;
    if ((value & sign) != 0u) {
        uint32_t magnitude = ((~value) & mask) + 1u;
        return -(int32_t)magnitude;
    }
    return (int32_t)value;
}

static inline int armv7m_condition_passed(const semu_cpu *cpu,
                                           unsigned condition)
{
    int n = (cpu->state.xpsr & ARMV7M_XPSR_N) != 0u;
    int z = (cpu->state.xpsr & ARMV7M_XPSR_Z) != 0u;
    int c = (cpu->state.xpsr & ARMV7M_XPSR_C) != 0u;
    int v = (cpu->state.xpsr & ARMV7M_XPSR_V) != 0u;

    switch (condition & 15u) {
    case 0u: return z;
    case 1u: return !z;
    case 2u: return c;
    case 3u: return !c;
    case 4u: return n;
    case 5u: return !n;
    case 6u: return v;
    case 7u: return !v;
    case 8u: return c && !z;
    case 9u: return !c || z;
    case 10u: return n == v;
    case 11u: return n != v;
    case 12u: return !z && (n == v);
    case 13u: return z || (n != v);
    case 14u: return 1;
    default: return 0;
    }
}

/*
 * Bus fault handling for fetch and data accesses.  Inlined here so the
 * fetch fast path in semu_cpu_step avoids an extra call into support.c.
 */
static inline int armv7m_scs_address(uint32_t address)
{
    return address >= ARMV7M_SCS_BASE &&
           address - ARMV7M_SCS_BASE < ARMV7M_SCS_SIZE;
}

static inline void armv7m_request_bus_fault(semu_cpu *cpu, uint32_t address,
                                            semu_status status)
{
    if (status == SEMU_ERR_UNSUPPORTED &&
        armv7m_scs_address(address)) {
        cpu->state.halted = 1;
        cpu->stop_reason = SEMU_STOP_UNSUPPORTED_INSTRUCTION;
        return;
    }
    if (cpu->bus_fault_active != 0u) {
        cpu->state.halted = 1;
        cpu->stop_reason = SEMU_STOP_UNMAPPED_ACCESS;
        cpu->fault_address = address;
        cpu->has_fault_address = 1u;
        cpu->cfsr |= ARMV7M_CFSR_BFSR_PRECISERR |
                      ARMV7M_CFSR_BFSR_BFARVALID;
        return;
    }
    cpu->bus_fault_active = 1u;
    (void)armv7m_request_fault(cpu, 5u,
                               ARMV7M_CFSR_BFSR_PRECISERR |
                               ARMV7M_CFSR_BFSR_BFARVALID,
                               address, 1, NULL);
    cpu->bus_fault_active = 0u;
}

static inline semu_status armv7m_fetch16(semu_cpu *cpu, uint32_t address,
                                         uint32_t *value, semu_error *error)
{
    semu_status status =
        semu_bus_read_u16(cpu->bus, address, value, error);
    if (status != SEMU_OK) {
        armv7m_request_bus_fault(cpu, address, status);
    }
    return status;
}

semu_status armv7m_exec16(semu_cpu *cpu, uint16_t instruction,
                          uint32_t pc, semu_error *error);
semu_status armv7m_exec16_arith(semu_cpu *cpu, uint16_t instruction,
                                uint32_t pc, semu_error *error);
semu_status armv7m_exec16_control(semu_cpu *cpu, uint16_t instruction,
                                  uint32_t pc, semu_error *error);
semu_status armv7m_exec32(semu_cpu *cpu, uint16_t first, uint16_t second,
                          uint32_t pc, semu_error *error);
semu_status armv7m_exec32_data(semu_cpu *cpu, uint16_t first,
                               uint16_t second, uint32_t pc,
                               semu_error *error);
semu_status armv7m_exec32_memory(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, uint32_t pc,
                                 semu_error *error);
semu_status armv7m_exec32_dsp(semu_cpu *cpu, uint16_t first,
                              uint16_t second, uint32_t pc,
                              semu_error *error);
semu_status armv7m_exec32_shift(semu_cpu *cpu, uint16_t first,
                                uint16_t second, uint32_t pc,
                                semu_error *error);
semu_status armv7m_exec32_system(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, uint32_t pc,
                                 semu_error *error);
semu_status armv7m_exec32_fpu(semu_cpu *cpu, uint16_t first,
                              uint16_t second, uint32_t pc,
                              semu_error *error);
semu_status armv7m_fpu_check_access(semu_cpu *cpu, semu_error *error);
semu_status armv7m_fpu_arith(semu_cpu *cpu, uint16_t first,
                             uint16_t second, semu_error *error);
semu_status armv7m_fpu_convert(semu_cpu *cpu, uint16_t first,
                               uint16_t second, semu_error *error);
semu_status armv7m_fpu_transfer(semu_cpu *cpu, uint16_t first,
                                uint16_t second, uint32_t pc,
                                semu_error *error);
semu_status armv7m_fpu_context_prepare(semu_cpu *cpu, semu_error *error);
void armv7m_fpu_context_note_use(semu_cpu *cpu);
semu_status armv7m_fpu_context_stack(semu_cpu *cpu, uint32_t frame_sp,
                                     int extended, semu_error *error);
semu_status armv7m_fpu_context_unstack(semu_cpu *cpu, uint32_t frame_sp,
                                       int extended, semu_error *error);

semu_status armv7m_read(semu_cpu *cpu, uint32_t address, unsigned width,
                        uint32_t *value, semu_error *error);
semu_status armv7m_write(semu_cpu *cpu, uint32_t address, unsigned width,
                         uint32_t value, semu_error *error);
semu_status armv7m_validate_write(semu_cpu *cpu, uint32_t address,
                                  unsigned width, semu_error *error);
semu_status armv7m_unsupported(semu_cpu *cpu, uint32_t instruction,
                              semu_error *error);
semu_status armv7m_take_exception(semu_cpu *cpu, unsigned exception,
                                  semu_error *error);
semu_status armv7m_branch_exchange(semu_cpu *cpu, uint32_t target,
                                   semu_error *error);
semu_status armv7m_exec16_memory(semu_cpu *cpu, uint16_t instruction,
                                 uint32_t pc, semu_error *error);
void armv7m_set_itstate(semu_cpu *cpu, uint8_t value);
unsigned armv7m_bit_count(uint32_t value);
uint32_t armv7m_shift(semu_cpu *cpu, uint32_t value, unsigned type,
                      unsigned amount, int immediate);
uint32_t armv7m_expand_modified_immediate(uint16_t first,
                                          uint16_t second);
uint32_t armv7m_shifted_register(uint32_t value, uint16_t second);

void armv7m_clear_exclusive(semu_cpu *cpu);
void armv7m_set_exclusive(semu_cpu *cpu, uint32_t address, unsigned width);
int armv7m_exclusive_matches(const semu_cpu *cpu, uint32_t address,
                             unsigned width);
void armv7m_note_local_store(semu_cpu *cpu, uint32_t address,
                             unsigned width);
semu_status armv7m_address_fault(semu_cpu *cpu, uint32_t address,
                                 semu_error *error);
semu_status armv7m_add_address(semu_cpu *cpu, uint32_t base,
                               uint32_t offset, uint32_t *address,
                               semu_error *error);
semu_status armv7m_literal_base(semu_cpu *cpu, uint32_t pc, uint32_t *base,
                                semu_error *error);
semu_status armv7m_exec32_memory_exclusive(semu_cpu *cpu, uint16_t first,
                                            uint16_t second, uint32_t pc,
                                            semu_error *error);

semu_status armv7m_scs_read(void *context, uint32_t offset, unsigned width,
                            uint32_t *value, semu_error *error);
semu_status armv7m_scs_write(void *context, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error);
int armv7m_scb_offset(uint32_t offset);
semu_status armv7m_scb_read(semu_cpu *cpu, uint32_t offset, unsigned width,
                            uint32_t *value, semu_error *error);
semu_status armv7m_scb_write(semu_cpu *cpu, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error);
semu_status armv7m_systick_read(semu_cpu *cpu, uint32_t offset,
                                unsigned width, uint32_t *value,
                                semu_error *error);
semu_status armv7m_systick_write(semu_cpu *cpu, uint32_t offset,
                                 unsigned width, uint32_t value,
                                 semu_error *error);
void armv7m_systick_reset(semu_cpu *cpu);
void armv7m_systick_reschedule(semu_cpu *cpu);
void armv7m_systick_event(void *context, uint64_t now_ns);
semu_status armv7m_nvic_read(semu_cpu *cpu, uint32_t offset, unsigned width,
                             uint32_t *value, semu_error *error);
semu_status armv7m_nvic_write(semu_cpu *cpu, uint32_t offset, unsigned width,
                              uint32_t value, semu_error *error);
int armv7m_nvic_read_offset(uint32_t offset);
int armv7m_nvic_write_offset(uint32_t offset);
void armv7m_scs_reset(void *context);
int armv7m_pending_exception(const semu_cpu *cpu);
int armv7m_pending_exception_for_icsr(const semu_cpu *cpu);
int armv7m_exception_priority(const semu_cpu *cpu, unsigned exception);
int armv7m_exception_masked(const semu_cpu *cpu, unsigned exception);
int armv7m_exception_can_preempt(const semu_cpu *cpu, unsigned exception);
void armv7m_exception_entered(semu_cpu *cpu, unsigned exception);
void armv7m_exception_returned(semu_cpu *cpu, unsigned exception);
void armv7m_set_system_pending(semu_cpu *cpu, unsigned exception);
void armv7m_clear_system_pending(semu_cpu *cpu, unsigned exception);
void armv7m_set_irq_pending(semu_cpu *cpu, unsigned irq);
void armv7m_clear_irq_pending(semu_cpu *cpu, unsigned irq);
void armv7m_signal_pending_event(semu_cpu *cpu, unsigned exception);
int armv7m_pending_wake(const semu_cpu *cpu);
void armv7m_sleep_wfi(semu_cpu *cpu);
void armv7m_sleep_wfe(semu_cpu *cpu);
void armv7m_sleep_event(semu_cpu *cpu);
semu_status armv7m_sleep_step(semu_cpu *cpu, semu_error *error);
void armv7m_sleep_on_exception_return(semu_cpu *cpu);

#endif
