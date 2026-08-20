#include "armv7m_internal.h"

#include "../../core/scheduler_internal.h"

static uint16_t count_pending_sources(const semu_cpu *cpu)
{
    uint16_t count = 0u;
    unsigned index;
    for (index = 0u; index < SEMU_ARRAY_LEN(cpu->system_pending); ++index) {
        if (cpu->system_pending[index] != 0u) ++count;
    }
    for (index = 0u; index < ARMV7M_IRQ_COUNT; ++index) {
        if (cpu->irq_level[index] != 0u || cpu->irq_pending[index] != 0u)
            ++count;
    }
    return count;
}

static int valid_binary(uint8_t value)
{
    return value <= 1u;
}

static int itstate_matches_xpsr(const semu_cpu *cpu)
{
    uint32_t xpsr_itstate = ((cpu->state.xpsr >> 25u) & 3u) |
                            ((cpu->state.xpsr >> 8u) & 0xfcu);
    return xpsr_itstate == cpu->itstate;
}

static int stack_pointer_matches_bank(const semu_cpu *cpu)
{
    unsigned exception = cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK;

    if (exception != 0u || (cpu->state.control & 2u) == 0u)
        return cpu->state.r[13] == cpu->state.msp;
    return cpu->state.r[13] == cpu->state.psp;
}

static int valid_core_mask_state(const semu_cpu_state *state)
{
    return (state->primask & ~UINT32_C(0x1)) == 0u &&
           (state->basepri & ~UINT32_C(0xff)) == 0u &&
           (state->faultmask & ~UINT32_C(0x1)) == 0u &&
           (state->control & ~UINT32_C(0x3)) == 0u;
}

static int valid_xpsr_state(uint32_t xpsr)
{
    return (xpsr & ~ARMV7M_XPSR_LIVE_MASK) == 0u;
}

static int valid_exclusive_width(unsigned width)
{
    return width == 0u || width == 1u || width == 2u || width == 4u;
}

static int valid_systick_state(const semu_cpu *cpu)
{
    if ((cpu->systick_control & ~UINT32_C(0x7)) != 0u ||
        cpu->systick_reload > UINT32_C(0x00ffffff) ||
        cpu->systick_current > UINT32_C(0x00ffffff) ||
        cpu->systick_calibration != 0u) {
        return 0;
    }
    if ((cpu->systick_event_valid == 0u && cpu->systick_event != 0u) ||
        (cpu->systick_event_valid != 0u &&
        (cpu->systick_event == 0u ||
         (cpu->systick_control & UINT32_C(0x5)) != UINT32_C(0x5) ||
         (cpu->systick_current == 0u && cpu->systick_reload == 0u)))) {
        return 0;
    }
    return 1;
}

static int valid_interrupt_state(const semu_cpu *cpu)
{
    unsigned index;

    for (index = 0u; index < ARMV7M_IRQ_COUNT; ++index) {
        unsigned source = (unsigned)((cpu->irq_source_bits[index / 64u] >>
                                      (index % 64u)) & 1u);
        unsigned expected = (cpu->irq_level[index] != 0u ||
                             cpu->irq_pending[index] != 0u) ? 1u : 0u;
        if (!valid_binary(cpu->irq_level[index]) ||
            !valid_binary(cpu->irq_enabled[index]) ||
            !valid_binary(cpu->irq_pending[index]) ||
            !valid_binary(cpu->irq_active[index]) ||
            (cpu->irq_priority[index] & ~ARMV7M_NVIC_PRIORITY_MASK) != 0u ||
            source != expected) {
            return 0;
        }
    }
    for (index = 0u; index < SEMU_ARRAY_LEN(cpu->system_pending); ++index) {
        if (!valid_binary(cpu->system_pending[index]) ||
            !valid_binary(cpu->system_active[index]) ||
            (cpu->system_priority[index] &
             (uint8_t)~ARMV7M_NVIC_PRIORITY_MASK) != 0u) {
            return 0;
        }
    }
    return 1;
}

static int valid_system_state(const semu_cpu *cpu)
{
    const uint32_t cfsr_mask = ARMV7M_CFSR_MUNSTKERR |
                               ARMV7M_CFSR_MSTKERR |
                               ARMV7M_CFSR_MLSPERR |
                               ARMV7M_CFSR_BFSR_IBUSERR |
                               ARMV7M_CFSR_BFSR_PRECISERR |
                               ARMV7M_CFSR_BFSR_IMPRECISERR |
                               ARMV7M_CFSR_BFSR_UNSTKERR |
                               ARMV7M_CFSR_BFSR_STKERR |
                               ARMV7M_CFSR_BFSR_LSPERR |
                               ARMV7M_CFSR_BFSR_BFARVALID |
                               ARMV7M_CFSR_UFSR_DIVBYZERO |
                               ARMV7M_CFSR_UFSR_UNALIGNED |
                               ARMV7M_CFSR_UFSR_INVPC |
                               ARMV7M_CFSR_UFSR_INVSTATE |
                               ARMV7M_CFSR_UFSR_UNDEFINSTR;

    return (cpu->vector_table & 0x7fu) == 0u &&
           (cpu->scr & ~((1u << 4) | (1u << 2) | (1u << 1))) == 0u &&
           (cpu->ccr & ~((1u << 9) | (1u << 8) | (1u << 4) |
                         (1u << 3))) == 0u &&
           (cpu->shcsr & 0x00070000u) == cpu->shcsr &&
           (cpu->cfsr & ~cfsr_mask) == 0u &&
           (cpu->hfsr & ~(1u << 30)) == 0u &&
           (cpu->cpacr & ~0x00f00000u) == 0u &&
           (cpu->fpccr & ~ARMV7M_FPCCR_READ_MASK) == 0u &&
           (cpu->fpcar & ~0xfffffff8u) == 0u &&
           (cpu->fpdscr & ~0x07c00000u) == 0u;
}

static semu_status write_state(const semu_cpu_state *state,
                               semu_snapshot_writer *writer,
                               semu_error *error)
{
    size_t i;
    semu_status status;
#define W(call) do { status = (call); if (status != SEMU_OK) return status; } while (0)
    for (i = 0u; i < SEMU_ARRAY_LEN(state->r); ++i)
        W(semu_snapshot_writer_u32(writer, state->r[i], error));
    W(semu_snapshot_writer_u32(writer, state->xpsr, error));
    W(semu_snapshot_writer_u32(writer, state->msp, error));
    W(semu_snapshot_writer_u32(writer, state->psp, error));
    W(semu_snapshot_writer_u32(writer, state->primask, error));
    W(semu_snapshot_writer_u32(writer, state->basepri, error));
    W(semu_snapshot_writer_u32(writer, state->faultmask, error));
    W(semu_snapshot_writer_u32(writer, state->control, error));
    W(semu_snapshot_writer_u32(writer, state->fpscr, error));
    for (i = 0u; i < SEMU_ARRAY_LEN(state->s); ++i)
        W(semu_snapshot_writer_u32(writer, state->s[i], error));
    W(semu_snapshot_writer_u64(writer, state->instructions, error));
    W(semu_snapshot_writer_u8(writer, (uint8_t)(state->waiting_for_interrupt != 0), error));
    W(semu_snapshot_writer_u8(writer, (uint8_t)(state->halted != 0), error));
#undef W
    return SEMU_OK;
}

static semu_status read_state(semu_cpu_state *state,
                              semu_snapshot_reader *reader, semu_error *error)
{
    size_t i;
    uint8_t value;
    semu_status status;
#define R(call) do { status = (call); if (status != SEMU_OK) return status; } while (0)
    for (i = 0u; i < SEMU_ARRAY_LEN(state->r); ++i)
        R(semu_snapshot_reader_u32(reader, &state->r[i], error));
    R(semu_snapshot_reader_u32(reader, &state->xpsr, error));
    R(semu_snapshot_reader_u32(reader, &state->msp, error));
    R(semu_snapshot_reader_u32(reader, &state->psp, error));
    R(semu_snapshot_reader_u32(reader, &state->primask, error));
    R(semu_snapshot_reader_u32(reader, &state->basepri, error));
    R(semu_snapshot_reader_u32(reader, &state->faultmask, error));
    R(semu_snapshot_reader_u32(reader, &state->control, error));
    R(semu_snapshot_reader_u32(reader, &state->fpscr, error));
    for (i = 0u; i < SEMU_ARRAY_LEN(state->s); ++i)
        R(semu_snapshot_reader_u32(reader, &state->s[i], error));
    R(semu_snapshot_reader_u64(reader, &state->instructions, error));
    R(semu_snapshot_reader_u8(reader, &value, error));
    if (value > 1u) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid CPU interrupt-wait flag");
        return SEMU_ERR_FORMAT;
    }
    state->waiting_for_interrupt = value != 0u;
    R(semu_snapshot_reader_u8(reader, &value, error));
    if (value > 1u) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid CPU halted flag");
        return SEMU_ERR_FORMAT;
    }
    state->halted = value != 0u;
#undef R
    return SEMU_OK;
}

semu_status semu_cpu_snapshot_write(const semu_cpu *cpu,
                                    semu_snapshot_writer *writer,
                                    semu_error *error)
{
    size_t i;
    semu_status status;
#define W(call) do { status = (call); if (status != SEMU_OK) return status; } while (0)
    if (cpu == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "CPU snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    W(write_state(&cpu->state, writer, error));
    W(semu_snapshot_writer_u32(writer, (uint32_t)cpu->stop_reason, error));
    W(semu_snapshot_writer_u32(writer, cpu->fault_instruction, error));
    W(semu_snapshot_writer_u32(writer, cpu->fault_address, error));
    W(semu_snapshot_writer_u8(writer, cpu->has_fault_address, error));
    W(semu_snapshot_writer_u32(writer, cpu->vector_table, error));
    W(semu_snapshot_writer_bytes(writer, cpu->irq_level,
                                 sizeof(cpu->irq_level), error));
    W(semu_snapshot_writer_bytes(writer, cpu->irq_enabled,
                                 sizeof(cpu->irq_enabled), error));
    W(semu_snapshot_writer_bytes(writer, cpu->irq_pending,
                                 sizeof(cpu->irq_pending), error));
    W(semu_snapshot_writer_bytes(writer, cpu->irq_active,
                                 sizeof(cpu->irq_active), error));
    W(semu_snapshot_writer_bytes(writer, cpu->irq_priority,
                                 sizeof(cpu->irq_priority), error));
    for (i = 0u; i < SEMU_ARRAY_LEN(cpu->irq_source_bits); ++i)
        W(semu_snapshot_writer_u64(writer, cpu->irq_source_bits[i], error));
    W(semu_snapshot_writer_bytes(writer, cpu->system_priority,
                                 sizeof(cpu->system_priority), error));
    W(semu_snapshot_writer_bytes(writer, cpu->system_pending,
                                 sizeof(cpu->system_pending), error));
    W(semu_snapshot_writer_bytes(writer, cpu->system_active,
                                 sizeof(cpu->system_active), error));
    W(semu_snapshot_writer_u16(writer, cpu->pending_source_count, error));
    W(semu_snapshot_writer_u8(writer, cpu->exception_depth, error));
    W(semu_snapshot_writer_u8(writer, cpu->prigroup, error));
    W(semu_snapshot_writer_u32(writer, cpu->scr, error));
    W(semu_snapshot_writer_u32(writer, cpu->ccr, error));
    W(semu_snapshot_writer_u32(writer, cpu->shcsr, error));
    W(semu_snapshot_writer_u32(writer, cpu->cfsr, error));
    W(semu_snapshot_writer_u32(writer, cpu->hfsr, error));
    W(semu_snapshot_writer_u32(writer, cpu->mmfar, error));
    W(semu_snapshot_writer_u32(writer, cpu->bfar, error));
    W(semu_snapshot_writer_u32(writer, cpu->cpacr, error));
    W(semu_snapshot_writer_u32(writer, cpu->fpccr, error));
    W(semu_snapshot_writer_u32(writer, cpu->fpcar, error));
    W(semu_snapshot_writer_u32(writer, cpu->fpdscr, error));
    W(semu_snapshot_writer_u8(writer, cpu->fpca, error));
    W(semu_snapshot_writer_u8(writer, cpu->fp_context_fault, error));
    W(semu_snapshot_writer_u8(writer, cpu->stack_fault_active, error));
    W(semu_snapshot_writer_u8(writer, cpu->bus_fault_active, error));
    W(semu_snapshot_writer_u8(writer, cpu->itstate, error));
    W(semu_snapshot_writer_u8(writer, cpu->event_register, error));
    W(semu_snapshot_writer_u8(writer, cpu->sleep_mode, error));
    W(semu_snapshot_writer_u8(writer, cpu->sleep_wake_source, error));
    W(semu_snapshot_writer_u8(writer, cpu->reset_requested, error));
    W(semu_snapshot_writer_u32(writer, cpu->systick_control, error));
    W(semu_snapshot_writer_u32(writer, cpu->systick_reload, error));
    W(semu_snapshot_writer_u32(writer, cpu->systick_current, error));
    W(semu_snapshot_writer_u32(writer, cpu->systick_calibration, error));
    W(semu_snapshot_writer_u64(writer, cpu->systick_last_time, error));
    W(semu_snapshot_writer_u8(writer, cpu->systick_countflag, error));
    W(semu_snapshot_writer_u64(writer, cpu->systick_event, error));
    W(semu_snapshot_writer_u8(writer, cpu->systick_event_valid, error));
    W(semu_snapshot_writer_u8(writer, cpu->stack_align, error));
    W(semu_snapshot_writer_u8(writer, cpu->exclusive_valid, error));
    W(semu_snapshot_writer_u32(writer, cpu->exclusive_address, error));
    W(semu_snapshot_writer_u32(writer, (uint32_t)cpu->exclusive_width, error));
#undef W
    return SEMU_OK;
}

semu_status semu_cpu_snapshot_read(semu_cpu *cpu,
                                   semu_snapshot_reader *reader,
                                   semu_error *error)
{
    semu_cpu candidate;
    uint32_t value;
    semu_status status;
#define R(call) do { status = (call); if (status != SEMU_OK) return status; } while (0)
    if (cpu == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "CPU snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *cpu;
    R(read_state(&candidate.state, reader, error));
    R(semu_snapshot_reader_u32(reader, &value, error));
    if (value > (uint32_t)SEMU_STOP_USER) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid CPU stop reason");
        return SEMU_ERR_FORMAT;
    }
    candidate.stop_reason = (semu_stop_reason)value;
    R(semu_snapshot_reader_u32(reader, &candidate.fault_instruction, error));
    R(semu_snapshot_reader_u32(reader, &candidate.fault_address, error));
    R(semu_snapshot_reader_u8(reader, &candidate.has_fault_address, error));
    if (candidate.has_fault_address > 1u) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid CPU fault-address flag");
        return SEMU_ERR_FORMAT;
    }
    R(semu_snapshot_reader_u32(reader, &candidate.vector_table, error));
    R(semu_snapshot_reader_bytes(reader, candidate.irq_level,
                                 sizeof(candidate.irq_level), error));
    R(semu_snapshot_reader_bytes(reader, candidate.irq_enabled,
                                 sizeof(candidate.irq_enabled), error));
    R(semu_snapshot_reader_bytes(reader, candidate.irq_pending,
                                 sizeof(candidate.irq_pending), error));
    R(semu_snapshot_reader_bytes(reader, candidate.irq_active,
                                 sizeof(candidate.irq_active), error));
    R(semu_snapshot_reader_bytes(reader, candidate.irq_priority,
                                 sizeof(candidate.irq_priority), error));
    for (value = 0u; value < SEMU_ARRAY_LEN(candidate.irq_source_bits); ++value)
        R(semu_snapshot_reader_u64(reader, &candidate.irq_source_bits[value], error));
    R(semu_snapshot_reader_bytes(reader, candidate.system_priority,
                                 sizeof(candidate.system_priority), error));
    R(semu_snapshot_reader_bytes(reader, candidate.system_pending,
                                 sizeof(candidate.system_pending), error));
    R(semu_snapshot_reader_bytes(reader, candidate.system_active,
                                 sizeof(candidate.system_active), error));
    R(semu_snapshot_reader_u16(reader, &candidate.pending_source_count, error));
    R(semu_snapshot_reader_u8(reader, &candidate.exception_depth, error));
    R(semu_snapshot_reader_u8(reader, &candidate.prigroup, error));
    if (candidate.prigroup > 7u) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid CPU priority grouping");
        return SEMU_ERR_FORMAT;
    }
    R(semu_snapshot_reader_u32(reader, &candidate.scr, error));
    R(semu_snapshot_reader_u32(reader, &candidate.ccr, error));
    R(semu_snapshot_reader_u32(reader, &candidate.shcsr, error));
    R(semu_snapshot_reader_u32(reader, &candidate.cfsr, error));
    R(semu_snapshot_reader_u32(reader, &candidate.hfsr, error));
    R(semu_snapshot_reader_u32(reader, &candidate.mmfar, error));
    R(semu_snapshot_reader_u32(reader, &candidate.bfar, error));
    R(semu_snapshot_reader_u32(reader, &candidate.cpacr, error));
    R(semu_snapshot_reader_u32(reader, &candidate.fpccr, error));
    R(semu_snapshot_reader_u32(reader, &candidate.fpcar, error));
    R(semu_snapshot_reader_u32(reader, &candidate.fpdscr, error));
    R(semu_snapshot_reader_u8(reader, &candidate.fpca, error));
    R(semu_snapshot_reader_u8(reader, &candidate.fp_context_fault, error));
    R(semu_snapshot_reader_u8(reader, &candidate.stack_fault_active, error));
    R(semu_snapshot_reader_u8(reader, &candidate.bus_fault_active, error));
    R(semu_snapshot_reader_u8(reader, &candidate.itstate, error));
    R(semu_snapshot_reader_u8(reader, &candidate.event_register, error));
    R(semu_snapshot_reader_u8(reader, &candidate.sleep_mode, error));
    R(semu_snapshot_reader_u8(reader, &candidate.sleep_wake_source, error));
    R(semu_snapshot_reader_u8(reader, &candidate.reset_requested, error));
    R(semu_snapshot_reader_u32(reader, &candidate.systick_control, error));
    R(semu_snapshot_reader_u32(reader, &candidate.systick_reload, error));
    R(semu_snapshot_reader_u32(reader, &candidate.systick_current, error));
    R(semu_snapshot_reader_u32(reader, &candidate.systick_calibration, error));
    R(semu_snapshot_reader_u64(reader, &candidate.systick_last_time, error));
    R(semu_snapshot_reader_u8(reader, &candidate.systick_countflag, error));
    R(semu_snapshot_reader_u64(reader, &candidate.systick_event, error));
    R(semu_snapshot_reader_u8(reader, &candidate.systick_event_valid, error));
    R(semu_snapshot_reader_u8(reader, &candidate.stack_align, error));
    R(semu_snapshot_reader_u8(reader, &candidate.exclusive_valid, error));
    R(semu_snapshot_reader_u32(reader, &candidate.exclusive_address, error));
    R(semu_snapshot_reader_u32(reader, &value, error));
    if (!valid_system_state(&candidate)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid CPU system register state");
        return SEMU_ERR_FORMAT;
    }
    if (!valid_core_mask_state(&candidate.state)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid CPU core register mask state");
        return SEMU_ERR_FORMAT;
    }
    if (!valid_xpsr_state(candidate.state.xpsr)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid CPU xPSR state");
        return SEMU_ERR_FORMAT;
    }
    if (!valid_interrupt_state(&candidate)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid CPU interrupt state");
        return SEMU_ERR_FORMAT;
    }
    if (!valid_binary(candidate.fpca) ||
        !valid_binary(candidate.fp_context_fault) ||
        !valid_binary(candidate.stack_fault_active) ||
        !valid_binary(candidate.bus_fault_active) ||
        !valid_binary(candidate.event_register) ||
        candidate.sleep_mode > ARMV7M_SLEEP_WFE ||
        candidate.sleep_wake_source > 3u ||
        !valid_binary(candidate.reset_requested) ||
        !valid_binary(candidate.systick_countflag) ||
        !valid_binary(candidate.systick_event_valid) ||
        !valid_binary(candidate.stack_align) ||
        !valid_binary(candidate.exclusive_valid)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid CPU snapshot state flag");
        return SEMU_ERR_FORMAT;
    }
    if (candidate.stack_align != (uint8_t)((candidate.ccr >> 9u) & 1u)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "CPU stack alignment does not match CCR");
        return SEMU_ERR_FORMAT;
    }
    if (!itstate_matches_xpsr(&candidate)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "CPU ITSTATE does not match xPSR");
        return SEMU_ERR_FORMAT;
    }
    if (!stack_pointer_matches_bank(&candidate)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "CPU stack pointer does not match active bank");
        return SEMU_ERR_FORMAT;
    }
    if ((candidate.state.waiting_for_interrupt != 0) !=
            (candidate.sleep_mode != ARMV7M_SLEEP_NONE) ||
        (candidate.sleep_mode != ARMV7M_SLEEP_NONE &&
         candidate.sleep_wake_source != 0u)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid CPU sleep state");
        return SEMU_ERR_FORMAT;
    }
    if (!valid_systick_state(&candidate)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid CPU SysTick state");
        return SEMU_ERR_FORMAT;
    }
    if (!valid_exclusive_width(value) ||
        (candidate.exclusive_valid == 0u && value != 0u) ||
        (candidate.exclusive_valid != 0u && value == 0u)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid CPU exclusive width");
        return SEMU_ERR_FORMAT;
    }
    candidate.exclusive_width = (unsigned)value;
    if (candidate.pending_source_count != count_pending_sources(&candidate)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid CPU pending-source count");
        return SEMU_ERR_FORMAT;
    }
    *cpu = candidate;
#undef R
    return SEMU_OK;
}

semu_status semu_cpu_snapshot_resolve_event(
    semu_cpu *cpu, uint32_t kind, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error)
{
    if (cpu == NULL || callback == NULL || context == NULL ||
        kind != SEMU_SCHED_EVENT_SYSTICK || subject != 0u ||
        cpu->systick_event_valid == 0u) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "CPU snapshot event is not present");
        return SEMU_ERR_CONFLICT;
    }
    *callback = armv7m_systick_event;
    *context = cpu;
    return SEMU_OK;
}
