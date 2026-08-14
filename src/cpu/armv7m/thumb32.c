#include "armv7m_internal.h"

semu_status armv7m_exec32(semu_cpu *cpu, uint16_t first, uint16_t second,
                          uint32_t pc, semu_error *error)
{
    cpu->state.r[15] = pc + 4u;
    if ((first & 0xfff0u) == 0xe840u ||
        (first & 0xfff0u) == 0xe850u ||
        (first & 0xfff0u) == 0xe8c0u ||
        (first & 0xfff0u) == 0xe8d0u ||
        (first & 0xfff0u) == 0xe880u ||
        (first & 0xfff0u) == 0xe890u ||
        (first & 0xfff0u) == 0xe8a0u ||
        (first & 0xfff0u) == 0xe8b0u ||
        (first & 0xfff0u) == 0xe900u ||
        (first & 0xfff0u) == 0xe910u ||
        (first & 0xfff0u) == 0xe920u ||
        (first & 0xfff0u) == 0xe930u ||
        (first & 0xff80u) == 0xf880u ||
        (first & 0xff80u) == 0xf800u ||
        (first & 0xff80u) == 0xf980u ||
        (first & 0xff80u) == 0xf900u) {
        semu_status status = armv7m_exec32_memory(cpu, first, second, pc,
                                                  error);
        if (status != SEMU_OK) cpu->state.r[15] = pc;
        return status;
    }
    if (first == 0xeee1u || first == 0xeef1u) {
        return armv7m_exec32_fpu(cpu, first, second, pc, error);
    }
    if (first == 0xf3bfu || first == 0xf3afu || first == 0xf3efu ||
        ((first & 0xfff0u) == 0xf380u &&
         (second & 0xf000u) == 0x8000u)) {
        return armv7m_exec32_system(cpu, first, second, pc, error);
    }
    if ((first & 0xff00u) == 0xfb00u ||
        (first & 0xfff0u) == 0xeac0u ||
        (first & 0xffd0u) == 0xf300u ||
        (first & 0xffd0u) == 0xf380u ||
        (first & 0xff80u) == 0xfa00u ||
        ((first & 0xff80u) == 0xfa80u &&
         (first & 0xfff0u) != 0xfab0u)) {
        return armv7m_exec32_dsp(cpu, first, second, pc, error);
    }
    return armv7m_exec32_data(cpu, first, second, pc, error);
}
