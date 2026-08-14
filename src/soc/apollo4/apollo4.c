#include "apollo4_internal.h"

#include <stdlib.h>
#include <string.h>

static const semu_bus_device_ops regbank_ops = {
    semu_regbank_read, semu_regbank_write, semu_regbank_reset
};

static void init_bank(semu_regbank *bank, const char *name,
                      const uint32_t *offsets, size_t count)
{
    bank->name = name;
    bank->count = count;
    memcpy(bank->allowed, offsets, count * sizeof(offsets[0]));
}

static semu_status map_bank(semu_apollo4 *soc, semu_regbank *bank,
                            uint32_t base, uint32_t size, semu_error *error)
{
    return semu_bus_map_device(soc->bus, bank->name, base, size,
                               &regbank_ops, bank, error);
}

semu_apollo4 *semu_apollo4_create(semu_bus *bus, semu_error *error)
{
    static const uint32_t clock_offsets[] = { 0x84u };
    static const uint32_t power_offsets[] = {
        0x04u, 0x24u, 0x100u, 0x108u, 0x250u, 0x254u
    };
    static const uint32_t mram_offsets[] = { 0x00u, 0x04u, 0x08u };
    static const uint32_t crypto_offsets[] = { 0xfe0u };
    static const uint32_t system_offsets[] = { 0xd08u, 0xd88u };
    semu_apollo4 *soc;
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Apollo4 requires a bus");
        return NULL;
    }
    soc = (semu_apollo4 *)calloc(1u, sizeof(*soc));
    if (soc == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate Apollo4");
        return NULL;
    }
    soc->bus = bus;
    memset(soc->gpio_level, 1, sizeof(soc->gpio_level));
    init_bank(&soc->clock, "apollo4.clock", clock_offsets,
              SEMU_ARRAY_LEN(clock_offsets));
    init_bank(&soc->power, "apollo4.power", power_offsets,
              SEMU_ARRAY_LEN(power_offsets));
    init_bank(&soc->mram, "apollo4.mram", mram_offsets,
              SEMU_ARRAY_LEN(mram_offsets));
    init_bank(&soc->crypto, "apollo4.crypto", crypto_offsets,
              SEMU_ARRAY_LEN(crypto_offsets));
    init_bank(&soc->system, "armv7m.system", system_offsets,
              SEMU_ARRAY_LEN(system_offsets));
    if (map_bank(soc, &soc->clock, 0x40004000u, 0x800u, error) != SEMU_OK ||
        map_bank(soc, &soc->mram, 0x40014000u, 0x1000u, error) != SEMU_OK ||
        map_bank(soc, &soc->power, 0x40021000u, 0x400u, error) != SEMU_OK ||
        map_bank(soc, &soc->crypto, 0x400c0000u, 0x2000u, error) != SEMU_OK ||
        map_bank(soc, &soc->system, 0xe000e000u, 0x1000u, error) != SEMU_OK) {
        free(soc);
        return NULL;
    }
    return soc;
}

void semu_apollo4_destroy(semu_apollo4 *soc)
{
    free(soc);
}

void semu_apollo4_reset(semu_apollo4 *soc)
{
    if (soc != NULL) {
        memset(soc->gpio_level, 1, sizeof(soc->gpio_level));
        semu_regbank_reset(&soc->clock);
        semu_regbank_reset(&soc->power);
        semu_regbank_reset(&soc->mram);
        semu_regbank_reset(&soc->crypto);
        semu_regbank_reset(&soc->system);
    }
}

semu_status semu_apollo4_set_gpio_input(semu_apollo4 *soc, unsigned pin,
                                        int level, semu_error *error)
{
    if (soc == NULL || pin >= SEMU_APOLLO4_GPIO_COUNT) {
        semu_error_set(error, SEMU_ERR_RANGE, "invalid GPIO pin %u", pin);
        return SEMU_ERR_RANGE;
    }
    soc->gpio_level[pin] = level != 0;
    return SEMU_OK;
}

int semu_apollo4_get_gpio_input(const semu_apollo4 *soc, unsigned pin)
{
    return soc != NULL && pin < SEMU_APOLLO4_GPIO_COUNT
               ? soc->gpio_level[pin] != 0
               : -1;
}
