#include "armv7m_internal.h"

#define FPU_NOCP (1u << 19)
#define SHCSR_MEMFAULTENA (1u << 16)
#define SHCSR_BUSFAULTENA (1u << 17)

static int privileged(const semu_cpu *cpu)
{
    return (cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK) != 0u ||
           (cpu->state.control & 1u) == 0u;
}

static semu_status address_at(semu_cpu *cpu, uint32_t base, unsigned offset,
                              uint32_t *address, semu_error *error)
{
    return armv7m_add_address(cpu, base, offset, address, error);
}

static semu_status context_fault(semu_cpu *cpu, unsigned status_bits,
                                 uint32_t address, semu_error *error)
{
    semu_status status;

    cpu->fp_context_fault = 1u;
    status = armv7m_request_fault(cpu, 5u, status_bits, address, 1, error);
    return status == SEMU_OK ? SEMU_ERR_STATE : status;
}

static semu_status validate_words(semu_cpu *cpu, uint32_t base,
                                  unsigned count, unsigned fault_bits,
                                  semu_error *error)
{
    unsigned index;

    for (index = 0u; index < count; ++index) {
        uint32_t address;
        semu_status status = address_at(cpu, base, index * 4u, &address,
                                        error);
        if (status != SEMU_OK) return status;
        status = semu_bus_validate_write(cpu->bus, address, 4u, error);
        if (status != SEMU_OK)
            return context_fault(cpu, fault_bits, address, error);
    }
    return SEMU_OK;
}

static semu_status write_word(semu_cpu *cpu, uint32_t address, uint32_t value,
                              unsigned fault_bits, semu_error *error)
{
    semu_status status = semu_bus_write(cpu->bus, address, 4u, value, error);
    if (status != SEMU_OK)
        return context_fault(cpu, fault_bits, address, error);
    return SEMU_OK;
}

static semu_status read_word(semu_cpu *cpu, uint32_t address, uint32_t *value,
                             semu_error *error)
{
    semu_status status = semu_bus_read(cpu->bus, address, 4u, value, error);
    if (status != SEMU_OK)
        return context_fault(cpu, ARMV7M_CFSR_BFSR_LSPERR, address, error);
    return SEMU_OK;
}

static void clear_lazy_state(semu_cpu *cpu)
{
    cpu->fpccr &= ~ARMV7M_FPCCR_STATUS_MASK;
    cpu->fpcar = 0u;
}

static void set_lazy_status(semu_cpu *cpu)
{
    unsigned current = cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK;
    int priority = current == 0u ? 0 : armv7m_exception_priority(cpu, current);
    uint32_t status = ARMV7M_FPCCR_LSPACT;

    if (!privileged(cpu)) status |= ARMV7M_FPCCR_USER;
    if (current == 0u) status |= ARMV7M_FPCCR_THREAD;
    if (priority > -1) status |= ARMV7M_FPCCR_HFRDY;
    if ((cpu->shcsr & SHCSR_BUSFAULTENA) != 0u &&
        priority > (int)cpu->system_priority[5u])
        status |= ARMV7M_FPCCR_BFRDY;
    if ((cpu->shcsr & SHCSR_MEMFAULTENA) != 0u &&
        priority > (int)cpu->system_priority[4u])
        status |= ARMV7M_FPCCR_MMRDY;
    cpu->fpccr = (cpu->fpccr & ~ARMV7M_FPCCR_STATUS_MASK) | status;
}

semu_status armv7m_fpu_context_prepare(semu_cpu *cpu, semu_error *error)
{
    uint32_t values[16];
    uint32_t base;
    uint32_t address;
    uint32_t fpscr_address;
    unsigned index;
    semu_status status;

    cpu->fp_context_fault = 0u;
    if ((cpu->fpccr & ARMV7M_FPCCR_LSPACT) == 0u) return SEMU_OK;
    base = cpu->fpcar;
    if ((base & 7u) != 0u)
        return context_fault(cpu, ARMV7M_CFSR_BFSR_LSPERR, base, error);
    for (index = 0u; index < 16u; ++index) {
        values[index] = cpu->state.s[index];
    }
    status = validate_words(cpu, base, 16u, ARMV7M_CFSR_BFSR_LSPERR, error);
    if (status != SEMU_OK) return status;
    status = address_at(cpu, base, 0x40u, &fpscr_address, error);
    if (status != SEMU_OK) return status;
    status = semu_bus_validate_write(cpu->bus, fpscr_address, 4u, error);
    if (status != SEMU_OK)
        return context_fault(cpu, ARMV7M_CFSR_BFSR_LSPERR, fpscr_address,
                             error);
    for (index = 0u; index < 16u; ++index) {
        status = address_at(cpu, base, index * 4u, &address, error);
        if (status != SEMU_OK) return status;
        status = write_word(cpu, address, values[index],
                            ARMV7M_CFSR_BFSR_LSPERR, error);
        if (status != SEMU_OK) return status;
    }
    status = write_word(cpu, fpscr_address, cpu->state.fpscr,
                        ARMV7M_CFSR_BFSR_LSPERR, error);
    if (status != SEMU_OK) return status;
    clear_lazy_state(cpu);
    return SEMU_OK;
}

void armv7m_fpu_context_note_use(semu_cpu *cpu)
{
    if ((cpu->fpccr & ARMV7M_FPCCR_ASPEN) != 0u) cpu->fpca = 1u;
}

semu_status armv7m_fpu_context_stack(semu_cpu *cpu, uint32_t frame_sp,
                                     int extended, semu_error *error)
{
    uint32_t address;
    semu_status status;
    unsigned index;

    if (!extended) return SEMU_OK;
    status = address_at(cpu, frame_sp, 0x20u, &address, error);
    if (status != SEMU_OK) return status;
    if ((cpu->fpccr & ARMV7M_FPCCR_LSPEN) != 0u) {
        cpu->fpcar = address;
        set_lazy_status(cpu);
        return SEMU_OK;
    }
    for (index = 0u; index < 16u; ++index) {
        status = address_at(cpu, frame_sp, 0x20u + index * 4u, &address,
                            error);
        if (status != SEMU_OK) return status;
        status = write_word(cpu, address, cpu->state.s[index],
                            ARMV7M_CFSR_BFSR_STKERR, error);
        if (status != SEMU_OK) return status;
    }
    status = address_at(cpu, frame_sp, 0x60u, &address, error);
    if (status != SEMU_OK) return status;
    status = write_word(cpu, address, cpu->state.fpscr,
                        ARMV7M_CFSR_BFSR_STKERR, error);
    if (status != SEMU_OK) return status;
    status = address_at(cpu, frame_sp, 0x64u, &address, error);
    if (status != SEMU_OK) return status;
    status = write_word(cpu, address, 0u, ARMV7M_CFSR_BFSR_STKERR, error);
    if (status != SEMU_OK) return status;
    for (index = 0u; index < 16u; ++index) cpu->state.s[index] = 0u;
    cpu->state.fpscr = 0u;
    clear_lazy_state(cpu);
    return SEMU_OK;
}

semu_status armv7m_fpu_context_unstack(semu_cpu *cpu, uint32_t frame_sp,
                                       int extended, semu_error *error)
{
    uint32_t values[16];
    uint32_t address;
    uint32_t fpscr;
    semu_status status;
    unsigned index;

    if (!extended) return SEMU_OK;
    if ((cpu->fpccr & ARMV7M_FPCCR_LSPACT) != 0u) {
        clear_lazy_state(cpu);
        return SEMU_OK;
    }
    if (!((((cpu->cpacr >> 20u) & 3u) == 3u &&
           ((cpu->cpacr >> 22u) & 3u) == 3u) ||
          (((cpu->cpacr >> 20u) & 3u) == 1u &&
           ((cpu->cpacr >> 22u) & 3u) == 1u && privileged(cpu)))) {
        status = armv7m_request_fault(cpu, 6u, FPU_NOCP, 0u, 0, error);
        return status;
    }
    for (index = 0u; index < 16u; ++index) {
        status = address_at(cpu, frame_sp, 0x20u + index * 4u, &address,
                            error);
        if (status != SEMU_OK) return status;
        status = read_word(cpu, address, &values[index], error);
        if (status != SEMU_OK) return status;
    }
    status = address_at(cpu, frame_sp, 0x60u, &address, error);
    if (status != SEMU_OK) return status;
    status = read_word(cpu, address, &fpscr, error);
    if (status != SEMU_OK) return status;
    for (index = 0u; index < 16u; ++index) cpu->state.s[index] = values[index];
    cpu->state.fpscr = fpscr;
    clear_lazy_state(cpu);
    return SEMU_OK;
}
