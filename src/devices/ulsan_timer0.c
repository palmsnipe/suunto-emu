/*
 * Ulsan 2.35.36 TIMER block at 0x40008000 (ticket 730, E-ULS-0021,
 * E-ULS-0035).
 *
 * E-ULS-0021 plane (unchanged): boot touches seven registers per
 * delta pass - TIMER0 offsets 0x10 (read, read-modify-write store
 * 0x2), 0x60 (read, store 0x4), 0x68 (store 0x4); on the TIMER1 page
 * reads and stores 0, 0xa20, 0xa22, 0xa20, 0xa21 at 0x220 (control),
 * stores 0, 0x20, 0x3d at 0x228 (compare 0), 0 at 0x22c and 0 at
 * 0x230. The lane answers reads and stores writes silently for these
 * registers; every other address, width, and read of a
 * write-only-observed register still refuses. The IRQ 68 service
 * routine (lane hook 0x000c30ff; tree handler reached via the
 * comparator wake at pc 0x000c3240) additionally reads 0x64, which
 * the lane answers 0x00000000 from a registered framework store
 * (lane samples at 1.0 s and 1.003 s of virtual time, run twice);
 * this model keeps 0x64 as a plain store like its read-observed
 * neighbours: the lane never faults writes to its registered
 * interrupt registers, so refusing a hypothetical write would not be
 * lane-equivalent; no lane write to 0x64 was ever logged.
 *
 * E-ULS-0035 comparator wake (the source that releases the suspend
 * loop after the IOM4 doorbell burst): the lane's upstream
 * Timers.AmbiqApollo4_Timer at 0x40008000 wires per-comparator lines
 * "N -> timerIRQ@N | nvic@(67+N)", so comparator channel 1 drives
 * both the combined TIMER0 line (IRQ 14) and IRQ 68
 * (platforms/cpus/ambiq-apollo4.repl). The lane log shows that line
 * pulsing False->True->False repeatedly, starting immediately after
 * the guest programs the TIMER1 page. Lane register dumps at 0.3 s of
 * virtual time (run twice, identical) hold Timer1Control
 * (0x220)=0xa21 (enable = bit 0), compare 0x228=0xa8=168, and the
 * TIMER1 counter (0x224) advancing and wrapping at the compare value.
 * The observed False->True gaps at compare 168 are 9.6-10.8 ms of
 * virtual time (median ~10.15 ms), so one tick is 57-64 us; this
 * model uses the Apollo4 GSTATCLK/2 tick of 61035 ns (32.768 kHz /
 * 2), the Apollo4 clock that places the 168-tick period (10.254 ms)
 * inside the observed range.
 *
 * Observed-only semantics: writing 0x220 with bit 0 set (0xa21,
 * always the last control store after the compare store) (re)arms a
 * one-shot compare event compare*tick in the future; writing bit 0
 * clear (0, 0xa20, 0xa22 - all observed stop stores) cancels it. A
 * compare event drives IRQ 14 and IRQ 68 high through the machine
 * sink, schedules the observed fall back low one tick later (the
 * lane's pulses are momentary: True at e.g. 17:26:10.1822, False by
 * 10.1827), and re-arms the next event while bit 0 stays set (the
 * lane counter wraps at compare). No other store re-arms: the guest
 * always follows a compare rewrite by a control store, so arming
 * from a compare write is unobserved and not implemented. The lane
 * pulses regardless of the guest's GlobalEnable (0x10, lane read
 * 0x7ff, boot stores 0x2) dance, so 0x10 stays a plain store with no
 * gating. The counter register (0x224) is not readable here: boot
 * never read it (lane shows reads of 0x220 only).
 */

#include "ulsan_timer0.h"

#include "semu/scheduler.h"

#define TIMER0_BASE 0x40008000u
#define TIMER0_SIZE 0x800u
#define TIMER1_CONTROL 0x220u
#define TIMER1_CONTROL_ENABLE 1u
#define TIMER1_CONTROL_CLEAR 2u
#define TIMER7_CONTROL 0x2e0u
#define TIMER7_COMPARE0 0x2e8u
#define TIMER7_COMPARE1 0x2ecu
#define TIMER7_MODE 0x2f0u
#define TIMER_TICK_NS UINT64_C(61035)
#define TIMER1_244_HZ UINT64_C(32768)
#define NS_PER_SECOND UINT64_C(1000000000)
#define TIMER_COMBINED_IRQ 14u
#define TIMER1_IRQ 68u

static semu_ulsan_timer0 timer0_instance;

static int timer0_slot(uint32_t offset, unsigned *slot, int write, int race_s_244)
{
    switch (offset) {
    case 0x010u: *slot = 0u; break;
    case 0x060u: *slot = 1u; break;
    case 0x068u: *slot = 2u; break;
    case 0x220u: *slot = 3u; break;
    case 0x064u: *slot = 7u; break;
    case 0x228u: *slot = 4u; break;
    case 0x22cu: *slot = 5u; break;
    case 0x230u: *slot = 6u; break;
    case TIMER7_CONTROL: *slot = 8u; return race_s_244;
    case TIMER7_COMPARE0: *slot = 9u; return race_s_244 && write;
    case TIMER7_COMPARE1: *slot = 10u; return race_s_244 && write;
    case TIMER7_MODE: *slot = 11u; return race_s_244 && write;
    default: return 0;
    }
    if (!write) {
        switch (offset) {
        case 0x010u: /* fallthrough */
        case 0x060u: /* fallthrough */
        case 0x064u: /* fallthrough */
        case 0x220u:
            return 1; /* reads observed only for these four */
        default: return 0;
        }
    }
    return 1;
}

static void timer0_set_line(semu_ulsan_timer0 *s, int level)
{
    if (s->irq_sink == NULL || ((s->line_high != 0) == (level != 0))) {
        return;
    }
    s->line_high = level != 0 ? 1 : 0;
    s->irq_sink(s->irq_context, TIMER_COMBINED_IRQ, level);
    s->irq_sink(s->irq_context, TIMER1_IRQ, level);
}

static void timer0_compare_event(void *context, uint64_t now_ns);

static void timer0_arm(semu_ulsan_timer0 *s)
{
    semu_error error;
    uint64_t delay_ns;

    if (s->scheduler == NULL) {
        return;
    }
    if (s->compare_event_valid) {
        (void)semu_scheduler_cancel_owned(s->scheduler, s->compare_event,
                                         timer0_compare_event, s);
        s->compare_event_valid = 0;
    }
    /* No observed run ever armed with compare 0; the first enabled
     * control store followed a compare store of 0x20. */
    if (s->regs[4] == 0u) {
        return;
    }
    if (s->race_s_244) {
        /* E-ULS-0070: round up to the first whole ns at the compare tick.
         * The 32-bit compare fits in 64 bits after scaling and rounding. */
        delay_ns = ((uint64_t)s->regs[4] * NS_PER_SECOND + TIMER1_244_HZ - 1u)
                   / TIMER1_244_HZ;
    } else {
        delay_ns = (uint64_t)s->regs[4] * TIMER_TICK_NS;
    }
    if (semu_scheduler_schedule(s->scheduler,
            delay_ns,
            timer0_compare_event, s, &s->compare_event, &error) == SEMU_OK) {
        s->compare_event_valid = 1;
    }
}

static void timer0_clear_event(void *context, uint64_t now_ns)
{
    semu_ulsan_timer0 *s = (semu_ulsan_timer0 *)context;

    (void)now_ns;
    s->clear_event_valid = 0;
    timer0_set_line(s, 0);
}

static void timer0_compare_event(void *context, uint64_t now_ns)
{
    semu_ulsan_timer0 *s = (semu_ulsan_timer0 *)context;
    semu_error error;

    (void)now_ns;
    s->compare_event_valid = 0;
    timer0_set_line(s, 1);
    if (semu_scheduler_schedule(s->scheduler, TIMER_TICK_NS,
            timer0_clear_event, s, &s->clear_event, &error) == SEMU_OK) {
        s->clear_event_valid = 1;
    }
    timer0_arm(s); /* the lane counter wraps at compare: pulses repeat */
}

static semu_status timer0_read(void *context, uint32_t offset,
                               unsigned width, uint32_t *value,
                               semu_error *error)
{
    unsigned slot = 0u;

    const semu_ulsan_timer0 *s = (const semu_ulsan_timer0 *)context;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan TIMER read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset & 3u) != 0u ||
        !timer0_slot(offset, &slot, 0, s->race_s_244)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan TIMER read at 0x%08x width %u is "
                       "unsupported", offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    *value = s->regs[slot];
    return SEMU_OK;
}

static semu_status timer0_write(void *context, uint32_t offset,
                                unsigned width, uint32_t value,
                                semu_error *error)
{
    semu_ulsan_timer0 *s = (semu_ulsan_timer0 *)context;
    unsigned slot = 0u;

    if (width != 4u || (offset & 3u) != 0u ||
        !timer0_slot(offset, &slot, 1, s->race_s_244) ||
        (offset == TIMER7_CONTROL && value != 0u &&
         value != 0x120u && value != 0x122u) ||
        ((offset == TIMER7_COMPARE0 || offset == TIMER7_COMPARE1) &&
         value != UINT32_MAX) ||
        (offset == TIMER7_MODE && value != 0u)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan TIMER write at 0x%08x width %u is "
                       "unsupported", offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    if ((offset == TIMER1_CONTROL && s->race_s_244) || offset == TIMER7_CONTROL) {
        value &= ~TIMER1_CONTROL_CLEAR; /* E-ULS-0065 and E-ULS-0081 */
    }
    s->regs[slot] = value;
    if (offset == TIMER1_CONTROL) {
        if ((value & TIMER1_CONTROL_ENABLE) != 0u) {
            timer0_arm(s);
        } else if (s->compare_event_valid && s->scheduler != NULL) {
            (void)semu_scheduler_cancel_owned(s->scheduler, s->compare_event,
                                         timer0_compare_event, s);
            s->compare_event_valid = 0;
        }
    }
    return SEMU_OK;
}

void semu_ulsan_timer0_reset_state(semu_ulsan_timer0 *s)
{    unsigned index = 0u;

    if (s->compare_event_valid && s->scheduler != NULL) {
        (void)semu_scheduler_cancel_owned(s->scheduler, s->compare_event,
                                         timer0_compare_event, s);
    }
    if (s->clear_event_valid && s->scheduler != NULL) {
        (void)semu_scheduler_cancel_owned(s->scheduler, s->clear_event,
                                         timer0_clear_event, s);
    }
    s->compare_event_valid = 0;
    s->clear_event_valid = 0;
    timer0_set_line(s, 0);
    for (index = 0u; index < SEMU_ARRAY_LEN(s->regs); ++index) {
        s->regs[index] = 0u;
    }
    if (s->race_s_244) {
        s->regs[0] = 0x7ffu; /* GlobalEnable reset (E-ULS-0065) */
    }
}

static void timer0_reset(void *context)
{
    semu_ulsan_timer0_reset_state((semu_ulsan_timer0 *)context);
}

static const semu_bus_device_ops timer0_ops = {
    timer0_read,
    timer0_write,
    timer0_reset
};

static semu_status timer0_map(semu_bus *bus, semu_ulsan_timer0 *state,
                               int race_s_244, semu_scheduler *scheduler,
                               semu_apollo4_irq_fn sink, void *context,
                               semu_error *error)
{
    semu_status status;

    if (state == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Ulsan TIMER needs state");
        return SEMU_ERR_ARGUMENT;
    }
    status = semu_bus_map_device(bus, "ulsan.timer0", TIMER0_BASE, TIMER0_SIZE,
                                 &timer0_ops, state, error);
    if (status == SEMU_OK) {
        *state = (semu_ulsan_timer0){0};
        state->race_s_244 = race_s_244;
        state->scheduler = scheduler;
        state->irq_sink = sink;
        state->irq_context = context;
        semu_ulsan_timer0_reset_state(state);
    }
    return status;
}

semu_status semu_ulsan_timer0_map(semu_bus *bus, semu_error *error)
{
    return timer0_map(bus, &timer0_instance, 0, NULL, NULL, NULL, error);
}

semu_status semu_ulsan_244_timer0_map(semu_bus *bus, semu_ulsan_timer0 *state,
                                     semu_scheduler *scheduler,
                                     semu_apollo4_irq_fn sink, void *context,
                                     semu_error *error)
{
    return timer0_map(bus, state, 1, scheduler, sink, context, error);
}

void semu_ulsan_timer0_attach(semu_scheduler *scheduler,
                              semu_apollo4_irq_fn sink, void *context)
{
    /* The machine creates the scheduler before mapping the Ulsan
     * board, so comparator events are schedulable from the first
     * guest control store. */
    timer0_instance.scheduler = scheduler;
    timer0_instance.irq_sink = sink;
    timer0_instance.irq_context = context;
}
