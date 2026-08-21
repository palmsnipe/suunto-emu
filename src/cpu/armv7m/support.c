#include "armv7m_internal.h"

#include "../../core/bus_internal.h"

static void note_bus_failure(semu_cpu *cpu, uint32_t address,
                             semu_status status)
{
    if (status == SEMU_ERR_UNSUPPORTED && armv7m_scs_address(address)) {
        cpu->state.halted = 1;
        cpu->stop_reason = SEMU_STOP_UNSUPPORTED_INSTRUCTION;
        return;
    }
    cpu->state.halted = 1;
    cpu->stop_reason = SEMU_STOP_UNMAPPED_ACCESS;
    cpu->fault_address = address;
    cpu->has_fault_address = 1u;
}

semu_status armv7m_read(semu_cpu *cpu, uint32_t address, unsigned width,
                        uint32_t *value, semu_error *error)
{
    semu_status status = semu_bus_read(cpu->bus, address, width, value, error);
    if (status != SEMU_OK) {
        armv7m_request_bus_fault(cpu, address, status);
    }
    return status;
}

semu_status armv7m_write(semu_cpu *cpu, uint32_t address, unsigned width,
                         uint32_t value, semu_error *error)
{
    semu_status status = semu_bus_write(cpu->bus, address, width, value, error);
    if (status != SEMU_OK) {
        armv7m_request_bus_fault(cpu, address, status);
    } else {
        armv7m_note_local_store(cpu, address, width);
    }
    return status;
}

semu_status armv7m_validate_write(semu_cpu *cpu, uint32_t address,
                                  unsigned width, semu_error *error)
{
    semu_status status = semu_bus_validate_write(cpu->bus, address, width,
                                                 error);
    if (status != SEMU_OK) {
        note_bus_failure(cpu, address, status);
    }
    return status;
}

void armv7m_clear_exclusive(semu_cpu *cpu)
{
    if (cpu == NULL) return;
    cpu->exclusive_valid = 0u;
    cpu->exclusive_address = 0u;
    cpu->exclusive_width = 0u;
}

void armv7m_set_exclusive(semu_cpu *cpu, uint32_t address, unsigned width)
{
    if (cpu == NULL) return;
    cpu->exclusive_valid = 1u;
    cpu->exclusive_address = address;
    cpu->exclusive_width = width;
}

int armv7m_exclusive_matches(const semu_cpu *cpu, uint32_t address,
                             unsigned width)
{
    return cpu != NULL && cpu->exclusive_valid != 0u &&
           cpu->exclusive_address == address &&
           cpu->exclusive_width == width;
}

void armv7m_note_local_store(semu_cpu *cpu, uint32_t address,
                             unsigned width)
{
    uint32_t reservation_address;
    uint64_t store_end;
    uint64_t reservation_end;

    if (cpu == NULL || cpu->exclusive_valid == 0u) return;
    reservation_address = cpu->exclusive_address & ~3u;
    store_end = (uint64_t)address + width;
    reservation_end = (uint64_t)reservation_address + 4u;
    if ((uint64_t)address < reservation_end &&
        (uint64_t)reservation_address < store_end) {
        armv7m_clear_exclusive(cpu);
    }
}

semu_status armv7m_address_fault(semu_cpu *cpu, uint32_t address,
                                 semu_error *error)
{
    semu_error_set(error, SEMU_ERR_RANGE, "memory address overflow at 0x%08x",
                   address);
    cpu->state.halted = 1;
    cpu->stop_reason = SEMU_STOP_UNMAPPED_ACCESS;
    cpu->fault_address = address;
    cpu->has_fault_address = 1u;
    return SEMU_ERR_RANGE;
}

semu_status armv7m_add_address(semu_cpu *cpu, uint32_t base,
                               uint32_t offset, uint32_t *address,
                               semu_error *error)
{
    uint64_t result = (uint64_t)base + offset;
    if (result > UINT32_MAX) return armv7m_address_fault(cpu, base, error);
    *address = (uint32_t)result;
    return SEMU_OK;
}

semu_status armv7m_literal_base(semu_cpu *cpu, uint32_t pc, uint32_t *base,
                                semu_error *error)
{
    semu_status status = armv7m_add_address(cpu, pc, 4u, base, error);
    if (status == SEMU_OK) *base &= ~3u;
    return status;
}

semu_status armv7m_unsupported(semu_cpu *cpu, uint32_t instruction,
                              semu_error *error)
{
    cpu->fault_instruction = instruction;
    cpu->state.halted = 1;
    cpu->stop_reason = SEMU_STOP_UNSUPPORTED_INSTRUCTION;
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "unsupported Thumb instruction 0x%08lx at 0x%08lx",
                   (unsigned long)instruction,
                   (unsigned long)cpu->state.r[15]);
    return SEMU_ERR_UNSUPPORTED;
}

