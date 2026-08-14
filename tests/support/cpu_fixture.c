#include "cpu_fixture.h"

#include <string.h>

static void put_u32(uint8_t bytes[4], uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

int semu_cpu_fixture_load_u32(semu_cpu_fixture *fixture, uint32_t address,
                              uint32_t value)
{
    uint8_t bytes[4];
    put_u32(bytes, value);
    return fixture != NULL && semu_bus_load(fixture->bus, address, bytes,
                                            sizeof(bytes), &fixture->error) ==
           SEMU_OK;
}

int semu_cpu_fixture_init_at(semu_cpu_fixture *fixture, const uint8_t *program,
                             size_t program_size, uint32_t program_address)
{
    if (fixture == NULL || program == NULL || program_size == 0u ||
        program_address >= 0x1000u || program_size > 0x1000u - program_address) {
        return 0;
    }
    (void)memset(fixture, 0, sizeof(*fixture));
    fixture->program_address = program_address;
    fixture->program_size = program_size;
    fixture->bus = semu_bus_create(&fixture->error);
    if (fixture->bus == NULL ||
        semu_bus_map_ram(fixture->bus, "cpu-test", 0u, 0x1000u,
                         &fixture->error) != SEMU_OK ||
        !semu_cpu_fixture_load_u32(fixture, 0u, 0x800u) ||
        !semu_cpu_fixture_load_u32(fixture, 4u, program_address | 1u) ||
        semu_bus_load(fixture->bus, program_address, program, program_size,
                      &fixture->error) != SEMU_OK) {
        semu_cpu_fixture_destroy(fixture);
        return 0;
    }
    fixture->scheduler = semu_scheduler_create(&fixture->error);
    if (fixture->scheduler == NULL) {
        semu_cpu_fixture_destroy(fixture);
        return 0;
    }
    fixture->cpu = semu_cpu_create(fixture->bus, fixture->scheduler,
                                   &fixture->error);
    if (fixture->cpu == NULL) {
        semu_cpu_fixture_destroy(fixture);
        return 0;
    }
    semu_cpu_reset(fixture->cpu, 0u, &fixture->error);
    if (fixture->error.code != SEMU_OK) {
        semu_cpu_fixture_destroy(fixture);
        return 0;
    }
    return 1;
}

int semu_cpu_fixture_init(semu_cpu_fixture *fixture, const uint8_t *program,
                          size_t program_size)
{
    return semu_cpu_fixture_init_at(fixture, program, program_size, 0x100u);
}

void semu_cpu_fixture_destroy(semu_cpu_fixture *fixture)
{
    if (fixture == NULL) {
        return;
    }
    semu_cpu_destroy(fixture->cpu);
    semu_scheduler_destroy(fixture->scheduler);
    semu_bus_destroy(fixture->bus);
    fixture->cpu = NULL;
    fixture->scheduler = NULL;
    fixture->bus = NULL;
}

void semu_cpu_fixture_apply_state(semu_cpu_fixture *fixture,
                                  const semu_cpu_state *initial)
{
    semu_cpu_state *state;
    if (fixture == NULL || fixture->cpu == NULL || initial == NULL) {
        return;
    }
    state = semu_cpu_get_state_mutable(fixture->cpu);
    *state = *initial;
}

semu_status semu_cpu_fixture_step(semu_cpu_fixture *fixture)
{
    if (fixture == NULL || fixture->cpu == NULL) {
        return SEMU_ERR_ARGUMENT;
    }
    semu_error_clear(&fixture->error);
    return semu_cpu_step(fixture->cpu, &fixture->error);
}

semu_status semu_cpu_fixture_run(semu_cpu_fixture *fixture,
                                 unsigned max_steps)
{
    unsigned step;
    semu_status status = SEMU_OK;

    if (fixture == NULL || fixture->cpu == NULL || max_steps == 0u) {
        return SEMU_ERR_ARGUMENT;
    }
    for (step = 0u; step < max_steps; ++step) {
        status = semu_cpu_fixture_step(fixture);
        if (status != SEMU_OK || semu_cpu_get_state(fixture->cpu)->halted) {
            return status;
        }
    }
    return status;
}

int semu_cpu_fixture_expect(const semu_cpu_fixture *fixture,
                            const semu_cpu_state *expected,
                            const semu_cpu_memory_byte *memory,
                            size_t memory_count)
{
    size_t index;
    uint32_t value;
    semu_error error;

    if (fixture == NULL || fixture->cpu == NULL || expected == NULL ||
        (memory_count != 0u && memory == NULL) ||
        memcmp(semu_cpu_get_state(fixture->cpu), expected, sizeof(*expected)) !=
        0) {
        return 0;
    }
    semu_error_clear(&error);
    for (index = 0u; index < memory_count; ++index) {
        if (semu_bus_read(fixture->bus, memory[index].address, 1u, &value,
                          &error) != SEMU_OK ||
            value != memory[index].value) {
            return 0;
        }
    }
    return 1;
}
