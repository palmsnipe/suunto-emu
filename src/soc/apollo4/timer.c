#include "timer.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define TIMER_CHANNEL_COUNT 16u
#define TIMER_CHANNEL_BASE 0x200u
#define TIMER_CHANNEL_STRIDE 0x20u
#define TIMER_GLOBAL_MASK 0x10u
#define TIMER_GLOBAL_STATUS 0x60u
#define TIMER_GLOBAL_STATUS2 0x64u
#define TIMER_GLOBAL_CLEAR 0x68u
#define TIMER_AUXILIARY 0xe8u
#define TIMER_PATTERN 0x104u
#define TIMER_CONTROL 0x00u
#define TIMER_VALUE 0x04u
#define TIMER_COMPARE0 0x08u
#define TIMER_COMPARE1 0x0cu
#define TIMER_INTEN 0x10u
#define TIMER_ENABLE 0x1u
#define TIMER_INTEN_COMPARE 0x100u
typedef struct timer_channel timer_channel;
struct timer_channel {
    struct semu_apollo4_timer *owner;
    uint32_t control;
    uint32_t base_value;
    uint32_t compare[2];
    uint32_t interrupt_enable;
    uint64_t epoch;
    semu_event_id event;
    uint8_t event_valid;
    uint8_t irq_level;
};
struct semu_apollo4_timer {
    semu_scheduler *scheduler;
    semu_apollo4_timer_irq_fn irq;
    void *irq_context;
    timer_channel channels[TIMER_CHANNEL_COUNT];
    uint32_t interrupt_mask;
    uint32_t pending;
    uint32_t status_value;
    uint8_t status_written;
    uint32_t output_control;
    uint32_t auxiliary;
    uint32_t pattern;
};
static semu_status refuse(uint32_t offset, semu_error *error)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "Apollo4 CTIMER refuses offset 0x%03x", offset);
    return SEMU_ERR_UNSUPPORTED;
}
static int valid_access(uint32_t offset, unsigned width)
{
    return width == 4u && (offset & 3u) == 0u;
}
static uint32_t channel_index(uint32_t offset)
{
    return (offset - TIMER_CHANNEL_BASE) / TIMER_CHANNEL_STRIDE;
}
static uint32_t channel_offset(uint32_t offset)
{
    return (offset - TIMER_CHANNEL_BASE) % TIMER_CHANNEL_STRIDE;
}
static int is_channel_offset(uint32_t offset)
{
    return offset >= TIMER_CHANNEL_BASE && offset <
           TIMER_CHANNEL_BASE + TIMER_CHANNEL_COUNT * TIMER_CHANNEL_STRIDE;
}
static uint32_t counter_value(const timer_channel *channel, uint64_t now)
{
    uint64_t elapsed = now - channel->epoch;
    return channel->base_value + (uint32_t)elapsed;
}
static void cancel_channel(timer_channel *channel)
{
    if (channel->event_valid != 0u) {
        (void)semu_scheduler_cancel(channel->owner->scheduler, channel->event);
        channel->event_valid = 0u;
        channel->event = 0u;
    }
}
static int pwm_control(uint32_t channel, uint32_t control)
{
    return channel == 9u && (control == 0xa40u || control == 0xa41u ||
                             control == 0xa42u || control == 0xa44u);
}
static uint32_t control_readback(unsigned channel, uint32_t control)
{
    return pwm_control(channel, control) ? 0xa40u : control;
}
static int supported_control(uint32_t value)
{
    static const uint32_t values[] = {
        0u, 1u, 2u, 3u, 0x110u, 0x111u, 0x112u, 0x120u, 0x121u, 0x122u,
        0x123u,
        0x140u, 0x142u, 0x144u, 0x220u, 0x221u, 0x222u,
        0x1c20u, 0x1c21u, 0x1c22u, 0x1c23u, 0xa40u, 0xa41u, 0xa42u,
        0xa44u
    };
    size_t index;
    for (index = 0u; index < sizeof(values) / sizeof(values[0]); ++index)
        if (values[index] == value) return 1;
    return 0;
}
static uint64_t next_delay(const timer_channel *channel, uint64_t now)
{
    uint32_t current = counter_value(channel, now);
    uint64_t best = UINT64_MAX;
    unsigned index;
    for (index = 0u; index < 2u; ++index) {
        uint32_t compare = channel->compare[index];
        uint64_t delta;
        if (compare == 0u) continue;
        delta = (uint32_t)(compare - current);
        if (delta == 0u) delta = UINT32_MAX;
        if (delta < best) best = delta;
    }
    return best;
}
static void timer_event(void *context, uint64_t now)
{
    timer_channel *channel = (timer_channel *)context;
    semu_apollo4_timer *timer = channel->owner;
    unsigned index = (unsigned)(channel - timer->channels);
    channel->event_valid = 0u;
    channel->event = 0u;
    if ((channel->control & TIMER_ENABLE) == 0u || pwm_control(index, channel->control) ||
        (channel->interrupt_enable & TIMER_INTEN_COMPARE) == 0u ||
        (timer->interrupt_mask & (1u << index)) == 0u) {
        return;
    }
    channel->base_value = 0u;
    channel->epoch = now;
    timer->pending |= 1u << index;
    if (channel->irq_level == 0u) {
        channel->irq_level = 1u;
        if (timer->irq != NULL) timer->irq(timer->irq_context, index, 1);
    }
    if (next_delay(channel, now) != UINT64_MAX) {
        semu_error error; semu_error_clear(&error);
        uint64_t delay = next_delay(channel, now);
        if (semu_scheduler_schedule(timer->scheduler, delay, timer_event,
                                     channel, &channel->event, &error) == SEMU_OK)
            channel->event_valid = 1u;
    }
}
static semu_status reschedule(timer_channel *channel, semu_error *error)
{
    semu_apollo4_timer *timer = channel->owner;
    uint64_t delay;
    cancel_channel(channel);
    if ((channel->control & TIMER_ENABLE) == 0u || pwm_control((unsigned)(channel - timer->channels), channel->control) ||
        (channel->interrupt_enable & TIMER_INTEN_COMPARE) == 0u ||
        (timer->interrupt_mask &
         (1u << (unsigned)(channel - timer->channels))) == 0u)
        return SEMU_OK;
    delay = next_delay(channel, semu_scheduler_now(timer->scheduler));
    if (delay == UINT64_MAX) return SEMU_OK;
    if (semu_scheduler_schedule(timer->scheduler, delay, timer_event, channel,
                                &channel->event, error) != SEMU_OK)
        return error != NULL ? error->code : SEMU_ERR_STATE;
    channel->event_valid = 1u;
    return SEMU_OK;
}
static void clear_pending(semu_apollo4_timer *timer, uint32_t mask)
{
    unsigned index;
    timer->pending &= ~mask;
    for (index = 0u; index < TIMER_CHANNEL_COUNT; ++index) {
        if ((mask & (1u << index)) != 0u &&
            timer->channels[index].irq_level != 0u) {
            timer->channels[index].irq_level = 0u;
            if (timer->irq != NULL)
                timer->irq(timer->irq_context, index, 0);
        }
    }
}
semu_apollo4_timer *semu_apollo4_timer_create(
    semu_scheduler *scheduler, semu_apollo4_timer_irq_fn irq,
    void *irq_context, semu_error *error)
{
    semu_apollo4_timer *timer;
    unsigned index;
    if (scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Apollo4 CTIMER requires scheduler");
        return NULL;
    }
    timer = (semu_apollo4_timer *)calloc(1u, sizeof(*timer));
    if (timer == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate Apollo4 CTIMER");
        return NULL;
    }
    timer->scheduler = scheduler;
    timer->irq = irq;
    timer->irq_context = irq_context;
    for (index = 0u; index < TIMER_CHANNEL_COUNT; ++index)
        timer->channels[index].owner = timer;
    semu_error_clear(error);
    return timer;
}
void semu_apollo4_timer_destroy(semu_apollo4_timer *timer)
{
    unsigned index;
    if (timer == NULL) return;
    for (index = 0u; index < TIMER_CHANNEL_COUNT; ++index)
        cancel_channel(&timer->channels[index]);
    free(timer);
}
void semu_apollo4_timer_reset(semu_apollo4_timer *timer)
{
    unsigned index;
    if (timer == NULL) return;
    for (index = 0u; index < TIMER_CHANNEL_COUNT; ++index) {
        cancel_channel(&timer->channels[index]);
        memset(&timer->channels[index], 0, sizeof(timer->channels[index]));
        timer->channels[index].owner = timer;
    }
    timer->interrupt_mask = 0u;
    timer->pending = 0u;
    timer->status_value = 0u;
    timer->status_written = 0u;
    timer->output_control = 0u;
    timer->auxiliary = 0u;
    timer->pattern = 0u;
}
semu_status semu_apollo4_timer_read(semu_apollo4_timer *timer,
                                    uint32_t offset, unsigned width,
                                    uint32_t *value, semu_error *error)
{
    timer_channel *channel; uint32_t local;
    if (timer == NULL || value == NULL || !valid_access(offset, width))
        return refuse(offset, error);
    if (offset == TIMER_GLOBAL_MASK) *value = timer->interrupt_mask;
    else if (offset == TIMER_GLOBAL_STATUS)
        *value = timer->status_written != 0u ? timer->status_value
                                             : timer->pending;
    else if (offset == TIMER_GLOBAL_STATUS2) *value = 0u;
    else if (offset == TIMER_GLOBAL_CLEAR) *value = timer->output_control;
    else if (offset == TIMER_AUXILIARY) *value = timer->auxiliary;
    else if (offset == TIMER_PATTERN) *value = timer->pattern;
    else if (is_channel_offset(offset)) {
        local = channel_offset(offset);
        channel = &timer->channels[channel_index(offset)];
        switch (local) {
        case TIMER_CONTROL:
            *value = control_readback(channel_index(offset), channel->control);
            break;
        case TIMER_VALUE:
            *value = counter_value(channel, semu_scheduler_now(timer->scheduler));
            break;
        case TIMER_COMPARE0: *value = channel->compare[0]; break;
        case TIMER_COMPARE1: *value = channel->compare[1]; break;
        case TIMER_INTEN: *value = channel->interrupt_enable; break;
        default: return refuse(offset, error);
        }
    } else return refuse(offset, error);
    semu_error_clear(error);
    return SEMU_OK;
}
semu_status semu_apollo4_timer_write(semu_apollo4_timer *timer,
                                     uint32_t offset, unsigned width,
                                     uint32_t value, semu_error *error)
{
    timer_channel *channel; uint32_t local; unsigned index;
    timer_channel old_channel; semu_status status;
    if (timer == NULL || !valid_access(offset, width)) return refuse(offset, error);
    if (offset == TIMER_GLOBAL_MASK) {
        if ((value & ~0x27ffu) != 0u)
            return refuse(offset, error);
        timer->interrupt_mask = value;
        for (index = 0u; index < TIMER_CHANNEL_COUNT; ++index) {
            semu_error local_error;
            semu_error_clear(&local_error);
            (void)reschedule(&timer->channels[index], &local_error);
        }
    } else if (offset == TIMER_GLOBAL_STATUS) {
        if (value != 0u && value != 2u && value != 0x8000000u &&
            value != 0x8000001u)
            return refuse(offset, error);
        timer->status_value = value;
        timer->status_written = 1u;
    } else if (offset == TIMER_GLOBAL_CLEAR) {
        if (value != 1u && value != 0x30000u && value != 0x8000000u &&
            value != 0xc000000u)
            return refuse(offset, error);
        timer->output_control = value;
        if (value == 1u) clear_pending(timer, 1u);
        if (value == 0x8000000u || value == 0xc000000u)
            clear_pending(timer, 1u << 13);
    } else if (offset == TIMER_AUXILIARY) {
        if (value != 0u && value != 0x12u) return refuse(offset, error);
        timer->auxiliary = value;
    } else if (offset == TIMER_PATTERN) {
        if (value != 0u && value != 0x100u && value != 0x2000u &&
            value != 0x2100u && value != 0x10100u && value != 0x10101u &&
            value != 0x12100u && value != 0x12101u)
            return refuse(offset, error);
        timer->pattern = value;
    } else if (!is_channel_offset(offset)) return refuse(offset, error);
    else {
        index = channel_index(offset);
        local = channel_offset(offset);
        channel = &timer->channels[index];
        old_channel = *channel;
        if (local == TIMER_CONTROL) {
            if (!supported_control(value)) return refuse(offset, error);
            channel->base_value = counter_value(channel,
                                                semu_scheduler_now(timer->scheduler));
            channel->epoch = semu_scheduler_now(timer->scheduler);
            channel->control = control_readback(index, value);
        } else if (local == TIMER_VALUE) {
            channel->base_value = value;
            channel->epoch = semu_scheduler_now(timer->scheduler);
        } else if (local == TIMER_COMPARE0) channel->compare[0] = value;
        else if (local == TIMER_COMPARE1) channel->compare[1] = value;
        else if (local == TIMER_INTEN) {
            if ((value & ~TIMER_INTEN_COMPARE) != 0u) return refuse(offset, error);
            channel->interrupt_enable = value;
        } else return refuse(offset, error);
        status = reschedule(channel, error);
        if (status != SEMU_OK) {
            *channel = old_channel;
            (void)reschedule(channel, error);
            return status;
        }
    }
    semu_error_clear(error);
    return SEMU_OK;
}
