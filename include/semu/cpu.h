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
    /* v8-M stack-limit registers (E-WIS-0001: the Wismar entry writes
     * both at reset; the lane oracle accepts the writes). Stored so the
     * guest's writes are observable; no limit-enforcement fault is
     * modeled because no lane trace shows one. */
    uint32_t msplim;
    uint32_t psplim;
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
/* Binds the NVIC implemented-priority width for this CPU instance:
 * 0xe0 (three bits, reset default) or 0xf0 (four bits, Wismar-class
 * evidence E-WIS-0001). Refuses any other value; survives until the
 * next CPU reset, which restores 0xe0. Returns 1 on success. */
int semu_cpu_bind_priority_mask(semu_cpu *cpu, uint8_t mask, semu_error *error);
uint8_t semu_cpu_priority_mask(const semu_cpu *cpu);
void semu_cpu_signal_event(semu_cpu *cpu);
int semu_cpu_reset_requested(const semu_cpu *cpu);
semu_stop_reason semu_cpu_stop_reason(const semu_cpu *cpu);
uint32_t semu_cpu_fault_instruction(const semu_cpu *cpu);
int semu_cpu_fault_address(const semu_cpu *cpu, uint32_t *address);

#endif
