#include "armv7m_internal.h"

static semu_status store_multiple_decrement(semu_cpu *cpu, uint16_t first,
                                            uint16_t second,
                                            semu_error *error)
{
    unsigned rn = first & 15u;
    uint32_t list = second;
    uint32_t address;
    uint32_t base = cpu->state.r[rn];
    uint32_t total = armv7m_bit_count(list) * 4u;
    uint64_t start;
    unsigned reg;

    if (rn == 15u || list == 0u || (list & (1u << 15)) != 0u ||
        (list & (1u << rn)) != 0u) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16) | second,
                                  error);
    }
    start = (uint64_t)base - total;
    if (base < total || start > UINT32_MAX) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "multiple-store address underflow at 0x%08x", base);
        cpu->state.halted = 1;
        cpu->stop_reason = SEMU_STOP_UNMAPPED_ACCESS;
        cpu->fault_address = base;
        cpu->has_fault_address = 1u;
        return SEMU_ERR_RANGE;
    }
    address = (uint32_t)start;
    for (reg = 0u; reg < 15u; ++reg) {
        if ((list & (1u << reg)) != 0u) {
            if (armv7m_validate_write(cpu, address, 4u, error) != SEMU_OK) {
                return error != NULL ? error->code : SEMU_ERR_RANGE;
            }
            address += 4u;
        }
    }
    address = (uint32_t)start;
    for (reg = 0u; reg < 15u; ++reg) {
        if ((list & (1u << reg)) != 0u) {
            if (armv7m_write(cpu, address, 4u, cpu->state.r[reg], error) !=
                SEMU_OK) {
                return error != NULL ? error->code : SEMU_ERR_RANGE;
            }
            address += 4u;
        }
    }
    if (rn == 13u) {
        armv7m_set_sp(cpu, base - total);
    } else {
        cpu->state.r[rn] = base - total;
    }
    return SEMU_OK;
}

static semu_status subtract_address(semu_cpu *cpu, uint32_t base,
                                    uint32_t offset, uint32_t *address,
                                    semu_error *error)
{
    if (base < offset) return armv7m_address_fault(cpu, base, error);
    *address = base - offset;
    return SEMU_OK;
}

static semu_status wide_transfer(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, uint32_t pc,
                                 semu_error *error)
{
    unsigned kind = (first >> 4) & 15u;
    unsigned rn = first & 15u;
    unsigned rt = (second >> 12) & 15u;
    unsigned width;
    int load;
    uint32_t base;
    uint32_t address;
    uint32_t value;
    semu_status status;

    if (kind == 8u || kind == 9u) {
        width = 1u;
    } else if (kind == 10u || kind == 11u) {
        width = 2u;
    } else {
        width = 4u;
    }
    load = (kind & 1u) != 0u;
    if ((!load && rt == 15u) || (load && rt == 15u && width != 4u)) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16) | second,
                                  error);
    }
    if (rn == 15u) {
        if (armv7m_literal_base(cpu, pc, &base, error) != SEMU_OK)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
    } else {
        base = cpu->state.r[rn];
    }
    if (armv7m_add_address(cpu, base, second & 0xfffu, &address, error) !=
        SEMU_OK)
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    if (!load) {
        return armv7m_write(cpu, address, width, cpu->state.r[rt], error);
    }
    status = armv7m_read(cpu, address, width, &value, error);
    if (status != SEMU_OK) {
        return status;
    }
    if (rt == 15u) {
        return armv7m_branch_exchange(cpu, value, error);
    }
    cpu->state.r[rt] = value;
    return SEMU_OK;
}

static semu_status indexed_transfer(semu_cpu *cpu, uint16_t first,
                                    uint16_t second, semu_error *error)
{
    unsigned rn = first & 15u;
    unsigned rt = (second >> 12u) & 15u;
    uint32_t base = cpu->state.r[rn];
    uint32_t immediate = second & 0xffu;
    uint32_t offset;
    uint32_t address;
    int load = (first & 0x0010u) != 0u;
    unsigned width = 1u << ((first >> 5u) & 3u);
    semu_status status;
    uint32_t value;

    if (rn == 15u || rt == 15u || (second & 0x0800u) == 0u) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    }
    if ((second & 0x0200u) != 0u) {
        status = armv7m_add_address(cpu, base, immediate, &offset, error);
    } else {
        status = subtract_address(cpu, base, immediate, &offset, error);
    }
    if (status != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    }
    address = (second & 0x0400u) != 0u ? offset : base;
    if (load) {
        status = armv7m_read(cpu, address, width, &value, error);
        if (status == SEMU_OK) {
            cpu->state.r[rt] = value;
        }
    } else {
        status = armv7m_write(cpu, address, width, cpu->state.r[rt], error);
    }
    if (status == SEMU_OK && (second & 0x0100u) != 0u) {
        cpu->state.r[rn] = offset;
        if (rn == 13u) {
            armv7m_set_sp(cpu, offset);
        }
    }
    return status;
}

semu_status armv7m_exec32_memory(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, uint32_t pc,
                                 semu_error *error)
{
    if ((first & 0xfff0u) == 0xe840u ||
        (first & 0xfff0u) == 0xe850u ||
        (first & 0xfff0u) == 0xe8c0u ||
        (first & 0xfff0u) == 0xe8d0u)
        return armv7m_exec32_memory_exclusive(cpu, first, second, pc, error);
    if ((first & 0xfff0u) == 0xe920u) {
        return store_multiple_decrement(cpu, first, second, error);
    }
    if ((first & 0xff80u) == 0xf880u) {
        return wide_transfer(cpu, first, second, pc, error);
    }
    if ((first & 0xff80u) == 0xf800u) {
        return indexed_transfer(cpu, first, second, error);
    }
    return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second, error);
}
