#include "armv7m_internal.h"

static semu_status store_multiple_decrement(semu_cpu *cpu, uint16_t first,
                                            uint16_t second,
                                            semu_error *error)
{
    unsigned rn = first & 15u;
    uint32_t list = second;
    uint32_t address;
    unsigned reg;

    if (list == 0u || (list & (1u << 15)) != 0u ||
        (list & (1u << rn)) != 0u) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16) | second,
                                  error);
    }
    address = cpu->state.r[rn] - armv7m_bit_count(list) * 4u;
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
        armv7m_set_sp(cpu, cpu->state.r[rn] - armv7m_bit_count(list) * 4u);
    } else {
        cpu->state.r[rn] -= armv7m_bit_count(list) * 4u;
    }
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
    base = rn == 15u ? ((pc + 4u) & ~3u) : cpu->state.r[rn];
    address = base + (second & 0xfffu);
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
    uint32_t offset = (second & 0x0200u) != 0u
                          ? base + immediate : base - immediate;
    uint32_t address = (second & 0x0400u) != 0u ? offset : base;
    int load = (first & 0x0010u) != 0u;
    unsigned width = 1u << ((first >> 5u) & 3u);
    semu_status status;
    uint32_t value;

    if (rn == 15u || rt == 15u || (second & 0x0800u) == 0u) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    }
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
