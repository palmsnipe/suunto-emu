#include "armv7m_internal.h"

static int privileged(const semu_cpu *cpu)
{
    return (cpu->state.xpsr & 0x1ffu) != 0u ||
           (cpu->state.control & 1u) == 0u;
}

static int valid_special(unsigned special)
{
    return special <= 3u || (special >= 5u && special <= 9u) ||
           (special >= 16u && special <= 20u);
}

static uint32_t apsr_value(const semu_cpu *cpu)
{
    return cpu->state.xpsr & ARMV7M_XPSR_APSR_MASK;
}

static uint32_t mrs_value(const semu_cpu *cpu, unsigned special)
{
    int is_privileged = privileged(cpu);

    if (special <= 3u) {
        uint32_t value = apsr_value(cpu);
        if ((special & 1u) != 0u && is_privileged) {
            value |= cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK;
        }
        return value;
    }
    if (special == 5u || special == 7u) {
        return is_privileged ? cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK : 0u;
    }
    if (special == 6u) {
        return 0u;
    }
    if (!is_privileged) {
        return special == 20u ? cpu->state.control & 3u : 0u;
    }
    switch (special) {
    case 8u: return cpu->state.msp & ~3u;
    case 9u: return cpu->state.psp & ~3u;
    case 16u: return cpu->state.primask & 1u;
    case 17u:
    case 18u: return cpu->state.basepri & 0xffu;
    case 19u: return cpu->state.faultmask & 1u;
    case 20u: return cpu->state.control & 3u;
    default: return 0u;
    }
}

static void write_stack(semu_cpu *cpu, unsigned special, uint32_t value)
{
    value &= ~3u;
    if (special == 8u) {
        cpu->state.msp = value;
        if ((cpu->state.xpsr & 0x1ffu) != 0u ||
            (cpu->state.control & 2u) == 0u) {
            cpu->state.r[13] = value;
        }
    } else {
        cpu->state.psp = value;
        if ((cpu->state.xpsr & 0x1ffu) == 0u &&
            (cpu->state.control & 2u) != 0u) {
            cpu->state.r[13] = value;
        }
    }
}

static void write_apsr(semu_cpu *cpu, uint32_t value, unsigned mask)
{
    if ((mask & 1u) != 0u) {
        cpu->state.xpsr = (cpu->state.xpsr & ~0x000f0000u) |
                          (value & 0x000f0000u);
    }
    if ((mask & 2u) != 0u) {
        cpu->state.xpsr = (cpu->state.xpsr & ~0xf8000000u) |
                          (value & 0xf8000000u);
    }
}

static semu_status execute_mrs(semu_cpu *cpu, uint16_t second,
                               semu_error *error)
{
    unsigned destination = (second >> 8u) & 15u;
    unsigned special = second & 0xffu;

    if ((second & 0xf000u) != 0x8000u || destination == 13u ||
        destination == 15u || !valid_special(special)) {
        return armv7m_unsupported(cpu, ((uint32_t)0xf3efu << 16u) | second,
                                   error);
    }
    cpu->state.r[destination] = mrs_value(cpu, special);
    return SEMU_OK;
}

static semu_status execute_msr(semu_cpu *cpu, uint16_t first,
                               uint16_t second, semu_error *error)
{
    unsigned source = first & 15u;
    unsigned mask = (second >> 10u) & 3u;
    unsigned special = second & 0xffu;
    uint32_t value;

    if ((second & 0xf000u) != 0x8000u || (second & 0x0300u) != 0u ||
        source == 13u || source == 15u || !valid_special(special) ||
        mask == 0u || (mask != 2u && special > 3u)) {
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                   error);
    }
    value = cpu->state.r[source];
    if (special <= 3u) {
        write_apsr(cpu, value, mask);
        return SEMU_OK;
    }
    if (!privileged(cpu)) return SEMU_OK;
    switch (special) {
    case 5u:
    case 6u:
    case 7u:
        return SEMU_OK;
    case 8u:
    case 9u:
        write_stack(cpu, special, value);
        return SEMU_OK;
    case 16u:
        cpu->state.primask = value & 1u;
        return SEMU_OK;
    case 17u:
        cpu->state.basepri = value & 0xffu;
        return SEMU_OK;
    case 18u:
        value &= 0xffu;
        if (value != 0u &&
            (cpu->state.basepri == 0u || value < cpu->state.basepri)) {
            cpu->state.basepri = value;
        }
        return SEMU_OK;
    case 19u:
        if ((cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK) == 2u ||
            (cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK) == 3u) {
            return SEMU_OK;
        }
        cpu->state.faultmask = value & 1u;
        return SEMU_OK;
    case 20u:
        cpu->state.control = (cpu->state.control & ~1u) | (value & 1u);
        if ((cpu->state.xpsr & 0x1ffu) == 0u) {
            cpu->state.control = (cpu->state.control & ~2u) |
                                 (value & 2u);
            cpu->state.r[13] = (value & 2u) != 0u ? cpu->state.psp :
                               cpu->state.msp;
        }
        return SEMU_OK;
    default:
        return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second,
                                  error);
    }
}

semu_status armv7m_exec32_system(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, uint32_t pc,
                                 semu_error *error)
{
    (void)pc;
    if (first == 0xf3bfu && second == 0x8f2fu) {
        armv7m_clear_exclusive(cpu);
        return SEMU_OK;
    }
    if (first == 0xf3bfu &&
        (second == 0x8f4fu || second == 0x8f5fu || second == 0x8f6fu)) {
        return SEMU_OK;
    }
    if (first == 0xf3afu && second == 0x8000u) {
        return SEMU_OK;
    }
    if (first == 0xf3afu && second == 0x8002u) {
        armv7m_sleep_wfe(cpu);
        return SEMU_OK;
    }
    if (first == 0xf3afu && second == 0x8003u) {
        armv7m_sleep_wfi(cpu);
        return SEMU_OK;
    }
    if (first == 0xf3efu) return execute_mrs(cpu, second, error);
    if ((first & 0xfff0u) == 0xf380u &&
        (second & 0xf000u) == 0x8000u) {
        return execute_msr(cpu, first, second, error);
    }
    return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second, error);
}
