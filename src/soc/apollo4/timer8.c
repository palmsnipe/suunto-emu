#include "timer_internal.h"

/* E-EMU-SAP235-TIMER8-003: exact lane CPU-visible clock, not PWM output.
 * The lane drops fractional progress at compare0 and UINT32_MAX boundaries.
 * Express 6 MHz as 3/500 tick per ns; no host floating point or time. */
void semu_apollo4_timer8_sync(timer_channel *channel, uint64_t now)
{
    uint64_t elapsed;
    if (now < channel->epoch) return;
    elapsed = now - channel->epoch;
    channel->epoch = now;
    if (channel->control != 0x141u || channel->limit_stalled != 0u) return;
    while (elapsed != 0u) {
        uint32_t compare = channel->compare[0];
        uint32_t boundary = compare > channel->base_value ? compare : UINT32_MAX;
        uint64_t delta, delay;
        if (channel->base_value == 0u && channel->phase == 0u &&
            compare != 0u && compare != UINT32_MAX) {
            /* A whole cycle has two independently rounded intervals. */
            uint64_t cycle = ((uint64_t)compare * 500u + 2u) / 3u +
                ((uint64_t)(UINT32_MAX - compare) * 500u + 2u) / 3u;
            elapsed %= cycle;
            if (elapsed == 0u) break;
        }
        delta = (uint64_t)(boundary - channel->base_value) * 500u;
        delay = delta > channel->phase ? (delta - channel->phase + 2u) / 3u : 0u;
        if (elapsed < delay) {
            /* elapsed is bounded by at most UINT32_MAX * 500 / 3. */
            uint64_t ticks = elapsed * 3u + channel->phase;
            channel->base_value += (uint32_t)(ticks / 500u);
            channel->phase = (uint32_t)(ticks % 500u);
            break;
        }
        elapsed -= delay;
        channel->phase = 0u;
        channel->base_value = boundary;
        if (boundary == UINT32_MAX) {
            channel->base_value = 0u;
            if (compare == 0u || compare == UINT32_MAX) {
                channel->limit_stalled = 1u;
                break;
            }
        }
    }
}
