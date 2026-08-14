#include "cpu_guest.h"

#include <string.h>

static void put_u32(uint8_t bytes[4], uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

int semu_cpu_guest_init(semu_cpu_guest *guest, const uint8_t *image,
                        size_t image_size, uint32_t ram_size)
{
    if (guest == NULL || image == NULL || image_size == 0u ||
        ram_size == 0u || image_size > ram_size) {
        return 0;
    }
    (void)memset(guest, 0, sizeof(*guest));
    guest->bus = semu_bus_create(&guest->error);
    if (guest->bus == NULL ||
        semu_bus_map_ram(guest->bus, "synthetic-rtos-ram", 0u, ram_size,
                         &guest->error) != SEMU_OK ||
        semu_bus_load(guest->bus, 0u, image, image_size, &guest->error) !=
            SEMU_OK) {
        semu_cpu_guest_destroy(guest);
        return 0;
    }
    guest->scheduler = semu_scheduler_create(&guest->error);
    if (guest->scheduler == NULL) {
        semu_cpu_guest_destroy(guest);
        return 0;
    }
    guest->cpu = semu_cpu_create(guest->bus, guest->scheduler,
                                 &guest->error);
    if (guest->cpu == NULL) {
        semu_cpu_guest_destroy(guest);
        return 0;
    }
    semu_cpu_reset(guest->cpu, 0u, &guest->error);
    if (guest->error.code != SEMU_OK) {
        semu_cpu_guest_destroy(guest);
        return 0;
    }
    return 1;
}

void semu_cpu_guest_destroy(semu_cpu_guest *guest)
{
    if (guest == NULL) return;
    semu_cpu_destroy(guest->cpu);
    semu_scheduler_destroy(guest->scheduler);
    semu_bus_destroy(guest->bus);
    guest->cpu = NULL;
    guest->scheduler = NULL;
    guest->bus = NULL;
}

int semu_cpu_guest_write_u32(semu_cpu_guest *guest, uint32_t address,
                             uint32_t value)
{
    uint8_t bytes[4];

    if (guest == NULL || guest->bus == NULL) return 0;
    put_u32(bytes, value);
    return semu_bus_load(guest->bus, address, bytes, sizeof(bytes),
                         &guest->error) == SEMU_OK;
}

semu_status semu_cpu_guest_run(semu_cpu_guest *guest,
                               uint64_t instruction_limit,
                               uint64_t virtual_time_limit)
{
    semu_status status = SEMU_OK;

    if (guest == NULL || guest->cpu == NULL || guest->scheduler == NULL ||
        instruction_limit == 0u || virtual_time_limit == 0u) {
        return SEMU_ERR_ARGUMENT;
    }
    semu_error_clear(&guest->error);
    while (!semu_cpu_get_state(guest->cpu)->halted) {
        const semu_cpu_state *state = semu_cpu_get_state(guest->cpu);

        if (state->instructions >= instruction_limit ||
            semu_scheduler_now(guest->scheduler) >= virtual_time_limit) {
            return SEMU_ERR_STATE;
        }
        status = semu_cpu_step(guest->cpu, &guest->error);
        if (status != SEMU_OK) return status;
    }
    return SEMU_OK;
}
