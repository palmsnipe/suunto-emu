#include "timer_internal.h"

#include "../../core/scheduler_internal.h"

semu_status semu_apollo4_timer_snapshot_write(
    const semu_apollo4_timer *timer, semu_snapshot_writer *writer,
    semu_error *error)
{
    unsigned index;
    if (timer == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "CTIMER snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (semu_snapshot_writer_u32(writer, timer->interrupt_mask, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, timer->pending, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, timer->status_value, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, timer->status_written, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, timer->output_control, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, timer->auxiliary, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, timer->pattern, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, timer->observed_d8, error) != SEMU_OK)
        return error->code;
    for (index = 0u; index < TIMER_CHANNEL_COUNT; ++index) {
        const timer_channel *channel = &timer->channels[index];
        if (semu_snapshot_writer_u32(writer, channel->control, error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, channel->base_value, error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, channel->compare[0], error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, channel->compare[1], error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, channel->interrupt_enable, error) != SEMU_OK ||
            semu_snapshot_writer_u64(writer, channel->epoch, error) != SEMU_OK ||
            semu_snapshot_writer_u64(writer, channel->event, error) != SEMU_OK ||
            semu_snapshot_writer_u8(writer, channel->event_valid, error) != SEMU_OK ||
            semu_snapshot_writer_u8(writer, channel->irq_level, error) != SEMU_OK)
            return error->code;
    }
    return SEMU_OK;
}

semu_status semu_apollo4_timer_snapshot_read(
    semu_apollo4_timer *timer, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_apollo4_timer candidate;
    unsigned index;
    if (timer == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "CTIMER snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *timer;
    if (semu_snapshot_reader_u32(reader, &candidate.interrupt_mask, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.pending, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.status_value, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &candidate.status_written, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.output_control, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.auxiliary, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.pattern, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.observed_d8, error) != SEMU_OK)
        return error->code;
    for (index = 0u; index < TIMER_CHANNEL_COUNT; ++index) {
        timer_channel *channel = &candidate.channels[index];
        if (semu_snapshot_reader_u32(reader, &channel->control, error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &channel->base_value, error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &channel->compare[0], error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &channel->compare[1], error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &channel->interrupt_enable, error) != SEMU_OK ||
            semu_snapshot_reader_u64(reader, &channel->epoch, error) != SEMU_OK ||
            semu_snapshot_reader_u64(reader, &channel->event, error) != SEMU_OK ||
            semu_snapshot_reader_u8(reader, &channel->event_valid, error) != SEMU_OK ||
            semu_snapshot_reader_u8(reader, &channel->irq_level, error) != SEMU_OK)
            return error->code;
        if (channel->event_valid > 1u || channel->irq_level > 1u ||
            (channel->event_valid != 0u) != (channel->event != 0u)) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                           "invalid CTIMER snapshot event state");
            return SEMU_ERR_FORMAT;
        }
    }
    if (candidate.status_written > 1u) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid CTIMER status flag");
        return SEMU_ERR_FORMAT;
    }
    *timer = candidate;
    return SEMU_OK;
}

semu_status semu_apollo4_timer_snapshot_resolve_event(
    semu_apollo4_timer *timer, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error)
{
    if (timer == NULL || callback == NULL || context == NULL ||
        subject >= TIMER_CHANNEL_COUNT ||
        timer->channels[subject].event_valid == 0u) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "CTIMER snapshot event is not present");
        return SEMU_ERR_CONFLICT;
    }
    *callback = semu_apollo4_timer_event;
    *context = &timer->channels[subject];
    return SEMU_OK;
}
