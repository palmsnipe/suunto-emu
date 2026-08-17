#include "adc.h"

enum {
    ADC_BASE = 0x400b0000u,
    ADC_SIZE = 0x3000u,
    ADC_CONTROL = 0x2024u
};

/* E-A4-ADC-001: the Renode Sapporo trace leaves the ADC peripheral
 * unimplemented. The firmware nevertheless performs this bounded setup
 * sequence; Renode logs and ignores it, and returns zero for reads. */
static int read_allowed(uint32_t offset, unsigned width)
{
    if (offset == 0x0u || offset == 0x8u || offset == 0xcu ||
        offset == 0x10u || offset == 0x14u || offset == 0x18u) {
        return width == 4u;
    }
    if (offset == 0x4u) {
        return width == 1u || width == 4u;
    }
    if (offset == 0x1u || offset == 0xau ||
        offset == 0xbu) {
        return width == 1u;
    }
    if (offset == 0x2u) {
        return width == 2u;
    }
    return 0;
}

static int write_allowed(uint32_t offset, unsigned width, uint32_t value)
{
    if (offset == ADC_CONTROL) {
        return width == 4u && value == UINT32_C(0x80000000);
    }
    if (offset == 0xbu) {
        return width == 1u && value == 0x4u;
    }
    if (offset == 0x1u) {
        return width == 1u && (value == 0x40u || value == 0u);
    }
    if (offset == 0xcu) {
        return width == 4u && value <= 0x50000u &&
               (value & 0xffffu) == 0u;
    }
    return 0;
}

static semu_status adc_read(void *context, uint32_t offset, unsigned width,
                             uint32_t *value, semu_error *error)
{
    if (context == NULL || value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 ADC read requires context and value");
        return SEMU_ERR_ARGUMENT;
    }
    if (!read_allowed(offset, width)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 ADC read offset/width unsupported");
        return SEMU_ERR_UNSUPPORTED;
    }
    *value = 0u;
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status adc_write(void *context, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error)
{
    if (context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 ADC write requires context");
        return SEMU_ERR_ARGUMENT;
    }
    if (!write_allowed(offset, width, value)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 ADC write offset/width/value unsupported");
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

static void adc_reset(void *context)
{
    (void)context;
}

static const semu_bus_device_ops adc_ops = {
    adc_read, adc_write, adc_reset
};

semu_status semu_apollo4_adc_map(semu_bus *bus, void *context,
                                 semu_error *error)
{
    if (bus == NULL || context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 ADC requires bus and context");
        return SEMU_ERR_ARGUMENT;
    }
    return semu_bus_map_device(bus, "apollo4.adc", ADC_BASE, ADC_SIZE,
                               &adc_ops, context, error);
}
