#include "armv7m_internal.h"

semu_status armv7m_exec32_system(semu_cpu *cpu, uint16_t first,
                                 uint16_t second, uint32_t pc,
                                 semu_error *error)
{
    (void)pc;
    if (first == 0xf3bfu &&
        (second == 0x8f2fu || second == 0x8f4fu || second == 0x8f5fu ||
         second == 0x8f6fu)) {
        return SEMU_OK;
    }
    if (first == 0xf3afu && second == 0x8000u) {
        return SEMU_OK;
    }
    if (first == 0xf3afu && second == 0x8003u) {
        cpu->state.waiting_for_interrupt = 1;
        return SEMU_OK;
    }
    if ((first & 0xfff0u) == 0xf380u && (second & 0xff00u) == 0x8800u) {
        unsigned source = first & 15u;
        unsigned special = second & 0xffu;
        uint32_t value = cpu->state.r[source];
        switch (special) {
        case 0x08u:
            cpu->state.msp = value;
            armv7m_set_sp(cpu, value);
            return SEMU_OK;
        case 0x09u: cpu->state.psp = value; return SEMU_OK;
        case 0x10u: cpu->state.primask = value & 1u; return SEMU_OK;
        case 0x11u: cpu->state.basepri = value & 0xffu; return SEMU_OK;
        case 0x12u:
            if (cpu->state.basepri == 0u ||
                (value & 0xffu) < cpu->state.basepri) {
                cpu->state.basepri = value & 0xffu;
            }
            return SEMU_OK;
        case 0x13u: cpu->state.faultmask = value & 1u; return SEMU_OK;
        case 0x14u: cpu->state.control = value & 7u; return SEMU_OK;
        default: break;
        }
    }
    return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second, error);
}
