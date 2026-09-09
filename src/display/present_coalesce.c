#include "present_coalesce.h"
#include "semu/types.h"
#include <stdlib.h>
#include <string.h>

#define COALESCE_MAX_FRAME_BYTES (4096u * 4096u * 2u)
#define COALESCE_SLOTS 2u

struct semu_present_coalescer {
    uint64_t threshold_ns;
    int have;
    unsigned hold;
    uint64_t held_time_ns;
    semu_frame frames[COALESCE_SLOTS];
    uint8_t *pixels[COALESCE_SLOTS];
    size_t capacity[COALESCE_SLOTS];
};

semu_present_coalescer *semu_present_coalescer_create(uint64_t threshold_ns,
                                                      semu_error *error)
{
    semu_present_coalescer *co;
    (void)error;
    co = (semu_present_coalescer *)calloc(1u, sizeof(*co));
    if (co == NULL) {
        if (error != NULL) {
            semu_error_set(error, SEMU_ERR_NOMEM, "coalescer: out of memory");
        }
        return NULL;
    }
    co->threshold_ns = threshold_ns;
    return co;
}

void semu_present_coalescer_destroy(semu_present_coalescer *co)
{
    unsigned i;
    if (co == NULL) return;
    for (i = 0u; i < COALESCE_SLOTS; ++i) free(co->pixels[i]);
    free(co);
}

static int frame_valid(const semu_frame *frame)
{
    size_t need;
    if (frame == NULL || frame->pixels == NULL ||
        frame->format != SEMU_PIXEL_RGB565_LE ||
        frame->width == 0u || frame->height == 0u ||
        frame->stride < frame->width * 2u) {
        return 0;
    }
    if ((size_t)frame->stride > SIZE_MAX / (size_t)frame->height) return 0;
    need = (size_t)frame->stride * (size_t)frame->height;
    return frame->size >= need && frame->size <= COALESCE_MAX_FRAME_BYTES;
}

/* Copies frame into slot; returns 0 on success. On failure the slot and the
 * coalescer state are untouched. */
static int hold(semu_present_coalescer *co, const semu_frame *frame,
    uint64_t virtual_time_ns, unsigned slot, semu_error *error)
{
    uint8_t *grown;
    if (frame->size > co->capacity[slot]) {
        grown = (uint8_t *)realloc(co->pixels[slot], frame->size);
        if (grown == NULL) {
            if (error != NULL) {
                semu_error_set(error, SEMU_ERR_NOMEM,
                    "coalescer: cannot grow hold buffer");
            }
            return -1;
        }
        co->pixels[slot] = grown;
        co->capacity[slot] = frame->size;
    }
    memcpy(co->pixels[slot], frame->pixels, frame->size);
    co->frames[slot] = *frame;
    co->frames[slot].pixels = co->pixels[slot];
    co->held_time_ns = virtual_time_ns;
    co->hold = slot;
    co->have = 1;
    return 0;
}

const semu_frame *semu_present_coalescer_observe(
    semu_present_coalescer *co, const semu_frame *frame,
    uint64_t virtual_time_ns, semu_error *error)
{
    int matured = 0;
    const semu_frame *released = NULL;
    uint64_t gap;
    if (error != NULL) semu_error_clear(error);
    if (co == NULL) {
        if (error != NULL) {
            semu_error_set(error, SEMU_ERR_ARGUMENT, "coalescer: null");
        }
        return NULL;
    }
    if (!frame_valid(frame)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
            "coalescer: invalid frame refused");
        return NULL;
    }
    if (co->have) {
        if (virtual_time_ns >= co->held_time_ns) {
            gap = virtual_time_ns - co->held_time_ns;
        } else {
            gap = co->threshold_ns; /* time discontinuity ends the burst */
        }
        matured = gap >= co->threshold_ns;
        if (matured) released = &co->frames[co->hold];
    }
    /* On release the new frame takes the other slot, keeping the released
     * borrow alive until this contract's next call; in-burst replacement
     * overwrites the current hold in place. */
    if (hold(co, frame, virtual_time_ns, matured ? (co->hold + 1u) % COALESCE_SLOTS : co->hold,
            error) != 0) {
        return released; /* NOMEM: keep any matured frame, state unchanged */
    }
    return released;
}

const semu_frame *semu_present_coalescer_flush(semu_present_coalescer *co)
{
    if (co == NULL || !co->have) return NULL;
    co->have = 0;
    return &co->frames[co->hold];
}
