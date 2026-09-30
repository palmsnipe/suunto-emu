#include "apollo4_internal.h"

#include "../../core/scheduler_internal.h"
#include "../../devices/sapporo_iom4.h"
#include "../../devices/sapporo_rtc.h"
#include "stimer_internal.h"
#include "timer_internal.h"
#include "uart_internal.h"

static semu_status write_child(const semu_apollo4 *soc,
                               semu_snapshot_writer *writer, semu_error *error)
{
    if (semu_snapshot_writer_bytes(writer, soc->gpio_level,
                                   sizeof(soc->gpio_level), error) != SEMU_OK ||
        semu_apollo4_clock_snapshot_write(soc->clock, writer, error) != SEMU_OK ||
        semu_apollo4_power_snapshot_write(soc->power, writer, error) != SEMU_OK ||
        semu_apollo4_watchdog_snapshot_write(soc->watchdog, writer, error) != SEMU_OK ||
        semu_apollo4_rstgen_snapshot_write(soc->rstgen, writer, error) != SEMU_OK ||
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
    /* Ticket 792: the live 2.35 modules follow the shared block, in the
     * fixed order RTC then IOM4, exactly when the profile gate armed
     * them. Snapshot identity pins the profile, so presence is
     * unambiguous for both directions. */
    if (soc->rtc_live != 0 &&
        semu_sapporo_rtc_snapshot_write(soc->rtc, writer, error) != SEMU_OK)
        return error->code;
    if (soc->iom4_live != NULL &&
        semu_sapporo_iom4_snapshot_write(soc->iom4_live, writer, error) != SEMU_OK)
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
    if ((soc->rtc_live != 0 && soc->rtc == NULL) ||
        (soc->rtc_live == 0 && soc->iom4_live != NULL)) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "Apollo4 live-module bindings are inconsistent");
        return SEMU_ERR_STATE;
    }
    return write_child(soc, writer, error);
}

static semu_status read_child(
    semu_apollo4 *soc, semu_snapshot_reader *reader, semu_error *error)
{
    semu_apollo4 candidate;
    size_t index;
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
        semu_apollo4_watchdog_snapshot_read(candidate.watchdog, reader, error) != SEMU_OK ||
        semu_apollo4_rstgen_snapshot_read(candidate.rstgen, reader, error) != SEMU_OK ||
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
    /* Ticket 792: consume the live-module bytes iff this machine's own
     * profile gate armed them; a mismatched image fails as trailing or
     * missing data instead of silently reinterpreting the sections. */
    if (candidate.rtc_live != 0 &&
        semu_sapporo_rtc_snapshot_read(candidate.rtc, reader, error) != SEMU_OK)
        return error->code;
    if (candidate.iom4_live != NULL &&
        semu_sapporo_iom4_snapshot_read(candidate.iom4_live, reader, error) != SEMU_OK)
        return error->code;
    *soc = candidate;
    return SEMU_OK;
}

semu_status semu_apollo4_snapshot_read(
    semu_apollo4 *soc, semu_snapshot_reader *reader, semu_error *error)
{
    semu_snapshot_writer backup;
    semu_snapshot_reader rollback_reader;
    semu_error rollback_error;
    semu_status status;
    size_t offset;

    if (soc == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    offset = reader->offset;
    semu_snapshot_writer_init(&backup);
    status = semu_apollo4_snapshot_write(soc, &backup, error);
    if (status != SEMU_OK) {
        semu_snapshot_writer_destroy(&backup);
        return status;
    }
    status = read_child(soc, reader, error);
    if (status == SEMU_OK) {
        semu_snapshot_writer_destroy(&backup);
        return SEMU_OK;
    }
    reader->offset = offset;
    semu_snapshot_reader_init(&rollback_reader, backup.data, backup.size);
    semu_error_clear(&rollback_error);
    if (read_child(soc, &rollback_reader, &rollback_error) != SEMU_OK ||
        !semu_snapshot_reader_done(&rollback_reader)) {
        semu_snapshot_writer_destroy(&backup);
        semu_error_set(error, SEMU_ERR_STATE,
                       "Apollo4 snapshot rollback failed");
        return SEMU_ERR_STATE;
    }
    semu_snapshot_writer_destroy(&backup);
    return status;
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
    if (kind == SEMU_SCHED_EVENT_SAP235_RTC_ALARM)
        return semu_sapporo_rtc_snapshot_resolve_event(
            soc->rtc, subject, callback, context, error);
    semu_error_set(error, SEMU_ERR_CONFLICT,
                   "Apollo4 snapshot event kind is not present");
    return SEMU_ERR_CONFLICT;
}

semu_status semu_apollo4_snapshot_event_id_matches(
    const semu_apollo4 *soc, uint32_t kind, uint32_t subject,
    semu_event_id event_id, semu_error *error)
{
    const rx_event *rx;
    if (soc == NULL || soc->timer == NULL || soc->stimer == NULL ||
        soc->uart == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 snapshot event identity requires SoC");
        return SEMU_ERR_ARGUMENT;
    }
    if (kind == SEMU_SCHED_EVENT_CTIMER && subject < TIMER_CHANNEL_COUNT &&
        soc->timer->channels[subject].event_valid != 0u &&
        soc->timer->channels[subject].event == event_id)
        return SEMU_OK;
    if (kind == SEMU_SCHED_EVENT_STIMER && subject < 2u &&
        soc->stimer->compare[subject].event_valid != 0u &&
        soc->stimer->compare[subject].event == event_id)
        return SEMU_OK;
    if (kind == SEMU_SCHED_EVENT_UART_TX && subject == 0u &&
        soc->uart->tx_event == event_id && soc->uart->tx_event != 0u)
        return SEMU_OK;
    if (kind == SEMU_SCHED_EVENT_UART_RX) {
        for (rx = soc->uart->rx_events; rx != NULL; rx = rx->next) {
            if (rx->slot == subject && rx->id == event_id)
                return SEMU_OK;
        }
    }
    if (kind == SEMU_SCHED_EVENT_SAP235_RTC_ALARM) {
        if (soc->rtc == NULL) {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                           "Apollo4 RTC event identity requires the live RTC");
            return SEMU_ERR_ARGUMENT;
        }
        return semu_sapporo_rtc_snapshot_event_id_matches(
            soc->rtc, subject, event_id, error);
    }
    semu_error_set(error, SEMU_ERR_FORMAT,
                   "Apollo4 snapshot event identity does not match device");
    return SEMU_ERR_FORMAT;
}

static int has_scheduler_event(const semu_scheduled_event_state *events,
                               size_t count, uint32_t kind, uint32_t subject,
                               semu_event_id event_id)
{
    size_t index;
    for (index = 0u; index < count; ++index) {
        if (events[index].kind == kind && events[index].subject == subject &&
            events[index].id == event_id)
            return 1;
    }
    return 0;
}

semu_status semu_apollo4_snapshot_event_links_match(
    const semu_apollo4 *soc, const semu_scheduled_event_state *events,
    size_t count, semu_error *error)
{
    unsigned index;
    const rx_event *rx;
    if (soc == NULL || soc->timer == NULL || soc->stimer == NULL ||
        soc->uart == NULL || (events == NULL && count != 0u)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 snapshot event linkage arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    for (index = 0u; index < TIMER_CHANNEL_COUNT; ++index) {
        const timer_channel *channel = &soc->timer->channels[index];
        if (channel->event_valid != 0u &&
            !has_scheduler_event(events, count, SEMU_SCHED_EVENT_CTIMER,
                                 index, channel->event)) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                           "CTIMER state has no scheduler event");
            return SEMU_ERR_FORMAT;
        }
    }
    for (index = 0u; index < 2u; ++index) {
        const stimer_compare *compare = &soc->stimer->compare[index];
        if (compare->event_valid != 0u &&
            !has_scheduler_event(events, count, SEMU_SCHED_EVENT_STIMER,
                                 index, compare->event)) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                           "STIMER state has no scheduler event");
            return SEMU_ERR_FORMAT;
        }
    }
    if (soc->uart->tx_event != 0u &&
        !has_scheduler_event(events, count, SEMU_SCHED_EVENT_UART_TX, 0u,
                             soc->uart->tx_event)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "UART TX state has no scheduler event");
        return SEMU_ERR_FORMAT;
    }
    for (rx = soc->uart->rx_events; rx != NULL; rx = rx->next) {
        if (!has_scheduler_event(events, count, SEMU_SCHED_EVENT_UART_RX,
                                 rx->slot, rx->id)) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                           "UART RX state has no scheduler event");
            return SEMU_ERR_FORMAT;
        }
    }
    if (soc->rtc != NULL)
        return semu_sapporo_rtc_snapshot_event_links_match(
            soc->rtc, events, count, error);
    return SEMU_OK;
}
