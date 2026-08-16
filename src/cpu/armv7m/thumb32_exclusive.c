#include "armv7m_internal.h"

static int data_register(unsigned reg)
{
    return reg < 13u || reg == 14u;
}

static int exclusive_load_subopcode(uint16_t second, unsigned width)
{
    unsigned expected = width == 1u ? 0x40u : 0x50u;
    return (second & 0x0ff0u) == (0x0f00u | expected) &&
           (second & 0x000fu) == 0x000fu;
}

static int exclusive_store_subopcode(uint16_t second, unsigned width)
{
    unsigned expected = width == 1u ? 0x40u : 0x50u;
    return (second & 0x0ff0u) == (0x0f00u | expected);
}

static semu_status exclusive_load(semu_cpu *cpu, uint16_t first,
                                  uint16_t second, unsigned width,
                                  uint32_t pc, semu_error *error)
{
    unsigned rn = first & 15u;
    unsigned rt = second >> 12u;
    uint32_t address;
    uint32_t value;
    uint32_t offset = width == 4u ? (second & 0xffu) << 2u : 0u;
    semu_status status;

    if (rn == 15u || !data_register(rt) ||
        (width == 4u && (second & 0x0f00u) != 0x0f00u))
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    if (width != 4u && !exclusive_load_subopcode(second, width))
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    if (armv7m_add_address(cpu, cpu->state.r[rn], offset, &address, error) !=
        SEMU_OK)
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    if (width > 1u && (address & (width - 1u)) != 0u)
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    status = armv7m_read(cpu, address, width, &value, error);
    if (status != SEMU_OK) return status;
    cpu->state.r[rt] = value;
    armv7m_set_exclusive(cpu, address, width);
    (void)pc;
    return SEMU_OK;
}

static semu_status exclusive_store(semu_cpu *cpu, uint16_t first,
                                   uint16_t second, unsigned width,
                                   semu_error *error)
{
    unsigned rn = first & 15u;
    unsigned rt = second >> 12u;
    unsigned rd = width == 4u ? (second >> 8u) & 15u : second & 15u;
    uint32_t address;
    uint32_t offset = width == 4u ? (second & 0xffu) << 2u : 0u;
    semu_status status;

    if (rn == 15u || !data_register(rt) || !data_register(rd) ||
        rd == rn || rd == rt ||
        (width != 4u && !exclusive_store_subopcode(second, width)))
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    if (armv7m_add_address(cpu, cpu->state.r[rn], offset, &address, error) !=
        SEMU_OK)
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    if (width > 1u && (address & (width - 1u)) != 0u)
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    if (!armv7m_exclusive_matches(cpu, address, width)) {
        cpu->state.r[rd] = 1u;
        armv7m_clear_exclusive(cpu);
        return SEMU_OK;
    }
    status = armv7m_write(cpu, address, width, cpu->state.r[rt], error);
    if (status != SEMU_OK) return status;
    cpu->state.r[rd] = 0u;
    armv7m_clear_exclusive(cpu);
    return SEMU_OK;
}

static semu_status table_branch(semu_cpu *cpu, uint16_t first,
                                uint16_t second, uint32_t pc,
                                semu_error *error)
{
    unsigned rn = first & 15u;
    unsigned rm = second & 15u;
    unsigned width = (second & 0x10u) != 0u ? 2u : 1u;
    uint32_t base;
    uint32_t offset;
    uint64_t scaled_index;
    uint32_t address;
    uint32_t value;
    uint64_t target;
    semu_status status;

    if ((second & 0xffe0u) != 0xf000u || rn == 13u ||
        rm == 13u || rm == 15u)
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    if (rn == 15u) {
        if (armv7m_add_address(cpu, pc, 4u, &base, error) != SEMU_OK)
            return error != NULL ? error->code : SEMU_ERR_RANGE;
    } else {
        base = cpu->state.r[rn];
    }
    scaled_index = width == 2u ? (uint64_t)cpu->state.r[rm] * 2u :
                                  cpu->state.r[rm];
    if (scaled_index > UINT32_MAX)
        return armv7m_address_fault(cpu, base, error);
    offset = (uint32_t)scaled_index;
    if (armv7m_add_address(cpu, base, offset, &address, error) != SEMU_OK)
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    status = armv7m_read(cpu, address, width, &value, error);
    if (status != SEMU_OK) return status;
    target = (uint64_t)pc + 4u + (uint64_t)value * 2u;
    if (target > UINT32_MAX) return armv7m_address_fault(cpu, pc, error);
    cpu->state.r[15] = (uint32_t)target;
    return SEMU_OK;
}

semu_status armv7m_exec32_memory_exclusive(semu_cpu *cpu, uint16_t first,
                                            uint16_t second, uint32_t pc,
                                            semu_error *error)
{
    if ((first & 0xfff0u) == 0xe840u)
        return exclusive_store(cpu, first, second, 4u, error);
    if ((first & 0xfff0u) == 0xe850u)
        return exclusive_load(cpu, first, second, 4u, pc, error);
    if ((first & 0xfff0u) == 0xe8c0u) {
        if ((second & 0x00f0u) == 0x40u)
            return exclusive_store(cpu, first, second, 1u, error);
        if ((second & 0x00f0u) == 0x50u)
            return exclusive_store(cpu, first, second, 2u, error);
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    }
    if ((first & 0xfff0u) == 0xe8d0u) {
        if ((second & 0xffe0u) == 0xf000u)
            return table_branch(cpu, first, second, pc, error);
        if ((second & 0x00f0u) == 0x40u)
            return exclusive_load(cpu, first, second, 1u, pc, error);
        if ((second & 0x00f0u) == 0x50u)
            return exclusive_load(cpu, first, second, 2u, pc, error);
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    }
    return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second, error);
}
