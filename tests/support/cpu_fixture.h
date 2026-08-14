#ifndef SEMU_CPU_FIXTURE_H
#define SEMU_CPU_FIXTURE_H

#include "semu/cpu.h"

typedef struct semu_cpu_fixture {
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_cpu *cpu;
    semu_error error;
    uint32_t program_address;
    size_t program_size;
} semu_cpu_fixture;

typedef struct semu_cpu_memory_byte {
    uint32_t address;
    uint8_t value;
} semu_cpu_memory_byte;

int semu_cpu_fixture_init(semu_cpu_fixture *fixture, const uint8_t *program,
                          size_t program_size);
int semu_cpu_fixture_init_at(semu_cpu_fixture *fixture, const uint8_t *program,
                             size_t program_size, uint32_t program_address);
void semu_cpu_fixture_destroy(semu_cpu_fixture *fixture);
int semu_cpu_fixture_load_u32(semu_cpu_fixture *fixture, uint32_t address,
                              uint32_t value);
void semu_cpu_fixture_apply_state(semu_cpu_fixture *fixture,
                                  const semu_cpu_state *initial);
semu_status semu_cpu_fixture_step(semu_cpu_fixture *fixture);
semu_status semu_cpu_fixture_run(semu_cpu_fixture *fixture,
                                 unsigned max_steps);
int semu_cpu_fixture_expect(const semu_cpu_fixture *fixture,
                            const semu_cpu_state *expected,
                            const semu_cpu_memory_byte *memory,
                            size_t memory_count);

#endif
