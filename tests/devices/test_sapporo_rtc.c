/*
 * Sapporo-2.35.34 live RTC tests (E-SAP-0035 law rewrite).
 *
 * The module mirrors the lane Timers.AmbiqApollo4_RTC register law:
 * CTRL stores bits 4:0 only, counter stores are WRTC-gated with the
 * CNTL/CNTU pair committing the epoch clock, the alarm cadence comes
 * from RPT with per-unit repeats, IRQ = Enable && Status until
 * InterruptClear, and every lane probe read-back from the
 * byte-identical rb3/law pairs is reproduced here. In-window
 * unmodelled offsets read 0 and writes drop (lane-observed, rb2/rb3);
 * the window ends at 0x210 and non-4 widths refuse. The boot case
 * pins the recorded post-wake stop and is manifest-gated.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/machine.h"
#include "semu/manifest.h"
#include "semu/scheduler.h"
#include "test.h"
#include "../../src/boards/machine_internal.h"
#include "../../src/devices/sapporo_rtc.h"

static unsigned g_alarm_count;
static unsigned g_alarm_irq[24];
static unsigned g_alarm_level[24];

static void alarm_sink(void *context, unsigned irq, int level)
{
    (void)context;
    if (g_alarm_count < 24u) {
        g_alarm_irq[g_alarm_count] = irq;
        g_alarm_level[g_alarm_count] = level != 0 ? 1u : 0u;
    }
    ++g_alarm_count;
}

/* Live-mode bus: scheduler + SoC + explicit attach (maps reset detach). */
static semu_bus *live_bus(semu_scheduler **scheduler, semu_error *error)
{
    semu_apollo4 *soc;
    semu_bus *bus;

    *scheduler = semu_scheduler_create(error);
    if (*scheduler == NULL) {
        return NULL;
    }
    bus = semu_bus_create(error);
    if (bus == NULL) {
        return NULL;
    }
    soc = semu_apollo4_create(bus, error);
    if (soc == NULL || semu_apollo4_init(soc, *scheduler, NULL, NULL,
                                         error) != SEMU_OK ||
        semu_apollo4_select_profile(soc, "sapporo-2.35.34",
                                    error) != SEMU_OK) {
        return NULL;
    }
    semu_sapporo_rtc_attach(*scheduler, alarm_sink, NULL);
    return bus;
}

static void rd(semu_test_context *c, semu_bus *b, uint32_t a, uint32_t want)
{
    semu_error error;
    uint32_t value = 0xdeadbeefu;

    semu_error_clear(&error);
    SEMU_TEST_ASSERT(c, semu_bus_read(b, a, 4u, &value, &error) == SEMU_OK);
    SEMU_TEST_EQ_U64(c, want, value);
}

static void wr(semu_test_context *c, semu_bus *b, uint32_t a, uint32_t v)
{
    semu_error error;

    semu_error_clear(&error);
    SEMU_TEST_ASSERT(c,
                     semu_bus_write(b, a, 4u, v, &error) == SEMU_OK);
}

static void adv(semu_test_context *c, semu_scheduler *s, uint64_t ns)
{
    semu_error error;

    semu_error_clear(&error);
    SEMU_TEST_ASSERT(c, semu_scheduler_advance(s, ns, &error) == SEMU_OK);
}

static void test_stub_profiles_stay_inert(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;
    semu_apollo4 *soc;
    semu_error error;
    uint32_t value = 0xdeadbeefu;

    g_alarm_count = 0u;
    semu_error_clear(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, scheduler != NULL);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    soc = semu_apollo4_create(bus, &error);
    SEMU_TEST_ASSERT(context, soc != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_init(soc, scheduler, NULL, NULL, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_select_profile(
        soc, "sapporo-2.33.16", &error));
    SEMU_TEST_ASSERT(context, semu_apollo4_select_profile(NULL, NULL,
                                                          &error) != SEMU_OK);
    semu_sapporo_rtc_attach(scheduler, alarm_sink, NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A00u, 4u, UINT32_C(1),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x40004A08u, 4u, UINT32_C(1),
                                    &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(scheduler, UINT64_C(2100000000),
                                            &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), g_alarm_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(bus, 0x40004820u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), value); /* stub answers 0 */
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

/* Lane cold map + write law (rb3 pair): CTRL masks to 5 bits, the
 * epoch counters answer 0 / 0x14700101, ALML validates BCD per field
 * with the lane's hex compare, INTEN keeps bit 0, INTCLR/INTSET read
 * 0, and every other in-window offset answers 0. */
static void test_lane_write_law(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;
    size_t i;
    const uint32_t zeros[] = {0x40004804u, 0x40004810u, 0x40004850u,
                              0x40004900u, 0x40004A0Cu, 0x40004A08u};

    g_alarm_count = 0u;
    semu_error_clear(&error);
    bus = live_bus(&scheduler, &error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    rd(context, bus, 0x40004800u, 0u);
    rd(context, bus, 0x40004820u, 0u);
    rd(context, bus, 0x40004824u, 0x14700101u); /* 1970-01-01, Thu, CB */
    for (i = 0u; i < sizeof(zeros) / sizeof(zeros[0]); ++i) {
        semu_error_clear(&error);
        SEMU_TEST_ASSERT(context,
                         semu_bus_read(bus, zeros[i], 4u, &value, &error) ==
                             SEMU_OK);
        SEMU_TEST_EQ_U64(context, UINT64_C(0), value);
        SEMU_TEST_ASSERT(context,
                         semu_bus_write(bus, zeros[i], 4u,
                                        UINT32_C(0xa5a5a5a5),
                                        &error) == SEMU_OK);
        SEMU_TEST_ASSERT(context,
                         semu_bus_read(bus, zeros[i], 4u, &value, &error) ==
                             SEMU_OK);
        SEMU_TEST_EQ_U64(context, UINT64_C(0), value); /* drop, not fault */
    }
    /* ALML field law: 0xa5 rejects (0x99), 0x25 passes (<=0x59 hex
     * compare), 0x25 > 0x23 rejects in ALMHR: read back 0x00252500. */
    wr(context, bus, 0x40004830u, 0xa5a5a5a5u);
    rd(context, bus, 0x40004830u, 0x00252500u);
    wr(context, bus, 0x40004830u, 0x3f3f3f3fu);
    rd(context, bus, 0x40004830u, 0x003f3f3fu);
    wr(context, bus, 0x40004A00u, 0xa5u);
    rd(context, bus, 0x40004A00u, 1u); /* InterruptEnable keeps bit 0 */
    wr(context, bus, 0x40004800u, 0xffffffffu);
    rd(context, bus, 0x40004800u, 0x1fu); /* CTRL bits 4:0 only */
    /* WRTC gating: with bit 0 clear the counter stores drop whole. */
    wr(context, bus, 0x40004800u, 0x0u);
    wr(context, bus, 0x40004820u, 0x100u);
    rd(context, bus, 0x40004820u, 0u);
    rd(context, bus, 0x40004804u, 0u); /* Status stays not-busy */
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

/* Window, widths, refusal cases: unmapped beyond 0x210, narrow
 * widths refuse fail-closed (no census access ever used them). */
static void test_access_refusals(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;
    semu_error error;
    uint32_t value = 0u;

    g_alarm_count = 0u;
    semu_error_clear(&error);
    bus = live_bus(&scheduler, &error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_ASSERT(context, semu_bus_read(bus, 0x40004A10u, 4u, &value,
                                             &error) != SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(bus, 0x40004A10u, 4u,
                                             UINT32_C(1), &error) != SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_read(bus, 0x40004800u, 2u, &value,
                                            &error) != SEMU_OK);
    SEMU_TEST_ASSERT(context, semu_bus_write(bus, 0x40004800u, 1u,
                                             UINT32_C(1), &error) != SEMU_OK);
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

/* CNTL/CNTU pair commits the epoch clock; RSTOP freezes it; WriteBusy
 * and CTERR follow the lane flow (CNTL sets busy, CNTU clears it and
 * latches CTERR when the CNTL read saw the same tick). */
static void test_counter_set_pair(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;

    g_alarm_count = 0u;
    bus = live_bus(&scheduler, &(semu_error){0});
    SEMU_TEST_ASSERT(context, bus != NULL);
    wr(context, bus, 0x40004800u, 0x01u); /* WRTC, RPT=Disabled */
    rd(context, bus, 0x40004820u, 0u); /* first read refreshes fields */
    wr(context, bus, 0x40004820u, 0x100u); /* seconds := 01 */
    rd(context, bus, 0x40004804u, 1u); /* WriteBusy set by CNTL */
    wr(context, bus, 0x40004824u, 0x14700101u); /* commit 00:00:01.00 */
    rd(context, bus, 0x40004804u, 0u); /* CNTU clears WriteBusy */
    rd(context, bus, 0x40004820u, 0x100u); /* clock now answers the set */
    /* the CNTL read cleared the CTERR flag (lane readError=false) */
    rd(context, bus, 0x40004824u, 0x14700101u);
    adv(context, scheduler, UINT64_C(1500000000));
    rd(context, bus, 0x40004820u, 0x250u); /* 2.50 s: sec 02 hun 50 */
    wr(context, bus, 0x40004800u, 0x11u); /* RSTOP */
    adv(context, scheduler, UINT64_C(5000000000));
    rd(context, bus, 0x40004820u, 0x250u); /* frozen while RSTOP */
    wr(context, bus, 0x40004800u, 0x01u); /* resume */
    adv(context, scheduler, UINT64_C(1000000000));
    rd(context, bus, 0x40004820u, 0x350u); /* continues from 2.50 + 1 */
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

/* The guest census law: CTRL=0xE (RPT=Second), ALML=0, then the
 * clear/enable pair; IRQ rises at the whole second while
 * InterruptStatus holds until InterruptClear drops it; the repeat is
 * per-unit; a re-written ALML recomputes the same occurrence. */
static void test_second_alarm_law(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;
    int line = -1;

    g_alarm_count = 0u;
    bus = live_bus(&scheduler, &(semu_error){0});
    SEMU_TEST_ASSERT(context, bus != NULL);
    adv(context, scheduler, UINT64_C(11800000)); /* census arm timing */
    wr(context, bus, 0x40004830u, 0u);
    wr(context, bus, 0x40004800u, 0xEu);
    wr(context, bus, 0x40004A08u, 1u); /* clear (nothing pending) */
    wr(context, bus, 0x40004A00u, 1u); /* enable */
    /* the occurrence is scheduled at the next whole second 1.0 */
    SEMU_TEST_EQ_U64(context, UINT64_C(1), semu_sapporo_rtc_probe_pending(
                                       &line));
    SEMU_TEST_EQ_U64(context, UINT64_C(0), (uint64_t)line);
    adv(context, scheduler, UINT64_C(1000000000) - 11800000u - 1u);
    SEMU_TEST_EQ_U64(context, UINT64_C(0), g_alarm_count);
    adv(context, scheduler, UINT64_C(1)); /* whole second 1.000000000 */
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_count);
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_alarm_irq[0]);
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_level[0]);
    rd(context, bus, 0x40004A04u, 1u); /* InterruptStatus ALM */
    wr(context, bus, 0x40004A0Cu, 0u); /* Set with 0 changes nothing */
    adv(context, scheduler, UINT64_C(400000));
    wr(context, bus, 0x40004A08u, 1u); /* acknowledge clear */
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_alarm_count);
    SEMU_TEST_EQ_U64(context, UINT64_C(0), g_alarm_level[1]);
    rd(context, bus, 0x40004A04u, 0u);
    /* Re-arm recomputation keeps the 2.000000000 occurrence. */
    wr(context, bus, 0x40004830u, 0u);
    adv(context, scheduler,
        UINT64_C(2000000000) - 1000400000u - 1u); /* to 1.999999999 */
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_alarm_count);
    adv(context, scheduler, UINT64_C(1));
    SEMU_TEST_EQ_U64(context, UINT64_C(3), g_alarm_count);
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_level[2]);
    /* Disable keeps the line silent although Status latches. */
    wr(context, bus, 0x40004A08u, 1u);
    SEMU_TEST_EQ_U64(context, UINT64_C(4), g_alarm_count); /* fall */
    wr(context, bus, 0x40004A00u, 0u);
    adv(context, scheduler, UINT64_C(1000000000));
    SEMU_TEST_EQ_U64(context, UINT64_C(4), g_alarm_count);
    rd(context, bus, 0x40004A04u, 1u); /* status lags, line silent */
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

/* Reset returns the peripheral to the epoch cold state (lane Reset:
 * fields re-init, IRQ falls, alarm cleared) and cancels the pending
 * occurrence. */
static void test_reset_reinitialises(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;

    g_alarm_count = 0u;
    bus = live_bus(&scheduler, &(semu_error){0});
    SEMU_TEST_ASSERT(context, bus != NULL);
    wr(context, bus, 0x40004830u, 0u);
    wr(context, bus, 0x40004800u, 0xEu);
    wr(context, bus, 0x40004A00u, 1u);
    /* at clock zero the alarm-0 occurrence is due now: Limit == Value
     * fires on the next advance, so the line rose on enable */
    adv(context, scheduler, UINT64_C(999000000));
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_count);
    semu_bus_reset(bus); /* Reset falls the line and clears the timer */
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_alarm_count);
    adv(context, scheduler, UINT64_C(2000000000));
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_alarm_count);
    rd(context, bus, 0x40004800u, 0u);
    rd(context, bus, 0x40004A00u, 0u);
    rd(context, bus, 0x40004A04u, 0u);
    /* lane Reset re-bases the clock to the machine clock (virtual
     * now), not to zero: 999 ms + 2000 ms = 2.999 s of BCD hundredths */
    rd(context, bus, 0x40004820u, 0x299u);
    rd(context, bus, 0x40004824u, 0x14700101u);
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

/* E-SAP-0034/0035: the machine's SRAM-retaining software reset zeroes
 * the scheduler and drops the pending occurrence without a device bus
 * reset. STAT (hardware-retained semantics) keeps the line high; the
 * new boot's own clear/enable writes resolve it, and a regressed
 * scheduler clock must not veto the recomputed occurrence. */
static void test_software_reset_flush_rearms(semu_test_context *context)
{
    semu_scheduler *scheduler;
    semu_bus *bus;
    int line = -1;

    g_alarm_count = 0u;
    bus = live_bus(&scheduler, &(semu_error){0});
    SEMU_TEST_ASSERT(context, bus != NULL);
    adv(context, scheduler, UINT64_C(11800000));
    wr(context, bus, 0x40004830u, 0u);
    wr(context, bus, 0x40004800u, 0xEu);
    wr(context, bus, 0x40004A08u, 1u);
    wr(context, bus, 0x40004A00u, 1u);
    adv(context, scheduler, UINT64_C(988200000) + 1u); /* fires at 1.0 */
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_count); /* rise */
    adv(context, scheduler, UINT64_C(30000));
    semu_scheduler_reset(scheduler); /* engine software reset flush */
    SEMU_TEST_EQ_U64(context, UINT64_C(1),
                     (uint64_t)semu_sapporo_rtc_probe_pending(&line));
    SEMU_TEST_EQ_U64(context, UINT64_C(1), (uint64_t)line);
    /* Boot two re-inits: the CTRL write sees the regressed clock,
     * drops the stale event, and RPT=0 leaves nothing scheduled. */
    wr(context, bus, 0x40004800u, 0u);
    SEMU_TEST_EQ_U64(context, UINT64_C(0),
                     (uint64_t)semu_sapporo_rtc_probe_pending(&line));
    SEMU_TEST_EQ_U64(context, UINT64_C(1), (uint64_t)line); /* still set */
    wr(context, bus, 0x40004A08u, 1u); /* handler-side clear */
    SEMU_TEST_EQ_U64(context, UINT64_C(2), g_alarm_count); /* fall */
    /* Re-arm at the regressed clock: the zero-time occurrence fires
     * the latched Status immediately (clock 0, alarm 0.00), the
     * census clear-before-enable order then keeps the line quiet until
     * the next whole-second occurrence at 2.000000000. */
    wr(context, bus, 0x40004800u, 0xEu);
    wr(context, bus, 0x40004830u, 0u);
    /* Re-arm on the uncommitted (epoch 0) clock: the alarm-0 occurrence
     * equals the current tick, so Limit == Value fires at once and the
     * retained enable raises the line right there; the next whole
     * second (clock 1.0 s) follows at the new scheduler time. */
    rd(context, bus, 0x40004A04u, 1u);
    SEMU_TEST_EQ_U64(context, UINT64_C(3), g_alarm_count); /* rise */
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_level[2]);
    wr(context, bus, 0x40004A08u, 1u);
    SEMU_TEST_EQ_U64(context, UINT64_C(4), g_alarm_count); /* fall */
    adv(context, scheduler, UINT64_C(1000000000) - 1u);
    SEMU_TEST_EQ_U64(context, UINT64_C(4), g_alarm_count);
    adv(context, scheduler, UINT64_C(1));
    SEMU_TEST_EQ_U64(context, UINT64_C(5), g_alarm_count); /* occurrence */
    SEMU_TEST_EQ_U64(context, UINT64_C(1), g_alarm_level[4]);
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
}

static void test_boot_recorded_stop(semu_test_context *context)
{
    const char *manifest_path = getenv("SEMU_SAPPORO_235_FIRMWARE_MANIFEST");
    FILE *probe;
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_machine *machine;
    semu_profile profile;
    semu_error error;

    if (manifest_path == NULL || manifest_path[0] == '\0') {
        manifest_path = "tests/private/sapporo-2.35.34.18929/firmware.semu";
    }
    probe = fopen(manifest_path, "rb");
    if (probe == NULL) {
        printf("SKIP sapporo 2.35.34 private manifest absent: %s\n",
               manifest_path);
        return;
    }
    fclose(probe);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_profile_load(
        "profiles/sapporo/2.35.34/profile.semu", &profile, &error));
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_manifest_load(manifest_path, &firmware, &error));
    memset(&options, 0, sizeof(options));
    options.profile = &profile;
    options.firmware = &firmware;
    semu_error_clear(&error);
    machine = semu_machine_create(&options, &error);
    SEMU_TEST_ASSERT(context, machine != NULL);
    {
        semu_run_limits limits = { UINT64_C(100000000),
                                   UINT64_C(30000000000) };
        semu_stop_reason reason;
        static const uint64_t instr[3] = { UINT64_C(89441522),
                                           UINT64_C(85778809),
                                           UINT64_C(85778817) };
        unsigned pass;
        /* E-SAP-0035: the RTC register-law module removes the W +0x20
         * refusal that drove the HardFault/AIRCR loop. Boot now wakes
         * at the 1 s alarm occurrences, runs through the old cycle
         * point, and parks in the WFI wait; the 100 M/30 s budget is
         * met by the time cap with the guest at the WFI park pc.
         * Pins are the byte-identical CLI pair and machine-API runs. */
        for (pass = 0u; pass < 3u; ++pass) {
            semu_error_clear(&error);
            if (pass != 0u) {
                SEMU_TEST_EQ_U64(context, SEMU_OK,
                                 semu_machine_reset(machine, &error));
            }
            semu_error_clear(&error);
            reason = semu_machine_run(machine, &limits, &error);
            SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_STOP_BUDGET,
                             (uint64_t)reason);
            SEMU_TEST_EQ_U64(context, UINT64_C(0x000e1862),
                             semu_machine_program_counter(machine));
            SEMU_TEST_EQ_U64(context, UINT64_C(30000000000),
                             semu_machine_virtual_time(machine));
            SEMU_TEST_EQ_U64(context, instr[pass],
                             semu_machine_instructions(machine));
        }
    }
    semu_machine_destroy(machine);
}

int main(void)
{
    static const semu_test_case cases[] = {
        { "test_stub_profiles_stay_inert", test_stub_profiles_stay_inert },
        { "test_lane_write_law", test_lane_write_law },
        { "test_access_refusals", test_access_refusals },
        { "test_counter_set_pair", test_counter_set_pair },
        { "test_second_alarm_law", test_second_alarm_law },
        { "test_reset_reinitialises", test_reset_reinitialises },
        { "test_software_reset_flush_rearms",
          test_software_reset_flush_rearms },
        { "test_boot_recorded_stop", test_boot_recorded_stop }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
