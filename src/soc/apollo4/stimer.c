#include "stimer_internal.h"
#include "../../core/scheduler_internal.h"
#include <stdlib.h>
enum {
    STIMER_CONFIGURATION = 0x00u,
    STIMER_COUNTER = 0x04u,
    STIMER_COMPARE_A = 0x20u,
    STIMER_COMPARE_C = 0x28u,
    STIMER_NVRAM0 = 0x50u,
    STIMER_NVRAM3 = 0x5cu,
    STIMER_INTERRUPT_ENABLE = 0x100u,
    STIMER_INTERRUPT_STATUS = 0x104u,
    STIMER_INTERRUPT_CLEAR = 0x108u
};
#define STIMER_RESET_CONFIGURATION UINT32_C(0x80000000)
#define STIMER_CONFIGURATION_MASK UINT32_C(0x80000503)
#define STIMER_COMPARE_ENABLE_A UINT32_C(1u << 8)
#define STIMER_COMPARE_ENABLE_C UINT32_C(1u << 10)
#define STIMER_INTERRUPT_ENABLE_MASK UINT32_C(0x101)
#define STIMER_IRQ_MASK UINT32_C(0x105)
#define STIMER_FREQUENCY_HZ UINT64_C(32768)
#define STIMER_NANOSECONDS_PER_SECOND UINT64_C(1000000000)
#define STIMER_COMPARE_WRITE_LATENCY UINT32_C(3)
static const semu_bus_device_ops stimer_ops = {
    semu_apollo4_stimer_read,
    semu_apollo4_stimer_write,
    semu_apollo4_stimer_reset
};
static semu_status refuse(uint32_t offset, semu_error *error,
                          const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Apollo4 STIMER refuses 0x%08x (%s)", offset, reason);
    return SEMU_ERR_UNSUPPORTED;
}
static semu_status validate_access(semu_apollo4_stimer *stimer,
                                   uint32_t offset, unsigned width,
                                   semu_error *error)
{
    if (stimer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 STIMER context required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset & 3u) != 0u) {
        return refuse(offset, error, "aligned 32-bit access required");
    }
    semu_error_clear(error);
    return SEMU_OK;
}
static uint32_t elapsed_ticks(uint64_t elapsed_ns)
{
    uint64_t seconds = elapsed_ns / STIMER_NANOSECONDS_PER_SECOND;
    uint64_t remainder = elapsed_ns % STIMER_NANOSECONDS_PER_SECOND;
    uint64_t seconds_mod = seconds % (UINT64_C(1) << 17);
    uint64_t whole_ticks = seconds_mod * STIMER_FREQUENCY_HZ;
    uint64_t partial_ticks = (remainder * STIMER_FREQUENCY_HZ) /
                             STIMER_NANOSECONDS_PER_SECOND;
    return (uint32_t)(whole_ticks + partial_ticks);
}
static uint32_t counter_now(const semu_apollo4_stimer *stimer)
{
    uint64_t elapsed = semu_scheduler_now(stimer->scheduler) -
                       stimer->counter_epoch;
    return stimer->counter_base + elapsed_ticks(elapsed);
}
static uint64_t ticks_to_nanoseconds(uint32_t ticks)
{
    uint64_t numerator = (uint64_t)ticks * STIMER_NANOSECONDS_PER_SECOND;
    return (numerator + STIMER_FREQUENCY_HZ - 1u) / STIMER_FREQUENCY_HZ;
}
static unsigned compare_number(uint32_t offset)
{
    return offset == STIMER_COMPARE_A ? 0u : 1u;
}
static uint32_t compare_enable_bit(unsigned number)
{
    return number == 0u ? STIMER_COMPARE_ENABLE_A : STIMER_COMPARE_ENABLE_C;
}
static unsigned compare_irq(unsigned number)
{
    return number == 0u ? SEMU_APOLLO4_STIMER_IRQ_A :
                          SEMU_APOLLO4_STIMER_IRQ_C;
}
static void set_irq(semu_apollo4_stimer *stimer, unsigned number, int level)
{
    uint32_t mask = UINT32_C(1) << (number == 0u ? 0u : 2u);
    int old_level = (stimer->pending & mask) != 0u;
    if (level != 0) {
        stimer->pending |= mask;
    } else {
        stimer->pending &= ~mask;
    }
    if (old_level != (level != 0) && stimer->irq != NULL) {
        stimer->irq(stimer->irq_context, compare_irq(number), level != 0);
    }
}
static void cancel_compare(semu_apollo4_stimer *stimer, unsigned number)
{
    stimer_compare *compare = &stimer->compare[number];
    if (compare->event_valid != 0u) {
        (void)semu_scheduler_cancel(stimer->scheduler, compare->event);
        compare->event_valid = 0u;
        compare->event = 0u;
    }
}
static semu_status schedule_compare(semu_apollo4_stimer *stimer,
                                    unsigned number, semu_error *error);
void semu_apollo4_stimer_event(void *context, uint64_t now)
{
    stimer_compare *compare = (stimer_compare *)context;
    semu_apollo4_stimer *stimer = compare->owner;
    unsigned number = compare->number;
    uint32_t current;
    (void)now;
    compare->event_valid = 0u;
    compare->event = 0u;
    current = counter_now(stimer);
    if (compare->enabled == 0u ||
        (stimer->configuration & compare_enable_bit(number)) == 0u ||
        stimer->interrupt_enable == 0u) {
        return;
    }
    if ((uint32_t)(current - compare->deadline) >= UINT32_C(0x80000000)) {
        semu_error error;
        semu_error_clear(&error);
        (void)schedule_compare(stimer, number, &error);
        return;
    }
    set_irq(stimer, number, 1);
}
static semu_status schedule_compare(semu_apollo4_stimer *stimer,
                                    unsigned number, semu_error *error)
{
    stimer_compare *compare = &stimer->compare[number];
    uint32_t current = counter_now(stimer);
    uint32_t delay_ticks = (uint32_t)(compare->deadline - current);
    uint64_t delay_ns = ticks_to_nanoseconds(delay_ticks);
    semu_status status;
    cancel_compare(stimer, number);
    if (compare->enabled == 0u ||
        (stimer->configuration & compare_enable_bit(number)) == 0u ||
        stimer->interrupt_enable == 0u) {
        return SEMU_OK;
    }
    status = semu_scheduler_schedule_tagged(stimer->scheduler, delay_ns,
                                      SEMU_SCHED_EVENT_STIMER,
                                      number, semu_apollo4_stimer_event, compare,
                                      &compare->event, error);
    if (status == SEMU_OK) {
        compare->event_valid = 1u;
    }
    return status;
}
static void reset_state(semu_apollo4_stimer *stimer)
{
    unsigned index;
    for (index = 0u; index < 2u; ++index) {
        cancel_compare(stimer, index);
        stimer->compare[index].deadline = 0u;
        stimer->compare[index].enabled = 0u;
    }
    if (stimer->pending != 0u) {
        if ((stimer->pending & 1u) != 0u) set_irq(stimer, 0u, 0);
        if ((stimer->pending & 4u) != 0u) set_irq(stimer, 1u, 0);
    }
    stimer->counter_epoch = semu_scheduler_now(stimer->scheduler);
    stimer->counter_base = 0u;
    stimer->configuration = STIMER_RESET_CONFIGURATION;
    stimer->interrupt_enable = 0u;
}
semu_apollo4_stimer *semu_apollo4_stimer_create(
    semu_bus *bus, semu_scheduler *scheduler,
    semu_apollo4_stimer_irq_fn irq, void *irq_context, semu_error *error)
{
    semu_apollo4_stimer *stimer;
    if (bus == NULL || scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 STIMER requires bus and scheduler");
        return NULL;
    }
    stimer = (semu_apollo4_stimer *)calloc(1u, sizeof(*stimer));
    if (stimer == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate Apollo4 STIMER");
        return NULL;
    }
    stimer->bus = bus;
    stimer->scheduler = scheduler;
    stimer->irq = irq;
    stimer->irq_context = irq_context;
    stimer->compare[0].owner = stimer;
    stimer->compare[0].number = 0u;
    stimer->compare[1].owner = stimer;
    stimer->compare[1].number = 1u;
    reset_state(stimer);
    if (semu_bus_map_device(bus, "apollo4.stimer", SEMU_APOLLO4_STIMER_BASE,
                            SEMU_APOLLO4_STIMER_SIZE, &stimer_ops, stimer,
                            error) != SEMU_OK) {
        free(stimer);
        return NULL;
    }
    semu_error_clear(error);
    return stimer;
}
void semu_apollo4_stimer_destroy(semu_apollo4_stimer *stimer)
{
    if (stimer != NULL) {
        cancel_compare(stimer, 0u);
        cancel_compare(stimer, 1u);
        free(stimer);
    }
}
void semu_apollo4_stimer_reset(void *context)
{
    semu_apollo4_stimer *stimer = (semu_apollo4_stimer *)context;
    if (stimer != NULL) reset_state(stimer);
}
semu_status semu_apollo4_stimer_read(void *context, uint32_t offset,
                                     unsigned width, uint32_t *value,
                                     semu_error *error)
{
    semu_apollo4_stimer *stimer = (semu_apollo4_stimer *)context;
    semu_status status = validate_access(stimer, offset, width, error);
    if (status != SEMU_OK) return status;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 STIMER read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (offset == STIMER_CONFIGURATION) *value = stimer->configuration;
    else if (offset == STIMER_COUNTER) *value = counter_now(stimer);
    else if (offset == STIMER_COMPARE_A || offset == STIMER_COMPARE_C)
        *value = stimer->compare[compare_number(offset)].deadline;
    else if (offset >= STIMER_NVRAM0 && offset <= STIMER_NVRAM3)
        *value = stimer->nvram[(offset - STIMER_NVRAM0) / 4u];
    else if (offset == STIMER_INTERRUPT_ENABLE)
        *value = stimer->interrupt_enable;
    else if (offset == STIMER_INTERRUPT_STATUS) *value = stimer->pending;
    else return refuse(offset, error, "offset not evidenced");
    semu_error_clear(error);
    return SEMU_OK;
}
semu_status semu_apollo4_stimer_write(void *context, uint32_t offset,
                                      unsigned width, uint32_t value,
                                      semu_error *error)
{
    semu_apollo4_stimer *stimer = (semu_apollo4_stimer *)context;
    semu_status status = validate_access(stimer, offset, width, error);
    unsigned number;
    uint32_t current;
    if (status != SEMU_OK) return status;
    if (offset >= STIMER_NVRAM0 && offset <= STIMER_NVRAM3) {
        stimer->nvram[(offset - STIMER_NVRAM0) / 4u] = value;
    } else if (offset == STIMER_CONFIGURATION) {
        if ((value & ~STIMER_CONFIGURATION_MASK) != 0u)
            return refuse(offset, error, "configuration bits not evidenced");
        stimer->configuration = value;
        for (number = 0u; number < 2u; ++number) {
            status = schedule_compare(stimer, number, error);
            if (status != SEMU_OK) return status;
        }
    } else if (offset == STIMER_INTERRUPT_ENABLE) {
        if ((value & ~STIMER_INTERRUPT_ENABLE_MASK) != 0u)
            return refuse(offset, error, "interrupt control not evidenced");
        stimer->interrupt_enable = value;
        for (number = 0u; number < 2u; ++number) {
            status = schedule_compare(stimer, number, error);
            if (status != SEMU_OK) return status;
        }
    } else if (offset == STIMER_INTERRUPT_CLEAR) {
        if ((value & ~STIMER_IRQ_MASK) != 0u)
            return refuse(offset, error, "only IRQA and IRQC are evidenced");
        if ((value & 1u) != 0u) set_irq(stimer, 0u, 0);
        if ((value & 4u) != 0u) set_irq(stimer, 1u, 0);
    } else if (offset == STIMER_COMPARE_A || offset == STIMER_COMPARE_C) {
        number = compare_number(offset);
        /*
         * Apollo advances the main counter by three ticks while committing a
         * compare write.  The firmware reads the resulting absolute target,
         * so model that latency before applying the relative value.  Event
         * scheduling uses the same post-write counter and cannot drift behind
         * the register-visible clock.
         */
        stimer->counter_base += STIMER_COMPARE_WRITE_LATENCY;
        current = counter_now(stimer);
        stimer->compare[number].deadline = current + value;
        stimer->compare[number].enabled = 1u;
        status = schedule_compare(stimer, number, error);
        if (status != SEMU_OK) return status;
    } else {
        return refuse(offset, error, "offset not evidenced or read-only");
    }
    semu_error_clear(error);
    return SEMU_OK;
}
const semu_bus_device_ops *semu_apollo4_stimer_bus_ops(void)
{
    return &stimer_ops;
}

semu_status semu_apollo4_stimer_snapshot_write(
    const semu_apollo4_stimer *stimer, semu_snapshot_writer *writer,
    semu_error *error)
{
    size_t index;
    if (stimer == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "STIMER snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (semu_snapshot_writer_u64(writer, stimer->counter_epoch, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, stimer->counter_base, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, stimer->configuration, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, stimer->interrupt_enable, error) != SEMU_OK ||
        semu_snapshot_writer_u32(writer, stimer->pending, error) != SEMU_OK)
        return error->code;
    for (index = 0u; index < SEMU_ARRAY_LEN(stimer->nvram); ++index) {
        if (semu_snapshot_writer_u32(writer, stimer->nvram[index], error) != SEMU_OK)
            return error->code;
    }
    for (index = 0u; index < SEMU_ARRAY_LEN(stimer->compare); ++index) {
        const stimer_compare *compare = &stimer->compare[index];
        if (semu_snapshot_writer_u32(writer, compare->deadline, error) != SEMU_OK ||
            semu_snapshot_writer_u64(writer, compare->event, error) != SEMU_OK ||
            semu_snapshot_writer_u8(writer, compare->enabled, error) != SEMU_OK ||
            semu_snapshot_writer_u8(writer, compare->event_valid, error) != SEMU_OK)
            return error->code;
    }
    return SEMU_OK;
}

semu_status semu_apollo4_stimer_snapshot_read(
    semu_apollo4_stimer *stimer, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_apollo4_stimer candidate;
    size_t index;
    if (stimer == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "STIMER snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *stimer;
    if (semu_snapshot_reader_u64(reader, &candidate.counter_epoch, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.counter_base, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.configuration, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.interrupt_enable, error) != SEMU_OK ||
        semu_snapshot_reader_u32(reader, &candidate.pending, error) != SEMU_OK)
        return error->code;
    for (index = 0u; index < SEMU_ARRAY_LEN(candidate.nvram); ++index) {
        if (semu_snapshot_reader_u32(reader, &candidate.nvram[index], error) != SEMU_OK)
            return error->code;
    }
    for (index = 0u; index < SEMU_ARRAY_LEN(candidate.compare); ++index) {
        stimer_compare *compare = &candidate.compare[index];
        if (semu_snapshot_reader_u32(reader, &compare->deadline, error) != SEMU_OK ||
            semu_snapshot_reader_u64(reader, &compare->event, error) != SEMU_OK ||
            semu_snapshot_reader_u8(reader, &compare->enabled, error) != SEMU_OK ||
            semu_snapshot_reader_u8(reader, &compare->event_valid, error) != SEMU_OK)
            return error->code;
        if (compare->enabled > 1u || compare->event_valid > 1u ||
            (compare->event_valid != 0u) != (compare->event != 0u) ||
            (compare->event_valid != 0u &&
             (compare->enabled == 0u ||
              (candidate.configuration & compare_enable_bit((unsigned)index)) == 0u ||
              candidate.interrupt_enable == 0u))) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                           "invalid STIMER snapshot event state");
            return SEMU_ERR_FORMAT;
        }
    }
    if ((candidate.configuration & ~STIMER_CONFIGURATION_MASK) != 0u ||
        (candidate.interrupt_enable & ~STIMER_INTERRUPT_ENABLE_MASK) != 0u ||
        (candidate.pending & ~UINT32_C(0x5)) != 0u) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid STIMER snapshot control state");
        return SEMU_ERR_FORMAT;
    }
    *stimer = candidate;
    return SEMU_OK;
}

semu_status semu_apollo4_stimer_snapshot_resolve_event(
    semu_apollo4_stimer *stimer, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error)
{
    if (stimer == NULL || callback == NULL || context == NULL || subject >= 2u ||
        stimer->compare[subject].event_valid == 0u) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "STIMER snapshot event is not present");
        return SEMU_ERR_CONFLICT;
    }
    *callback = semu_apollo4_stimer_event;
    *context = &stimer->compare[subject];
    return SEMU_OK;
}
