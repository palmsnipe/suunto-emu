#include "armv7m_internal.h"

#define FPU_NOCP (1u << 19)

static uint32_t packed_instruction(uint16_t first, uint16_t second)
{
    return ((uint32_t)first << 16u) | second;
}

static semu_status refuse(semu_cpu *cpu, uint16_t first, uint16_t second,
                          semu_error *error)
{
    return armv7m_unsupported(cpu, packed_instruction(first, second), error);
}

static int privileged(const semu_cpu *cpu)
{
    return (cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK) != 0u ||
           (cpu->state.control & 1u) == 0u;
}

semu_status armv7m_fpu_check_access(semu_cpu *cpu, semu_error *error)
{
    unsigned cp10 = (cpu->cpacr >> 20u) & 3u;
    unsigned cp11 = (cpu->cpacr >> 22u) & 3u;
    semu_status status;

    if (!((cp10 == 3u && cp11 == 3u) ||
          (cp10 == 1u && cp11 == 1u && privileged(cpu))))
        return armv7m_request_fault(cpu, 6u, FPU_NOCP, 0u, 0, error);
    status = armv7m_fpu_context_prepare(cpu, error);
    return status;
}

static semu_status subtract_address(semu_cpu *cpu, uint32_t base,
                                    uint32_t offset, uint32_t *address,
                                    semu_error *error)
{
    if (base < offset) return armv7m_address_fault(cpu, base, error);
    *address = base - offset;
    return SEMU_OK;
}

static semu_status single_address(semu_cpu *cpu, uint32_t base,
                                  uint32_t offset, int add,
                                  uint16_t first, uint16_t second,
                                  uint32_t *address, semu_error *error)
{
    semu_status status;

    status = add ? armv7m_add_address(cpu, base, offset, address, error) :
                   subtract_address(cpu, base, offset, address, error);
    if (status != SEMU_OK) return status;
    if ((*address & 3u) != 0u)
        return refuse(cpu, first, second, error);
    return SEMU_OK;
}

static unsigned single_register(uint16_t first, uint16_t second)
{
    return (((unsigned)second >> 12u) & 15u) * 2u +
           ((unsigned)first >> 6u & 1u);
}

static unsigned transfer_register(uint16_t first, uint16_t second)
{
    return ((unsigned)first & 15u) * 2u +
           (((unsigned)second >> 7u) & 1u);
}

static unsigned pair_register(uint16_t second)
{
    unsigned start = ((unsigned)second & 15u) * 2u;

    if ((second & 0x0100u) == 0u)
        start += ((unsigned)second >> 5u) & 1u;
    return start;
}

static int single_transfer_encoding(uint16_t first, uint16_t second)
{
    return (first & 0xffe0u) == 0xee00u &&
           (second & 0x0f70u) == 0x0a10u;
}

static int pair_transfer_encoding(uint16_t first, uint16_t second)
{
    return (first & 0xffc0u) == 0xec40u &&
           (second & 0x0e90u) == 0x0a10u;
}

static semu_status scalar_transfer(semu_cpu *cpu, uint16_t first,
                                   uint16_t second, semu_error *error)
{
    unsigned rt = ((unsigned)second >> 12u) & 15u;
    unsigned sn = transfer_register(first, second);
    unsigned op = ((unsigned)first >> 4u) & 1u;
    semu_status status;

    if (rt == 13u || rt == 15u || sn > 31u)
        return refuse(cpu, first, second, error);
    status = armv7m_fpu_check_access(cpu, error);
    if (status != SEMU_OK) return status;
    if (op == 0u) cpu->state.s[sn] = cpu->state.r[rt];
    else cpu->state.r[rt] = cpu->state.s[sn];
    return SEMU_OK;
}

static semu_status pair_transfer(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, semu_error *error)
{
    unsigned rt = ((unsigned)second >> 12u) & 15u;
    unsigned rt2 = (unsigned)first & 15u;
    unsigned sm = pair_register(second);
    unsigned op = ((unsigned)first >> 4u) & 1u;
    uint32_t first_value;
    uint32_t second_value;
    semu_status status;

    if (rt == 13u || rt == 15u || rt2 == 13u || rt2 == 15u ||
        sm > 30u || (op != 0u && rt == rt2))
        return refuse(cpu, first, second, error);
    status = armv7m_fpu_check_access(cpu, error);
    if (status != SEMU_OK) return status;
    if (op == 0u) {
        first_value = cpu->state.r[rt];
        second_value = cpu->state.r[rt2];
        cpu->state.s[sm] = first_value;
        cpu->state.s[sm + 1u] = second_value;
    } else {
        first_value = cpu->state.s[sm];
        second_value = cpu->state.s[sm + 1u];
        cpu->state.r[rt] = first_value;
        cpu->state.r[rt2] = second_value;
    }
    return SEMU_OK;
}

static semu_status system_transfer(semu_cpu *cpu, uint16_t first,
                                    uint16_t second, semu_error *error)
{
    unsigned rt = ((unsigned)second >> 12u) & 15u;
    semu_status status;

    if ((second & 0x0fffu) != 0x0a10u)
        return refuse(cpu, first, second, error);
    if (first == 0xeee1u && (rt == 13u || rt == 15u))
        return refuse(cpu, first, second, error);
    if (first != 0xeee1u && first != 0xeef1u)
        return refuse(cpu, first, second, error);
    if (first == 0xeef1u && rt == 13u)
        return refuse(cpu, first, second, error);
    status = armv7m_fpu_check_access(cpu, error);
    if (status != SEMU_OK) return status;
    if (first == 0xeee1u) {
        cpu->state.fpscr = cpu->state.r[rt];
    } else if (rt == 15u) {
        cpu->state.xpsr = (cpu->state.xpsr & 0x0fffffffu) |
                          (cpu->state.fpscr & 0xf0000000u);
    } else {
        cpu->state.r[rt] = cpu->state.fpscr;
    }
    return SEMU_OK;
}

static semu_status transfer_start(semu_cpu *cpu, uint32_t base,
                                   unsigned count, int add, int writeback,
                                   uint32_t *address, uint32_t *updated,
                                   semu_error *error)
{
    uint64_t bytes = (uint64_t)count * 4u;
    uint64_t end;

    if ((base & 3u) != 0u) return SEMU_ERR_UNSUPPORTED;
    if (add) {
        end = (uint64_t)base + bytes;
        if (end > UINT64_C(0x100000000))
            return armv7m_address_fault(cpu, base, error);
        if (writeback && end > UINT32_MAX)
            return armv7m_address_fault(cpu, base, error);
        *address = base;
        *updated = (uint32_t)end;
    } else {
        if ((uint64_t)base < bytes)
            return armv7m_address_fault(cpu, base, error);
        *address = base - (uint32_t)bytes;
        *updated = *address;
        end = (uint64_t)*address + bytes;
        if (end > UINT64_C(0x100000000))
            return armv7m_address_fault(cpu, *address, error);
    }
    if (!writeback) *updated = base;
    return SEMU_OK;
}

static semu_status transfer_address(semu_cpu *cpu, uint32_t start,
                                     unsigned index, uint32_t *address,
                                     semu_error *error)
{
    uint64_t result = (uint64_t)start + (uint64_t)index * 4u;

    if (result > UINT32_MAX) return armv7m_address_fault(cpu, start, error);
    *address = (uint32_t)result;
    return SEMU_OK;
}

static void set_core_register(semu_cpu *cpu, unsigned reg, uint32_t value)
{
    if (reg == 13u) armv7m_set_sp(cpu, value);
    else cpu->state.r[reg] = value;
}

static semu_status multiple_transfer(semu_cpu *cpu, uint16_t first,
                                     uint16_t second, semu_error *error)
{
    unsigned rn = (unsigned)first & 15u;
    unsigned count = (unsigned)second & 0xffu;
    unsigned start = single_register(first, (uint16_t)(second & 0xf000u));
    int push = (first & 0x0100u) != 0u;
    int add = (first & 0x0080u) != 0u;
    int writeback = (first & 0x0020u) != 0u;
    int load = (first & 0x0010u) != 0u;
    uint32_t values[16];
    uint32_t addresses[16];
    uint32_t address;
    uint32_t updated;
    semu_status status;
    unsigned index;

    if ((second & 0x0f00u) != 0x0a00u || rn == 15u || count == 0u ||
        count > 16u || start + count > 32u ||
        (push && (add || !writeback)))
        return refuse(cpu, first, second, error);
    if (!push && !add && writeback)
        return refuse(cpu, first, second, error);
    if (push && add) return refuse(cpu, first, second, error);
    if (!push && !add && !writeback)
        return refuse(cpu, first, second, error);
    if (!push && add && !writeback) {
        /* IA without write-back is the only no-write-back form. */
    } else if (!push && add && writeback) {
        /* IA with write-back is also legal. */
    } else if (push && !add && writeback) {
        /* DB with write-back is the stack form. */
    } else {
        return refuse(cpu, first, second, error);
    }
    status = armv7m_fpu_check_access(cpu, error);
    if (status != SEMU_OK) return status;
    status = transfer_start(cpu, cpu->state.r[rn], count, add, writeback,
                            &address, &updated, error);
    if (status == SEMU_ERR_UNSUPPORTED)
        return refuse(cpu, first, second, error);
    if (status != SEMU_OK) return status;
    for (index = 0u; index < count; ++index) {
        status = transfer_address(cpu, address, index, &addresses[index],
                                  error);
        if (status != SEMU_OK) return status;
    }
    if (load) {
        for (index = 0u; index < count; ++index) {
            status = armv7m_read(cpu, addresses[index], 4u,
                                 &values[index], error);
            if (status != SEMU_OK) return status;
        }
    } else {
        for (index = 0u; index < count; ++index) {
            values[index] = cpu->state.s[start + index];
            status = armv7m_validate_write(cpu, addresses[index], 4u,
                                           error);
            if (status != SEMU_OK) return status;
        }
    }
    if (load) {
        for (index = 0u; index < count; ++index)
            cpu->state.s[start + index] = values[index];
    } else {
        for (index = 0u; index < count; ++index) {
            status = armv7m_write(cpu, addresses[index], 4u, values[index],
                                  error);
            if (status != SEMU_OK) return status;
        }
    }
    if (writeback) set_core_register(cpu, rn, updated);
    return SEMU_OK;
}

static semu_status single_memory_transfer(semu_cpu *cpu, uint16_t first,
                                           uint16_t second, uint32_t pc,
                                           semu_error *error)
{
    unsigned rn = (unsigned)first & 15u;
    unsigned sd = single_register(first, second);
    unsigned offset = ((unsigned)second & 0xffu) * 4u;
    int add = (first & 0x0080u) != 0u;
    int load = (first & 0x0010u) != 0u;
    uint32_t base;
    uint32_t address;
    uint32_t value;
    semu_status status;

    if ((second & 0x0f00u) != 0x0a00u || sd > 31u || (!load && rn == 15u))
        return refuse(cpu, first, second, error);
    if (rn == 15u && !load) return refuse(cpu, first, second, error);
    status = armv7m_fpu_check_access(cpu, error);
    if (status != SEMU_OK) return status;
    if (rn == 15u) {
        status = armv7m_literal_base(cpu, pc, &base, error);
        if (status != SEMU_OK) return status;
    } else {
        base = cpu->state.r[rn];
    }
    status = single_address(cpu, base, offset, add, first, second,
                            &address, error);
    if (status != SEMU_OK) return status;
    if (load) {
        status = armv7m_read(cpu, address, 4u, &value, error);
        if (status == SEMU_OK) cpu->state.s[sd] = value;
        return status;
    }
    status = armv7m_validate_write(cpu, address, 4u, error);
    if (status != SEMU_OK) return status;
    return armv7m_write(cpu, address, 4u, cpu->state.s[sd], error);
}

semu_status armv7m_fpu_transfer(semu_cpu *cpu, uint16_t first,
                                uint16_t second, uint32_t pc,
                                semu_error *error)
{
    if (first == 0xeee1u || first == 0xeef1u)
        return system_transfer(cpu, first, second, error);
    if (single_transfer_encoding(first, second))
        return scalar_transfer(cpu, first, second, error);
    if (pair_transfer_encoding(first, second))
        return pair_transfer(cpu, first, second, error);
    if ((first & 0xff00u) == 0xec00u) {
        return multiple_transfer(cpu, first, second, error);
    }
    if ((first & 0xff00u) == 0xed00u) {
        if ((first & 0x0020u) == 0u)
            return single_memory_transfer(cpu, first, second, pc, error);
        return multiple_transfer(cpu, first, second, error);
    }
    return refuse(cpu, first, second, error);
}
