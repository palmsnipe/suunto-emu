#ifndef SEMU_ARMV7M_INTERNAL_H
#define SEMU_ARMV7M_INTERNAL_H

#include "semu/cpu.h"

#define ARMV7M_IRQ_COUNT 256u
#define ARMV7M_XPSR_N (1u << 31)
#define ARMV7M_XPSR_Z (1u << 30)
#define ARMV7M_XPSR_C (1u << 29)
#define ARMV7M_XPSR_V (1u << 28)
#define ARMV7M_XPSR_T (1u << 24)

struct semu_cpu {
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_cpu_state state;
    semu_stop_reason stop_reason;
    uint32_t fault_instruction;
    uint32_t vector_table;
    uint8_t irq_level[ARMV7M_IRQ_COUNT];
    uint8_t itstate;
};

semu_status armv7m_exec16(semu_cpu *cpu, uint16_t instruction,
                          uint32_t pc, semu_error *error);
semu_status armv7m_exec32(semu_cpu *cpu, uint16_t first, uint16_t second,
                          uint32_t pc, semu_error *error);

semu_status armv7m_read(semu_cpu *cpu, uint32_t address, unsigned width,
                        uint32_t *value, semu_error *error);
semu_status armv7m_write(semu_cpu *cpu, uint32_t address, unsigned width,
                         uint32_t value, semu_error *error);
semu_status armv7m_unsupported(semu_cpu *cpu, uint32_t instruction,
                              semu_error *error);
semu_status armv7m_take_exception(semu_cpu *cpu, unsigned exception,
                                  semu_error *error);
semu_status armv7m_branch_exchange(semu_cpu *cpu, uint32_t target,
                                   semu_error *error);
semu_status armv7m_exec16_memory(semu_cpu *cpu, uint16_t instruction,
                                 uint32_t pc, semu_error *error);
void armv7m_set_itstate(semu_cpu *cpu, uint8_t value);

uint32_t armv7m_reg(const semu_cpu *cpu, unsigned reg, uint32_t pc);
void armv7m_set_sp(semu_cpu *cpu, uint32_t value);
void armv7m_set_nz(semu_cpu *cpu, uint32_t value);
uint32_t armv7m_add(semu_cpu *cpu, uint32_t left, uint32_t right,
                    unsigned carry, int update_flags);
int armv7m_condition_passed(const semu_cpu *cpu, unsigned condition);
int32_t armv7m_sign_extend(uint32_t value, unsigned bits);

#endif
