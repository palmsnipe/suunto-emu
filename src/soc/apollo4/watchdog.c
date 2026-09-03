#include "watchdog.h"

#include <stdlib.h>

enum {
    WATCHDOG_CFG = 0u,
    WATCHDOG_RESTART = 4u,
    WATCHDOG_INTEN = 0x200u
};

#define WATCHDOG_CFG_MASK UINT32_C(0x07ffff0f)
#define WATCHDOG_CLOCK_SHIFT 24u
#define WATCHDOG_CLOCK_MASK UINT32_C(0x7)
#define WATCHDOG_CLOCK_MAX UINT32_C(4)
#define WATCHDOG_INTEN_MASK UINT32_C(0x3)

struct semu_apollo4_watchdog {
    uint32_t cfg;
    uint32_t interrupt_enable;
};

static int cfg_valid(uint32_t value)
{
    return (value & ~WATCHDOG_CFG_MASK) == 0u &&
           ((value >> WATCHDOG_CLOCK_SHIFT) & WATCHDOG_CLOCK_MASK) <=
               WATCHDOG_CLOCK_MAX;
}

static semu_status validate_access(void *context, uint32_t offset,
                                   unsigned width, semu_error *error)
{
    if (context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 watchdog context required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u ||
        (offset != WATCHDOG_CFG && offset != WATCHDOG_RESTART &&
         offset != WATCHDOG_INTEN)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 watchdog register access is unsupported");
        return SEMU_ERR_UNSUPPORTED;
    }
    return SEMU_OK;
}

semu_status semu_apollo4_watchdog_read(void *context, uint32_t offset,
                                       unsigned width, uint32_t *value,
                                       semu_error *error)
{
    semu_apollo4_watchdog *watchdog = (semu_apollo4_watchdog *)context;
    semu_status status = validate_access(context, offset, width, error);
    if (status != SEMU_OK)
        return status;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 watchdog read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (offset == WATCHDOG_CFG)
        *value = watchdog->cfg;
    else if (offset == WATCHDOG_RESTART)
        *value = 0u;
    else
        *value = watchdog->interrupt_enable;
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_apollo4_watchdog_write(void *context, uint32_t offset,
                                        unsigned width, uint32_t value,
                                        semu_error *error)
{
    semu_apollo4_watchdog *watchdog = (semu_apollo4_watchdog *)context;
    semu_status status = validate_access(context, offset, width, error);
    if (status != SEMU_OK)
        return status;
    if (offset == WATCHDOG_CFG) {
        if (!cfg_valid(value)) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "Apollo4 watchdog CFG value is unsupported");
            return SEMU_ERR_UNSUPPORTED;
        }
        watchdog->cfg = value;
    } else if (offset == WATCHDOG_RESTART) {
        if (value != SEMU_APOLLO4_WATCHDOG_RESTART_KEY) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "Apollo4 watchdog restart key is unsupported");
            return SEMU_ERR_UNSUPPORTED;
        }
    } else {
        if ((value & ~WATCHDOG_INTEN_MASK) != 0u) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "Apollo4 watchdog INTEN value is unsupported");
            return SEMU_ERR_UNSUPPORTED;
        }
        watchdog->interrupt_enable = value;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

void semu_apollo4_watchdog_reset(void *context)
{
    semu_apollo4_watchdog *watchdog = (semu_apollo4_watchdog *)context;
    if (watchdog != NULL) {
        watchdog->cfg = SEMU_APOLLO4_WATCHDOG_CFG_RESET;
        watchdog->interrupt_enable = 0u;
    }
}

static const semu_bus_device_ops watchdog_ops = {
    semu_apollo4_watchdog_read,
    semu_apollo4_watchdog_write,
    semu_apollo4_watchdog_reset
};

semu_apollo4_watchdog *semu_apollo4_watchdog_create(
    semu_bus *bus, semu_error *error)
{
    semu_apollo4_watchdog *watchdog;
    semu_status status;
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 watchdog requires a bus");
        return NULL;
    }
    watchdog = (semu_apollo4_watchdog *)calloc(1u, sizeof(*watchdog));
    if (watchdog == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate Apollo4 watchdog");
        return NULL;
    }
    semu_apollo4_watchdog_reset(watchdog);
    status = semu_bus_map_device(bus, "apollo4.watchdog",
                                 SEMU_APOLLO4_WATCHDOG_BASE,
                                 SEMU_APOLLO4_WATCHDOG_SIZE, &watchdog_ops,
                                 watchdog, error);
    if (status != SEMU_OK) {
        free(watchdog);
        return NULL;
    }
    semu_error_clear(error);
    return watchdog;
}

void semu_apollo4_watchdog_destroy(semu_apollo4_watchdog *watchdog)
{
    free(watchdog);
}

semu_status semu_apollo4_watchdog_snapshot_write(
    const semu_apollo4_watchdog *watchdog, semu_snapshot_writer *writer,
    semu_error *error)
{
    semu_status status;
    if (watchdog == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 watchdog snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    status = semu_snapshot_writer_u32(writer, watchdog->cfg, error);
    if (status != SEMU_OK)
        return status;
    return semu_snapshot_writer_u32(writer, watchdog->interrupt_enable, error);
}

semu_status semu_apollo4_watchdog_snapshot_read(
    semu_apollo4_watchdog *watchdog, semu_snapshot_reader *reader,
    semu_error *error)
{
    uint32_t cfg;
    uint32_t interrupt_enable;
    semu_status status;
    if (watchdog == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 watchdog snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    status = semu_snapshot_reader_u32(reader, &cfg, error);
    if (status != SEMU_OK)
        return status;
    status = semu_snapshot_reader_u32(reader, &interrupt_enable, error);
    if (status != SEMU_OK)
        return status;
    if (!cfg_valid(cfg)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "Apollo4 watchdog snapshot CFG is unreachable");
        return SEMU_ERR_FORMAT;
    }
    if ((interrupt_enable & ~WATCHDOG_INTEN_MASK) != 0u) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "Apollo4 watchdog snapshot INTEN is unreachable");
        return SEMU_ERR_FORMAT;
    }
    watchdog->cfg = cfg;
    watchdog->interrupt_enable = interrupt_enable;
    semu_error_clear(error);
    return SEMU_OK;
}
