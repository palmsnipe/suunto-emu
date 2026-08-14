#include "armv7m_internal.h"

semu_status armv7m_exec16(semu_cpu *cpu, uint16_t instruction,
                          uint32_t pc, semu_error *error)
{
    cpu->state.r[15] = pc + 2u;
    if ((instruction & 0xe000u) == 0u ||
        (instruction & 0xe000u) == 0x2000u ||
        (instruction & 0xfc00u) == 0x4000u) {
        return armv7m_exec16_arith(cpu, instruction, pc, error);
    }
    if ((instruction & 0xfc00u) == 0x4400u ||
        (instruction & 0xf800u) == 0xa000u ||
        (instruction & 0xf500u) == 0xb100u ||
        (instruction & 0xf000u) == 0xd000u ||
        (instruction & 0xf800u) == 0xe000u) {
        return armv7m_exec16_control(cpu, instruction, pc, error);
    }
    if ((instruction & 0xfe00u) == 0xb400u ||
        (instruction & 0xfe00u) == 0xbc00u ||
        (instruction & 0xf000u) == 0xc000u ||
        (instruction & 0xe000u) == 0x6000u ||
        (instruction & 0xf000u) == 0x8000u ||
        (instruction & 0xf800u) == 0x4800u ||
        (instruction & 0xf000u) == 0x9000u ||
        (instruction & 0xf000u) == 0x5000u) {
        return armv7m_exec16_memory(cpu, instruction, pc, error);
    }
    if ((instruction & 0xf000u) == 0xb000u) {
        return armv7m_exec16_control(cpu, instruction, pc, error);
    }
    return armv7m_exec16_memory(cpu, instruction, pc, error);
}
