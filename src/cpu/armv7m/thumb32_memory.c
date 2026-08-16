#include "armv7m_internal.h"

static semu_status multiple_transfer(semu_cpu *cpu, uint16_t first,
                                     uint16_t second, semu_error *error)
{
    unsigned rn = first & 15u;
    uint32_t list = second;
    uint32_t prefix = first & 0xfff0u;
    uint32_t address;
    uint32_t base = cpu->state.r[rn];
    uint32_t total = armv7m_bit_count(list) * 4u;
    uint32_t start;
    uint32_t updated;
    uint32_t values[16];
    int load;
    int decrement;
    int writeback;
    unsigned reg;

    switch (prefix) {
    case 0xe880u: case 0xe8a0u: load = 0; decrement = 0; break;
    case 0xe890u: case 0xe8b0u: load = 1; decrement = 0; break;
    case 0xe900u: case 0xe920u: load = 0; decrement = 1; break;
    case 0xe910u: case 0xe930u: load = 1; decrement = 1; break;
    default:
        return armv7m_unsupported(cpu, ((uint32_t)first << 16) | second,
                                  error);
    }
    writeback = (first & 0x20u) != 0u;
    if (rn == 15u || list == 0u || (!load && (list & (1u << 15)) != 0u) ||
        (writeback && (list & (1u << rn)) != 0u) || (base & 3u) != 0u) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16) | second,
                                  error);
    }
    if (decrement) {
        if (base < total) return armv7m_address_fault(cpu, base, error);
        start = base - total;
        updated = start;
    } else {
        start = base;
        updated = base;
        if (total > 4u && armv7m_add_address(cpu, base, total - 4u,
                                              &address, error) != SEMU_OK)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        if (writeback && armv7m_add_address(cpu, base, total, &updated,
                                             error) != SEMU_OK)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
    }
    address = start;
    if (!load) {
        for (reg = 0u; reg < 16u; ++reg) {
            if ((list & (1u << reg)) != 0u) {
                if (armv7m_validate_write(cpu, address, 4u, error) != SEMU_OK)
                    return error != NULL ? error->code : SEMU_ERR_RANGE;
                address += 4u;
            }
        }
    } else {
        for (reg = 0u; reg < 16u; ++reg) {
            if ((list & (1u << reg)) != 0u) {
                if (armv7m_read(cpu, address, 4u, &values[reg], error) !=
                    SEMU_OK)
                    return error != NULL ? error->code : SEMU_ERR_RANGE;
                address += 4u;
            }
        }
        if ((list & (1u << 15)) != 0u &&
            ((values[15] & 1u) == 0u ||
             ((values[15] & 0xfffffff0u) == 0xfffffff0u &&
              values[15] != 0xfffffff9u && values[15] != 0xfffffffdu)))
            return armv7m_unsupported(cpu, values[15], error);
    }
    if (load) {
        for (reg = 0u; reg < 15u; ++reg) {
            if ((list & (1u << reg)) != 0u) {
                if (reg == 13u) armv7m_set_sp(cpu, values[reg]);
                else cpu->state.r[reg] = values[reg];
            }
        }
    } else {
        address = start;
        for (reg = 0u; reg < 16u; ++reg) {
            if ((list & (1u << reg)) != 0u) {
                if (armv7m_write(cpu, address, 4u, cpu->state.r[reg], error) !=
                    SEMU_OK)
                    return error != NULL ? error->code : SEMU_ERR_RANGE;
                address += 4u;
            }
        }
    }
    if (writeback) {
        if (rn == 13u) armv7m_set_sp(cpu, updated);
        else cpu->state.r[rn] = updated;
    }
    if (load && (list & (1u << 15)) != 0u)
        return armv7m_branch_exchange(cpu, values[15], error);
    return SEMU_OK;
}

static int is_multiple_prefix(uint16_t first)
{
    uint32_t prefix = first & 0xfff0u;

    return prefix == 0xe880u || prefix == 0xe890u ||
           prefix == 0xe900u || prefix == 0xe910u ||
           prefix == 0xe8a0u || prefix == 0xe8b0u ||
           prefix == 0xe920u || prefix == 0xe930u;
}

static semu_status subtract_address(semu_cpu *cpu, uint32_t base,
                                    uint32_t offset, uint32_t *address,
                                    semu_error *error)
{
    if (base < offset) return armv7m_address_fault(cpu, base, error);
    *address = base - offset;
    return SEMU_OK;
}

static uint32_t sign_extend_load(uint32_t value, unsigned width)
{
    if (width == 1u && (value & 0x80u) != 0u) return value | 0xffffff00u;
    if (width == 2u && (value & 0x8000u) != 0u) return value | 0xffff0000u;
    return value;
}

static semu_status load_transfer(semu_cpu *cpu, uint32_t address,
                                 unsigned width, int signed_load,
                                 unsigned rt, semu_error *error)
{
    uint32_t value;
    semu_status status;

    status = armv7m_read(cpu, address, width, &value, error);
    if (status != SEMU_OK) return status;
    if (signed_load) value = sign_extend_load(value, width);
    if (rt == 15u) {
        if (signed_load || width != 4u || (address & 3u) != 0u)
            return armv7m_unsupported(cpu, value, error);
        return armv7m_branch_exchange(cpu, value, error);
    }
    cpu->state.r[rt] = value;
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
    int signed_load = (first & 0x0100u) != 0u;
    uint32_t base;
    uint32_t address;

    if (kind == 8u || kind == 9u) {
        width = 1u;
    } else if (kind == 10u || kind == 11u) {
        width = 2u;
    } else {
        if (kind != 12u && kind != 13u)
            return armv7m_unsupported(cpu, ((uint32_t)first << 16) | second,
                                      error);
        width = 4u;
    }
    load = (kind & 1u) != 0u;
    if (signed_load && (!load || (width != 1u && width != 2u))) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16) | second,
                                  error);
    }
    if ((!load && rt == 15u) || (load && rt == 15u && width != 4u) ||
        (rn == 15u && !load)) {
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
    return load_transfer(cpu, address, width, signed_load, rt, error);
}

static semu_status indexed_transfer(semu_cpu *cpu, uint16_t first,
                                    uint16_t second, uint32_t pc,
                                    semu_error *error)
{
    unsigned rn = first & 15u;
    unsigned rt = (second >> 12u) & 15u;
    unsigned kind = (first >> 4u) & 15u;
    unsigned rm = second & 15u;
    unsigned shift = (second >> 4u) & 3u;
    unsigned width = 1u << ((first >> 5u) & 3u);
    int signed_load = (first & 0x0100u) != 0u;
    int load = (first & 0x0010u) != 0u;
    uint32_t base;
    uint32_t immediate;
    uint32_t offset;
    uint32_t address;
    uint64_t scaled;
    semu_status status;

    if (width > 4u || (kind & 6u) == 6u ||
        (signed_load && (!load || width == 4u))) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    }
    if (rn == 15u) {
        if (!load || (second & 0x0fc0u) != 0u)
            return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                      error);
        if (armv7m_literal_base(cpu, pc, &base, error) != SEMU_OK)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        immediate = second & 0xffu;
        status = subtract_address(cpu, base, immediate, &address, error);
        if (status != SEMU_OK)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        return load_transfer(cpu, address, width, signed_load, rt, error);
    }
    if ((second & 0x0800u) == 0u) {
        if ((second & 0x0fc0u) != 0u || rm == 13u || rm == 15u ||
            rt == 15u || (second & 0x0300u) != 0u)
            return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                      error);
        base = cpu->state.r[rn];
        scaled = (uint64_t)cpu->state.r[rm] << shift;
        if (scaled > UINT32_MAX)
            return armv7m_address_fault(cpu, base, error);
        offset = (uint32_t)scaled;
        status = armv7m_add_address(cpu, base, offset, &address, error);
        if (status != SEMU_OK)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        if (load)
            return load_transfer(cpu, address, width, signed_load, rt, error);
        return armv7m_write(cpu, address, width, cpu->state.r[rt], error);
    }
    if (rt == 15u && (!load || signed_load || width != 4u)) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    }
    if ((second & 0x0400u) == 0u && (second & 0x0100u) == 0u) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    }
    if ((second & 0x0100u) != 0u && rn == rt) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    }
    base = cpu->state.r[rn];
    immediate = second & 0xffu;
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
        status = load_transfer(cpu, address, width, signed_load, rt, error);
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

static semu_status register_dual_transfer(semu_cpu *cpu, uint16_t first,
                                            uint16_t second,
                                            semu_error *error)
{
    unsigned rn = first & 15u;
    unsigned rt = (second >> 12u) & 15u;
    unsigned rt2 = (second >> 8u) & 15u;
    uint32_t imm8 = (uint32_t)(second & 0xffu) << 2u;
    int pre = (first & 0x0100u) != 0u;
    int add = (first & 0x0080u) != 0u;
    int writeback = (first & 0x0020u) != 0u;
    int load = (first & 0x0010u) != 0u;
    uint32_t base = cpu->state.r[rn];
    uint32_t offset;
    uint32_t address;
    uint32_t address2;
    uint32_t value0;
    uint32_t value1;
    semu_status status;

    if (!pre && !writeback)
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    if (rn == 15u || rt == 15u || rt2 == 15u || (base & 3u) != 0u)
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    if (writeback && (rn == rt || rn == rt2))
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    if (add)
        status = armv7m_add_address(cpu, base, imm8, &offset, error);
    else
        status = subtract_address(cpu, base, imm8, &offset, error);
    if (status != SEMU_OK)
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    address = pre ? offset : base;
    status = armv7m_add_address(cpu, address, 4u, &address2, error);
    if (status != SEMU_OK)
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    if (!load) {
        if (armv7m_validate_write(cpu, address, 4u, error) != SEMU_OK)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        if (armv7m_validate_write(cpu, address2, 4u, error) != SEMU_OK)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        if (armv7m_write(cpu, address, 4u, cpu->state.r[rt], error) !=
            SEMU_OK)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        if (armv7m_write(cpu, address2, 4u, cpu->state.r[rt2], error) !=
            SEMU_OK)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
    } else {
        if (armv7m_read(cpu, address, 4u, &value0, error) != SEMU_OK)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        if (armv7m_read(cpu, address2, 4u, &value1, error) != SEMU_OK)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        cpu->state.r[rt] = value0;
        cpu->state.r[rt2] = value1;
    }
    if (writeback) {
        if (rn == 13u) armv7m_set_sp(cpu, offset);
        else cpu->state.r[rn] = offset;
    }
    return SEMU_OK;
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
    if (is_multiple_prefix(first))
        return multiple_transfer(cpu, first, second, error);
    if ((first & 0xfe40u) == 0xe840u)
        return register_dual_transfer(cpu, first, second, error);
    if ((first & 0xff80u) == 0xf880u ||
        (first & 0xff80u) == 0xf980u) {
        return wide_transfer(cpu, first, second, pc, error);
    }
    if ((first & 0xff80u) == 0xf800u ||
        (first & 0xff80u) == 0xf900u) {
        return indexed_transfer(cpu, first, second, pc, error);
    }
    return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second, error);
}
