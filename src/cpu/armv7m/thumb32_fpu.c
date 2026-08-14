#include "armv7m_internal.h"

semu_status armv7m_exec32_fpu(semu_cpu *cpu, uint16_t first,
                              uint16_t second, uint32_t pc,
                              semu_error *error)
{
    (void)pc;
    if (first == 0xeee1u && (second & 0x0fffu) == 0x0a10u) {
        cpu->state.fpscr = cpu->state.r[(second >> 12u) & 15u];
        return SEMU_OK;
    }
    if (first == 0xeef1u && (second & 0x0fffu) == 0x0a10u) {
        unsigned rd = (second >> 12u) & 15u;
        if (rd == 15u) {
            cpu->state.xpsr = (cpu->state.xpsr & 0x0fffffffu) |
                              (cpu->state.fpscr & 0xf0000000u);
        } else {
            cpu->state.r[rd] = cpu->state.fpscr;
        }
        return SEMU_OK;
    }
    return armv7m_unsupported(cpu, ((uint32_t)first << 16u) | second, error);
}
