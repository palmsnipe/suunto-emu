#ifndef SEMU_CPU_H
#define SEMU_CPU_H

#include "semu/bus.h"
#include "semu/scheduler.h"

typedef struct semu_cpu semu_cpu;

typedef struct semu_cpu_state {
    uint32_t r[16];
    uint32_t xpsr;
    uint32_t msp;
    uint32_t psp;
    uint32_t primask;
    uint32_t basepri;
    uint32_t faultmask;
    uint32_t control;
    uint32_t fpscr;
    uint32_t s[32];
    uint64_t instructions;
    int waiting_for_interrupt;
    int halted;
} semu_cpu_state;

semu_cpu *semu_cpu_create(semu_bus *bus, semu_scheduler *scheduler,
                          semu_error *error);
void semu_cpu_destroy(semu_cpu *cpu);
void semu_cpu_reset(semu_cpu *cpu, uint32_t vector_table, semu_error *error);
semu_status semu_cpu_step(semu_cpu *cpu, semu_error *error);
const semu_cpu_state *semu_cpu_get_state(const semu_cpu *cpu);
semu_cpu_state *semu_cpu_get_state_mutable(semu_cpu *cpu);
void semu_cpu_set_irq(semu_cpu *cpu, unsigned irq, int level);
void semu_cpu_set_irq_priority(semu_cpu *cpu, unsigned irq, uint8_t priority);
void semu_cpu_signal_event(semu_cpu *cpu);
semu_stop_reason semu_cpu_stop_reason(const semu_cpu *cpu);
uint32_t semu_cpu_fault_instruction(const semu_cpu *cpu);
int semu_cpu_fault_address(const semu_cpu *cpu, uint32_t *address);

#endif
