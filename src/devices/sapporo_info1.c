#include "sapporo_info1.h"

enum {
    INFO1_BASE = 0x42002000u,
    INFO1_SIZE = 0x1340u
};

static semu_status info1_read(void *context, uint32_t offset,
                              unsigned width, uint32_t *value,
                              semu_error *error)
{
    if (context == NULL || value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo INFO1 read requires context and value");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || offset >= INFO1_SIZE) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Sapporo INFO1 offset/width unsupported");
        return SEMU_ERR_UNSUPPORTED;
    }
    *value = 0u;
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status info1_write(void *context, uint32_t offset,
                               unsigned width, uint32_t value,
                               semu_error *error)
{
    (void)offset;
    (void)width;
    (void)value;
    if (context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo INFO1 write requires context");
        return SEMU_ERR_ARGUMENT;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Sapporo INFO1 is read-only");
    return SEMU_ERR_UNSUPPORTED;
}

static void info1_reset(void *context)
{
    (void)context;
}

static const semu_bus_device_ops info1_ops = {
    info1_read, info1_write, info1_reset
};

semu_status semu_sapporo_info1_map(semu_bus *bus, semu_error *error)
{
    static const int context = 1;

    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo INFO1 requires a bus");
        return SEMU_ERR_ARGUMENT;
    }
    return semu_bus_map_device(bus, "sapporo.info1", INFO1_BASE, INFO1_SIZE,
                               &info1_ops, (void *)&context, error);
}
