#include "clock.h"

#include <stdlib.h>

/*
* E-A4-CLK-001 verified trace (sapporo-apollo4-coldboot-clock-reset-unpatched):
*   offset 0x44: read 0x0, write 0xF80000, read 0xF80000, write 0xF80040
*   offset 0x0c: read 0x0, write 0x0
* Offset 0x84 is never accessed by firmware. The Renode clkgen model is a
* permissive Python register bank; only the observed offsets and values are
* modeled here. All other offsets and widths fail closed.
*/

enum {
    CLOCK_CAL = 0x44u,
    CLOCK_INTR = 0x0cu
};

static int is_observed_value(uint32_t offset, uint32_t value)
{
    return (offset == CLOCK_CAL &&
            (value == UINT32_C(0xf80000) ||
             value == UINT32_C(0xf80040))) ||
           (offset == CLOCK_INTR && value == 0u);
}

struct semu_apollo4_clock {
    semu_bus *bus;
    semu_scheduler *scheduler;
    uint32_t cal;
    uint32_t intr;
};

static const semu_bus_device_ops clock_ops = {
    semu_apollo4_clock_read,
    semu_apollo4_clock_write,
    semu_apollo4_clock_reset
};

static int is_known_offset(uint32_t offset)
{
    return offset == CLOCK_CAL || offset == CLOCK_INTR;
}

static semu_status validate_access(void *context, uint32_t offset,
                                   unsigned width, semu_error *error)
{
    if (context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Apollo4 clock context required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 clock supports 32-bit accesses only");
        return SEMU_ERR_UNSUPPORTED;
    }
    if (!is_known_offset(offset)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 clock offset 0x%08x is unsupported", offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

static void reset_state(semu_apollo4_clock *clock)
{
    clock->cal = 0u;
    clock->intr = 0u;
}

semu_apollo4_clock *semu_apollo4_clock_create(semu_bus *bus,
                                              semu_scheduler *scheduler,
                                              semu_error *error)
{
    semu_apollo4_clock *clock;
    semu_status status;

    if (bus == NULL || scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 clock requires bus and scheduler");
        return NULL;
    }
    clock = (semu_apollo4_clock *)calloc(1u, sizeof(*clock));
    if (clock == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate Apollo4 clock generator");
        return NULL;
    }
    clock->bus = bus;
    clock->scheduler = scheduler;
    reset_state(clock);
    status = semu_bus_map_device(bus, "apollo4.clock", SEMU_APOLLO4_CLOCK_BASE,
                                 SEMU_APOLLO4_CLOCK_SIZE, &clock_ops, clock,
                                 error);
    if (status != SEMU_OK) {
        free(clock);
        return NULL;
    }
    semu_error_clear(error);
    return clock;
}

void semu_apollo4_clock_destroy(semu_apollo4_clock *clock)
{
    free(clock);
}

void semu_apollo4_clock_reset(void *context)
{
    semu_apollo4_clock *clock = (semu_apollo4_clock *)context;
    if (clock != NULL) {
        reset_state(clock);
    }
}

semu_status semu_apollo4_clock_read(void *context, uint32_t offset,
                                    unsigned width, uint32_t *value,
                                    semu_error *error)
{
    semu_apollo4_clock *clock = (semu_apollo4_clock *)context;
    semu_status status = validate_access(context, offset, width, error);

    if (status != SEMU_OK) {
        return status;
    }
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "clock read value required");
        return SEMU_ERR_ARGUMENT;
    }
    switch (offset) {
    case CLOCK_CAL:
        *value = clock->cal;
        break;
    case CLOCK_INTR:
        *value = clock->intr;
        break;
    default:
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_apollo4_clock_write(void *context, uint32_t offset,
                                     unsigned width, uint32_t value,
                                     semu_error *error)
{
    semu_apollo4_clock *clock = (semu_apollo4_clock *)context;
    semu_status status = validate_access(context, offset, width, error);

    if (status != SEMU_OK) {
        return status;
    }
    if (!is_observed_value(offset, value)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 clock value 0x%08x is unsupported at 0x%08x",
                       value, offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    switch (offset) {
    case CLOCK_CAL:
        clock->cal = value;
        break;
    case CLOCK_INTR:
        clock->intr = value;
        break;
    default:
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

const semu_bus_device_ops *semu_apollo4_clock_bus_ops(void)
{
    return &clock_ops;
}

semu_status semu_apollo4_clock_snapshot_write(
    const semu_apollo4_clock *clock, semu_snapshot_writer *writer,
    semu_error *error)
{
    if (clock == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "clock snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (semu_snapshot_writer_u32(writer, clock->cal, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, clock->intr, error) != SEMU_OK)
        return error->code;
    return SEMU_OK;
}

semu_status semu_apollo4_clock_snapshot_read(
    semu_apollo4_clock *clock, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_apollo4_clock candidate;
    if (clock == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "clock snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *clock;
    if (semu_snapshot_reader_u32(reader, &candidate.cal, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.intr, error) != SEMU_OK)
        return error->code;
    *clock = candidate;
    return SEMU_OK;
}
