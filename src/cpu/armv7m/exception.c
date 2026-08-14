#include "armv7m_internal.h"

static semu_status exception_return(semu_cpu *cpu, uint32_t token,
                                    semu_error *error)
{
    uint32_t frame[8];
    uint32_t sp;
    unsigned index;

    if (token != 0xfffffff9u && token != 0xfffffffdu) {
        return armv7m_unsupported(cpu, token, error);
    }
    sp = token == 0xfffffffdu ? cpu->state.psp : cpu->state.msp;
    for (index = 0u; index < 8u; ++index) {
        if (armv7m_read(cpu, sp + index * 4u, 4u, &frame[index], error) !=
            SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        }
    }
    if ((frame[7] & ARMV7M_XPSR_T) == 0u) {
        return armv7m_unsupported(cpu, token, error);
    }
    cpu->state.r[0] = frame[0];
    cpu->state.r[1] = frame[1];
    cpu->state.r[2] = frame[2];
    cpu->state.r[3] = frame[3];
    cpu->state.r[12] = frame[4];
    cpu->state.r[14] = frame[5];
    cpu->state.r[15] = frame[6] & ~1u;
    cpu->state.xpsr = frame[7];
    cpu->itstate = (uint8_t)(((frame[7] >> 25) & 3u) |
                             ((frame[7] >> 8) & 0xfcu));
    if (token == 0xfffffffdu) {
        cpu->state.psp = sp + 32u;
        cpu->state.r[13] = cpu->state.psp;
    } else {
        cpu->state.msp = sp + 32u;
        cpu->state.r[13] = cpu->state.msp;
    }
    return SEMU_OK;
}

semu_status armv7m_branch_exchange(semu_cpu *cpu, uint32_t target,
                                   semu_error *error)
{
    if ((target & 0xfffffff0u) == 0xfffffff0u) {
        return exception_return(cpu, target, error);
    }
    if ((target & 1u) == 0u) {
        return armv7m_unsupported(cpu, target, error);
    }
    cpu->state.r[15] = target & ~1u;
    return SEMU_OK;
}

semu_status armv7m_take_exception(semu_cpu *cpu, unsigned exception,
                                  semu_error *error)
{
    uint32_t values[8];
    uint32_t handler;
    uint32_t sp;
    unsigned index;
    int used_psp;

    if (exception >= 16u + ARMV7M_IRQ_COUNT) {
        return armv7m_unsupported(cpu, exception, error);
    }
    used_psp = (cpu->state.xpsr & 0x1ffu) == 0u &&
               (cpu->state.control & 2u) != 0u;
    sp = (used_psp ? cpu->state.psp : cpu->state.msp) - 32u;
    values[0] = cpu->state.r[0];
    values[1] = cpu->state.r[1];
    values[2] = cpu->state.r[2];
    values[3] = cpu->state.r[3];
    values[4] = cpu->state.r[12];
    values[5] = cpu->state.r[14];
    values[6] = cpu->state.r[15];
    values[7] = cpu->state.xpsr;
    for (index = 0u; index < 8u; ++index) {
        if (armv7m_write(cpu, sp + index * 4u, 4u, values[index], error) !=
            SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        }
    }
    if (armv7m_read(cpu, cpu->vector_table + exception * 4u, 4u, &handler,
                    error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_RANGE;
    }
    if ((handler & 1u) == 0u) {
        cpu->state.halted = 1;
        cpu->stop_reason = SEMU_STOP_FIRMWARE_ASSERT;
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "exception %u has invalid vector 0x%08lx", exception,
                       (unsigned long)handler);
        return SEMU_ERR_FORMAT;
    }
    if (used_psp) {
        cpu->state.psp = sp;
    } else {
        cpu->state.msp = sp;
    }
    cpu->state.r[13] = cpu->state.msp;
    cpu->state.r[14] = used_psp ? 0xfffffffdu : 0xfffffff9u;
    cpu->state.r[15] = handler & ~1u;
    cpu->state.xpsr = (cpu->state.xpsr & ~0x1ffu) | exception;
    cpu->state.xpsr |= ARMV7M_XPSR_T;
    cpu->itstate = 0u;
    return SEMU_OK;
}
