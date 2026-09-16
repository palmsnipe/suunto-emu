/*
 * Ulsan 2.35.36 SystemTimer registers at 0x40008800 (ticket 730,
 * E-ULS-0014 as superseded in part by E-ULS-0045/E-ULS-0046).
 * See ulsan_stimer.h for the full evidence summary.
 */

#include "ulsan_stimer.h"

#include "semu/types.h"
#include "semu/scheduler.h"

#define STIMER_BASE        0x40008800u
#define STIMER_SIZE        0x200u
/* +0x00 is CONFIG (lane upstream AmbiqApollo4_SystemTimer
 * Registers.Configuration), NOT a load register: a plain store-backed
 * 32-bit word whose reset value is 0x80000000 (FREEZE bit 31 set,
 * CLKSEL = NOCLK). E-ULS-0045 lane reset samples read 0x80000000 at
 * virtual time 0 before any guest write (lp48), and lp47
 * shows the 0x80000000 write by boot persists unchanged until the
 * guest's later absolute 0x303 write - the E-ULS-0014 "bit 31 is not
 * stored" mask was a misread of that final-state read-back. */
#define STIMER_CONFIG      0x000u
#define STIMER_CONFIG_FREEZE UINT32_C(0x80000000)
#define STIMER_CONFIG_CLEAR  UINT32_C(0x40000000)
#define STIMER_CONFIG_CLKSEL_MASK UINT32_C(0x0000000F)
#define STIMER_COUNT       0x004u
#define STIMER_CONTROL     0x100u
/* STIMER NVRAM words at +0x50..+0x5c (E-ULS-0020 pinned reads of 0;
 * E-ULS-0038 identified the plane): the lane wrapper
 * Apollo4RetainedSystemTimer intercepts every read and write of these
 * four words ahead of the upstream SystemTimer (Nvram0Offset 0x50 ..
 * Nvram3Offset 0x5c), backed by its own uint[4], and its Reset()
 * deliberately keeps them - "Apollo4's four STIMER NVRAM words survive
 * a software reset. The firmware uses them to carry its next startup
 * mode across AIRCR." */
#define STIMER_NVRAM0 0x050u
#define STIMER_NVRAM1 0x054u
#define STIMER_NVRAM2 0x058u
#define STIMER_NVRAM3 0x05cu

/* Lane CLKSEL frequency table from the upstream AmbiqApollo4_SystemTimer
 * class read during the E-ULS-0046 lane audit (/tmp report; lane
 * renode-infrastructure Timers/AmbiqApollo4_SystemTimer.cs, matching the
 * 1.16.1 binary and its runtime trace): 1 = 6 MHz, 2 = 375 kHz,
 * 3 = exactly 32000 Hz (the lane's modeling of XTAL_32KHZ; the lane is
 * the oracle, so 32000 - not silicon 32768 - is the engine law),
 * 4 = 16 kHz, 5/6 = 1 kHz. 0 = NOCLK; any other index is
 * "Unsupported/Invalid CLKSEL value" upstream, leaving the timer not
 * enabled. Counter tick duration is derived as
 * ticks = floor(elapsed_ns * freq_hz / 1e9), checked for overflow. */
#define STIMER_CLKSEL_MAX 6u
static const uint64_t stimer_clksel_hz[STIMER_CLKSEL_MAX + 1u] = {
    0u,          /* 0: NOCLK */
    UINT64_C(6000000),
    UINT64_C(375000),
    UINT64_C(32000),
    UINT64_C(16000),
    UINT64_C(1000),
    UINT64_C(1000)
};
#define STIMER_TICK_NS_SCALE UINT64_C(1000000000)

typedef struct {
    uint32_t nvram[4];
    uint32_t config;
    uint32_t control;
    uint64_t base_ticks;    /* value captured at the last CONFIG capture */
    uint64_t anchor_ns;     /* virtual time of that capture (or attach) */
    int enabled;
    semu_scheduler *scheduler;
} stimer_state;

static stimer_state stimer_instance;

static uint64_t stimer_clksel_hz_value(uint32_t config)
{
    uint32_t clksel = config & STIMER_CONFIG_CLKSEL_MASK;
    if (clksel > STIMER_CLKSEL_MAX) {
        return 0u; /* invalid CLKSEL leaves the upstream timer not enabled */
    }
    return stimer_clksel_hz[clksel];
}

static int stimer_config_enabled(uint32_t config)
{
    /* Upstream: Enabled = !FREEZE && !CLEAR && Frequency != 1. */
    return (config & STIMER_CONFIG_FREEZE) == 0u &&
           (config & STIMER_CONFIG_CLEAR) == 0u &&
           stimer_clksel_hz_value(config) != 0u;
}

/* floor(elapsed_ns * freq_hz / 1e9) with checked multiplication. */
static int stimer_elapsed_ticks(uint64_t elapsed_ns, uint64_t freq_hz,
                                uint64_t *out)
{
    if (freq_hz != 0u && elapsed_ns > UINT64_MAX / freq_hz) {
        return 0;
    }
    *out = (elapsed_ns * freq_hz) / STIMER_TICK_NS_SCALE;
    return 1;
}

/* Live STTMR value: base_ticks plus the ticks accrued since the anchor.
 * Upstream LimitTimer derives Value from the clock source against a
 * fixed origin; reading it never re-anchors. A per-read re-anchor (the
 * first E-ULS-0046 cut) truncated each sub-tick remainder, and the
 * 2.35.36 wake loop reads the counter every ~170 ns of virtual time, so
 * the frozen-remainder fold pinned STTMR at 184 forever (found by the
 * E-ULS-0046 in-era probe). Reads therefore only consult state. */
static int stimer_held(const stimer_state *state, uint64_t now_ns,
                       uint64_t *out)
{
    uint64_t ticks = 0u;

    if (!state->enabled) {
        /* Held (not zeroed, not advancing) while the upstream timer is
         * not Enabled, matching the lane reads-0-while-gated evidence:
         * the wake loop never re-enables, so a held 0 answers 0. */
        *out = state->base_ticks;
        return 1;
    }
    if (now_ns < state->anchor_ns) {
        return 0;
    }
    if (!stimer_elapsed_ticks(now_ns - state->anchor_ns,
                              stimer_clksel_hz_value(state->config),
                              &ticks)) {
        return 0;
    }
    if (UINT64_MAX - state->base_ticks < ticks) {
        return 0;
    }
    *out = state->base_ticks + ticks;
    return 1;
}

/* Fold the live value into base_ticks and re-anchor. Only CONFIG
 * transitions (the upstream Enabled gate) and attach capture; reads
 * must not call this. */
static semu_status stimer_capture(stimer_state *state, semu_error *error)
{
    uint64_t now_ns = semu_scheduler_now(state->scheduler);
    uint64_t held = 0u;

    if (!stimer_held(state, now_ns, &held)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan SystemTimer virtual time went backwards or "
                       "tick computation overflowed");
        return SEMU_ERR_UNSUPPORTED;
    }
    state->base_ticks = held;
    state->anchor_ns = now_ns;
    return SEMU_OK;
}

static semu_status stimer_read(void *context, uint32_t offset,
                               unsigned width, uint32_t *value,
                               semu_error *error)
{
    stimer_state *state = (stimer_state *)context;
    uint64_t held = 0u;

    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan SystemTimer read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan SystemTimer read at 0x%08x width %u is "
                       "unsupported", offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    switch (offset) {
    case STIMER_CONFIG:
        *value = state->config;
        return SEMU_OK;
    case STIMER_COUNT:
        /* STTMR is a live counter (E-ULS-0045/0046): virtual-time
         * derived at the CLKSEL rate while enabled, held while gated,
         * truncated to 32 bits like the upstream (uint)Value cast. */
        if (state->scheduler == NULL) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "Ulsan SystemTimer counter needs a scheduler");
            return SEMU_ERR_UNSUPPORTED;
        }
        if (!stimer_held(state, semu_scheduler_now(state->scheduler),
                         &held)) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "Ulsan SystemTimer virtual time went backwards "
                           "or tick computation overflowed");
            return SEMU_ERR_UNSUPPORTED;
        }
        *value = (uint32_t)held;
        return SEMU_OK;
    case STIMER_CONTROL:
        *value = state->control;
        return SEMU_OK;
    case STIMER_NVRAM0:
    case STIMER_NVRAM1:
    case STIMER_NVRAM2:
    case STIMER_NVRAM3:
        *value = state->nvram[(offset - STIMER_NVRAM0) / 4u];
        return SEMU_OK;
    default:
        break;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan SystemTimer read at 0x%08x is unsupported",
                   offset);
    return SEMU_ERR_UNSUPPORTED;
}

static semu_status stimer_write(void *context, uint32_t offset,
                                unsigned width, uint32_t value,
                                semu_error *error)
{
    stimer_state *state = (stimer_state *)context;

    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan SystemTimer write at 0x%08x width %u is "
                       "unsupported", offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    switch (offset) {
    case STIMER_CONFIG:
        /* A plain store; the gate transition re-anchors the clock at
         * the current virtual time (upstream LimitTimer toggles Enabled
         * on this write). Needs the scheduler to timestamp the edge. */
        if (state->scheduler == NULL) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "Ulsan SystemTimer CONFIG needs a scheduler");
            return SEMU_ERR_UNSUPPORTED;
        }
        if (stimer_capture(state, error) != SEMU_OK) {
            return SEMU_ERR_UNSUPPORTED;
        }
        state->config = value;
        state->enabled = stimer_config_enabled(value);
        return SEMU_OK;
    case STIMER_CONTROL:
        state->control = value;
        return SEMU_OK;
    case STIMER_NVRAM0:
    case STIMER_NVRAM1:
    case STIMER_NVRAM2:
    case STIMER_NVRAM3:
        state->nvram[(offset - STIMER_NVRAM0) / 4u] = value;
        return SEMU_OK;
    default:
        break;
    }
    /* STTMR (+0x04) is read-only upstream; the lane drops guest writes
     * there. No 2.35.36-era epoch ever wrote it (E-ULS-0045 census:
     * 663111 reads, zero writes), so the tree keeps refusing. */
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan SystemTimer write at 0x%08x is unsupported",
                   offset);
    return SEMU_ERR_UNSUPPORTED;
}

static void stimer_reset(void *context)
{
    stimer_state *state = (stimer_state *)context;

    state->config = STIMER_CONFIG_FREEZE;
    state->control = 0u;
    state->base_ticks = 0u;
    state->anchor_ns = 0u;
    state->enabled = 0;
    /* NVRAM words are deliberately not cleared: the lane wrapper's
     * Reset() keeps them (SapporoApollo4Extensions.cs line 136) so the
     * firmware carries its next startup mode across AIRCR. The static
     * instance is already zero on a fresh process map. */
}

static const semu_bus_device_ops stimer_ops = {
    stimer_read,
    stimer_write,
    stimer_reset
};

semu_status semu_ulsan_stimer_map(semu_bus *bus, semu_error *error)
{
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan SystemTimer needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    /* Fresh machine: NVRAM starts at power-on zero (lane power-on has an
     * empty wrapper array); in-machine resets keep it via stimer_reset. */
    stimer_instance.nvram[0] = 0u;
    stimer_instance.nvram[1] = 0u;
    stimer_instance.nvram[2] = 0u;
    stimer_instance.nvram[3] = 0u;
    stimer_instance.scheduler = NULL;
    stimer_reset(&stimer_instance);
    return semu_bus_map_device(bus, "ulsan.stimer", STIMER_BASE, STIMER_SIZE,
                               &stimer_ops, &stimer_instance, error);
}

void semu_ulsan_stimer_attach(semu_scheduler *scheduler)
{
    stimer_instance.scheduler = scheduler;
    /* Re-anchor live counting to the current virtual time so a re-attach
     * on a rebuilt machine never credits pre-attach time. */
    stimer_instance.anchor_ns = (scheduler != NULL)
        ? semu_scheduler_now(scheduler) : 0u;
}
