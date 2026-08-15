#include "mcu_control.h"

#include <stdlib.h>

/*
* E-A4-RST-001 verified trace (sapporo-apollo4-coldboot-clock-reset-unpatched):
*   CHIPREV offset 0x0c reads 0x21 (six reads across two reset cycles).
*   20 other offsets are read and return 0x0 (Renode model default).
*   10 offsets receive writes; the model ignores them silently.
*   All unlisted offsets and non-32-bit widths fail closed.
*/

enum {
    MCU_CHIPREV = 0x0cu
};

/* Evidenced read offsets (including CHIPREV). */
static const uint32_t read_offsets[] = {
    0x0cu, 0x28u, 0x44u, 0x60u, 0x80u, 0x88u, 0x108u, 0x124u,
    0x33cu, 0x340u, 0x344u, 0x34cu, 0x354u, 0x358u, 0x360u,
    0x36cu, 0x370u, 0x378u, 0x37cu, 0x380u, 0x42cu
};

/* Evidenced write offsets (subset of read offsets). */
static const uint32_t write_offsets[] = {
    0x33cu, 0x340u, 0x344u, 0x354u, 0x358u, 0x360u,
    0x370u, 0x378u, 0x37cu, 0x380u
};

struct semu_apollo4_mcu_control {
    semu_bus *bus;
    semu_scheduler *scheduler;
};

static const semu_bus_device_ops mcu_ops = {
    semu_apollo4_mcu_control_read,
    semu_apollo4_mcu_control_write,
    semu_apollo4_mcu_control_reset
};

static int in_list(const uint32_t *list, size_t count, uint32_t offset)
{
    size_t i;
    for (i = 0u; i < count; ++i) {
        if (list[i] == offset) {
            return 1;
        }
    }
    return 0;
}

static int is_known_read(uint32_t offset)
{
    return in_list(read_offsets, sizeof(read_offsets) / sizeof(read_offsets[0]),
                   offset);
}

static int is_known_write(uint32_t offset)
{
    return in_list(write_offsets, sizeof(write_offsets) / sizeof(write_offsets[0]),
                   offset);
}

static semu_status validate(void *context, uint32_t offset, unsigned width,
                            int write, semu_error *error)
{
    if (context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 MCU control context required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 MCU control supports 32-bit accesses only");
        return SEMU_ERR_UNSUPPORTED;
    }
    if (write ? !is_known_write(offset) : !is_known_read(offset)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 MCU control offset 0x%08x is unsupported",
                       offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

semu_apollo4_mcu_control *semu_apollo4_mcu_control_create(
    semu_bus *bus, semu_scheduler *scheduler, semu_error *error)
{
    semu_apollo4_mcu_control *mcu;
    semu_status status;

    if (bus == NULL || scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 MCU control requires bus and scheduler");
        return NULL;
    }
    mcu = (semu_apollo4_mcu_control *)calloc(1u, sizeof(*mcu));
    if (mcu == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate Apollo4 MCU control");
        return NULL;
    }
    mcu->bus = bus;
    mcu->scheduler = scheduler;
    status = semu_bus_map_device(bus, "apollo4.mcu_control",
                                 SEMU_APOLLO4_MCU_CONTROL_BASE,
                                 SEMU_APOLLO4_MCU_CONTROL_SIZE, &mcu_ops, mcu,
                                 error);
    if (status != SEMU_OK) {
        free(mcu);
        return NULL;
    }
    semu_error_clear(error);
    return mcu;
}

void semu_apollo4_mcu_control_destroy(semu_apollo4_mcu_control *mcu)
{
    free(mcu);
}

void semu_apollo4_mcu_control_reset(void *context)
{
    (void)context;
}

semu_status semu_apollo4_mcu_control_read(void *context, uint32_t offset,
                                          unsigned width, uint32_t *value,
                                          semu_error *error)
{
    semu_status status = validate(context, offset, width, 0, error);
    if (status != SEMU_OK) {
        return status;
    }
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "MCU control read value required");
        return SEMU_ERR_ARGUMENT;
    }
    *value = (offset == MCU_CHIPREV) ? SEMU_APOLLO4_CHIPREV : 0u;
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_apollo4_mcu_control_write(void *context, uint32_t offset,
                                           unsigned width, uint32_t value,
                                           semu_error *error)
{
    semu_status status = validate(context, offset, width, 1, error);
    if (status != SEMU_OK) {
        return status;
    }
    (void)value;
    semu_error_clear(error);
    return SEMU_OK;
}

const semu_bus_device_ops *semu_apollo4_mcu_control_bus_ops(void)
{
    return &mcu_ops;
}
