#include "armv7m_internal.h"

semu_status armv7m_exec32_fpu(semu_cpu *cpu, uint16_t first,
                              uint16_t second, uint32_t pc,
                              semu_error *error)
{
    uint32_t next_pc = cpu->state.r[15];
    uint32_t prior_ipsr = cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK;
    semu_status status;

    /* Fault entry must stack the address of the faulting instruction. */
    cpu->state.r[15] = pc;
    status = armv7m_fpu_convert(cpu, first, second, error);
    if (status == SEMU_ERR_UNSUPPORTED && cpu->state.halted == 0u)
        status = armv7m_fpu_arith(cpu, first, second, error);
    if (status == SEMU_ERR_UNSUPPORTED && cpu->state.halted == 0u)
        status = armv7m_fpu_transfer(cpu, first, second, pc, error);

    if (status != SEMU_OK) cpu->state.r[15] = pc;
    else if ((cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK) == prior_ipsr)
        cpu->state.r[15] = next_pc;
    return status;
}
