#ifndef SEMU_CPU_GUEST_H
#define SEMU_CPU_GUEST_H

#include "semu/cpu.h"

typedef struct semu_cpu_guest {
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_cpu *cpu;
    semu_error error;
    uint32_t image_size;
    uint32_t stop_pc;
} semu_cpu_guest;

int semu_cpu_guest_init(semu_cpu_guest *guest, const uint8_t *image,
                        size_t image_size, uint32_t ram_size);
void semu_cpu_guest_destroy(semu_cpu_guest *guest);
int semu_cpu_guest_write_u32(semu_cpu_guest *guest, uint32_t address,
                             uint32_t value);
semu_status semu_cpu_guest_run(semu_cpu_guest *guest,
                               uint64_t instruction_limit,
                               uint64_t virtual_time_limit);

#endif
