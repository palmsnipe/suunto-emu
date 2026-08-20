#include "armv7m_internal.h"

#include "../../core/bus_internal.h"

static semu_status refuse(uint32_t offset, semu_error *error)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "armv7m SCS refuses offset 0x%03x", offset);
    return SEMU_ERR_UNSUPPORTED;
}

static int privileged(const semu_cpu *cpu)
{
    return (cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK) != 0u ||
           (cpu->state.control & 1u) == 0u;
}

static semu_status complete(semu_status status, semu_error *error)
{
    if (status == SEMU_OK) {
        semu_error_clear(error);
    }
    return status;
}

semu_status armv7m_scs_read(void *context, uint32_t offset, unsigned width,
                            uint32_t *value, semu_error *error)
{
    semu_cpu *cpu = (semu_cpu *)context;
    semu_status status;

    if (cpu == NULL || !privileged(cpu)) return refuse(offset, error);
    if (offset >= 0x010u && offset < 0x020u) {
        status = armv7m_systick_read(cpu, offset, width, value, error);
    } else if (armv7m_nvic_read_offset(offset)) {
        status = armv7m_nvic_read(cpu, offset, width, value, error);
    } else if (armv7m_scb_offset(offset)) {
        status = armv7m_scb_read(cpu, offset, width, value, error);
    } else {
        status = semu_bus_read_below(cpu->bus, ARMV7M_SCS_BASE + offset,
                                     width, value, error);
        if (status != SEMU_OK) return refuse(offset, error);
    }
    return complete(status, error);
}

semu_status armv7m_scs_write(void *context, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error)
{
    semu_cpu *cpu = (semu_cpu *)context;
    semu_status status;

    if (cpu == NULL || !privileged(cpu)) return refuse(offset, error);
    if (offset >= 0x010u && offset < 0x020u) {
        status = armv7m_systick_write(cpu, offset, width, value, error);
    } else if (armv7m_nvic_write_offset(offset)) {
        status = armv7m_nvic_write(cpu, offset, width, value, error);
    } else if (armv7m_scb_offset(offset)) {
        status = armv7m_scb_write(cpu, offset, width, value, error);
    } else {
        status = semu_bus_write_below(cpu->bus, ARMV7M_SCS_BASE + offset,
                                      width, value, error);
        if (status != SEMU_OK) return refuse(offset, error);
    }
    return complete(status, error);
}
