#include "apollo4_internal.h"

#include "../../core/scheduler_internal.h"

static semu_status write_child(const semu_apollo4 *soc,
                               semu_snapshot_writer *writer, semu_error *error)
{
    if (semu_snapshot_writer_bytes(writer, soc->gpio_level,
                                   sizeof(soc->gpio_level), error) != SEMU_OK ||
        semu_apollo4_clock_snapshot_write(soc->clock, writer, error) != SEMU_OK ||
        semu_apollo4_power_snapshot_write(soc->power, writer, error) != SEMU_OK ||
        semu_apollo4_gpio_snapshot_write(soc->gpio, writer, error) != SEMU_OK ||
        semu_apollo4_timer_snapshot_write(soc->timer, writer, error) != SEMU_OK ||
        semu_apollo4_stimer_snapshot_write(soc->stimer, writer, error) != SEMU_OK ||
        semu_apollo4_uart_snapshot_write(soc->uart, writer, error) != SEMU_OK ||
        semu_apollo4_iom_snapshot_write(soc->iom0, writer, error) != SEMU_OK ||
        semu_apollo4_iom_snapshot_write(soc->iom2, writer, error) != SEMU_OK ||
        semu_apollo4_iom_snapshot_write(soc->iom3, writer, error) != SEMU_OK ||
        semu_apollo4_iom_snapshot_write(soc->iom4, writer, error) != SEMU_OK ||
        semu_apollo4_iom_snapshot_write(soc->iom6, writer, error) != SEMU_OK ||
        semu_apollo4_mspi_snapshot_write(soc->mspi1, writer, error) != SEMU_OK ||
        semu_apollo4_mspi_snapshot_write(soc->mspi2, writer, error) != SEMU_OK ||
        semu_apollo4_mram_snapshot_write(soc->mram, writer, error) != SEMU_OK)
        return error->code;
    return SEMU_OK;
}

semu_status semu_apollo4_snapshot_write(
    const semu_apollo4 *soc, semu_snapshot_writer *writer, semu_error *error)
{
    if (soc == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    return write_child(soc, writer, error);
}

semu_status semu_apollo4_snapshot_read(
    semu_apollo4 *soc, semu_snapshot_reader *reader, semu_error *error)
{
    semu_apollo4 candidate;
    size_t index;
    if (soc == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *soc;
    if (semu_snapshot_reader_bytes(reader, candidate.gpio_level,
                                   sizeof(candidate.gpio_level), error) != SEMU_OK) {
        return error->code;
    }
    for (index = 0u; index < SEMU_APOLLO4_GPIO_COUNT; ++index) {
        if (candidate.gpio_level[index] > 1u) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                           "Apollo4 GPIO level snapshot is unreachable");
            return SEMU_ERR_FORMAT;
        }
    }
    if (semu_apollo4_clock_snapshot_read(candidate.clock, reader, error) != SEMU_OK ||
        semu_apollo4_power_snapshot_read(candidate.power, reader, error) != SEMU_OK ||
        semu_apollo4_gpio_snapshot_read(candidate.gpio, reader, error) != SEMU_OK ||
        semu_apollo4_timer_snapshot_read(candidate.timer, reader, error) != SEMU_OK ||
        semu_apollo4_stimer_snapshot_read(candidate.stimer, reader, error) != SEMU_OK ||
        semu_apollo4_uart_snapshot_read(candidate.uart, reader, error) != SEMU_OK ||
        semu_apollo4_iom_snapshot_read(candidate.iom0, reader, error) != SEMU_OK ||
        semu_apollo4_iom_snapshot_read(candidate.iom2, reader, error) != SEMU_OK ||
        semu_apollo4_iom_snapshot_read(candidate.iom3, reader, error) != SEMU_OK ||
        semu_apollo4_iom_snapshot_read(candidate.iom4, reader, error) != SEMU_OK ||
        semu_apollo4_iom_snapshot_read(candidate.iom6, reader, error) != SEMU_OK ||
        semu_apollo4_mspi_snapshot_read(candidate.mspi1, reader, error) != SEMU_OK ||
        semu_apollo4_mspi_snapshot_read(candidate.mspi2, reader, error) != SEMU_OK ||
        semu_apollo4_mram_snapshot_read(candidate.mram, reader, error) != SEMU_OK)
        return error->code;
    *soc = candidate;
    return SEMU_OK;
}

semu_status semu_apollo4_snapshot_resolve_event(
    semu_apollo4 *soc, uint32_t kind, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error)
{
    if (soc == NULL || callback == NULL || context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 snapshot event arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (kind == SEMU_SCHED_EVENT_CTIMER)
        return semu_apollo4_timer_snapshot_resolve_event(
            soc->timer, subject, callback, context, error);
    if (kind == SEMU_SCHED_EVENT_STIMER)
        return semu_apollo4_stimer_snapshot_resolve_event(
            soc->stimer, subject, callback, context, error);
    if (kind == SEMU_SCHED_EVENT_UART_RX || kind == SEMU_SCHED_EVENT_UART_TX)
        return semu_apollo4_uart_snapshot_resolve_event(
            soc->uart, kind, subject, callback, context, error);
    semu_error_set(error, SEMU_ERR_CONFLICT,
                   "Apollo4 snapshot event kind is not present");
    return SEMU_ERR_CONFLICT;
}
