/*
 * Ulsan 2.35.36 display-controller identity endpoint at 0x400A0000,
 * IRQ 29 (ticket 730, E-ULS-0039).
 *
 * Byte-exact port of the lane-local class Apollo4DisplayController
 * (emulator/renode/display/Apollo4DisplayController.cs, 118 lines,
 * registered by apollo4-display-controller-ulsan.repl at 0x400A0000
 * with IRQ -> nvic@29; lane log lp34b: "Added Apollo4DisplayController
 * @ <0x400A0000, 0x400A8FFF>" and nine "External IRQ 45" toggle pairs
 * = exception 45 = IRQ 29). The class stores every doubleword write
 * into a sparse dictionary (missing reads answer 0), serves +0xF4 =
 * 0x87452365 and +0xEC = 0x77 ahead of the dictionary, sets bit 4 of
 * the +0xF8 word and raises the line on any +0x00 (PLAY) write, drops
 * the line on a +0xF8 write without bit 4, and clears both dictionary
 * and line on Reset. AllowedTranslations ByteToDoubleWord|
 * WordToDoubleWord: the engine slices bytes/halfwords out of the
 * aligned doubleword read and merges byte/halfword writes through the
 * aligned doubleword write path (so a byte write into +0x00..+0x03
 * fires the PLAY effect exactly like the class).
 *
 * TraceWrites is off in the 2.35.36 profile (no ULSAN_DISPLAY lines in
 * lp34b), so the write stream is unlogged; the dictionary semantics
 * are the class's own and reproduce it. First observed transaction:
 * the epoch-1 guest word read at 0x400A8074 (DSI PHY window, PC
 * 0x00101ef0, instruction 37,491,594) answers 0 from the dictionary.
 */

#include "ulsan_disp.h"

#include "semu/types.h"
#include <stdlib.h>
#include <string.h>

#define DISP_BASE 0x400a0000u
#define DISP_SIZE (SEMU_ULSAN_DISP_WORDS * 4u)
#define DISP_HWID_OFFSET 0xF4u    /* class reads 0x87452365           */
#define DISP_PANEL_OFFSET 0xECu   /* class reads 0x00000077           */
#define DISP_PLAY_OFFSET 0x00u    /* PLAY: raises status bit 4 + IRQ  */
#define DISP_INT_OFFSET 0xF8u     /* frame status / line clear word   */
#define DISP_VSYNC_BIT 0x10u      /* 1u << 4, class VsyncInterruptBit */
#define ULSAN_DISP_IRQ 29u        /* lane repl nvic@29 (exc 45)       */

#define DISP_244_PANEL_SIZE 466u
#define DISP_244_RGB_STRIDE (DISP_244_PANEL_SIZE * 2u)
#define DISP_244_L4_STRIDE (DISP_244_PANEL_SIZE / 2u)
#define DISP_244_PLAY_LAYER 0x00120008u
#define DISP_244_L4_MODE 0x88ff0109u
#define DISP_244_SRAM_START 0x10000000u
#define DISP_244_SRAM_END 0x10267000u

struct ulsan_disp_scanout {
    semu_frame frame;
    uint8_t pixels[DISP_244_RGB_STRIDE * DISP_244_PANEL_SIZE];
    uint8_t packed[DISP_244_L4_STRIDE * DISP_244_PANEL_SIZE];
};

static semu_ulsan_disp disp_instance;

static int disp_244_word_allowed(uint32_t offset, int write)
{
    /* E-ULS-0075 records 20 read and 49 write offsets, all word-sized. */
    static const uint16_t reads[] = {
        0x0c, 0x14, 0x18, 0x1c, 0x28, 0x2c, 0x30, 0xec, 0xf0, 0xf4,
        0xf8, 0xfc, 0x8000, 0x805c, 0x806c, 0x8070, 0x8078, 0x807c,
        0x8080, 0x8084
    };
    static const uint16_t writes[] = {
        0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c, 0x24, 0x28,
        0x2c, 0x30, 0x34, 0x38, 0x3c, 0x40, 0x44, 0xd0, 0xd4, 0xd8,
        0xe8, 0xf8, 0x1a0, 0x1a8, 0x400, 0x444, 0x488, 0x4cc, 0x510,
        0x554, 0x598, 0x5dc, 0x8000, 0x800c, 0x8010, 0x8014, 0x8018,
        0x801c, 0x8044, 0x8050, 0x805c, 0x8068, 0x806c, 0x8070,
        0x8074, 0x8078, 0x807c, 0x8080, 0x8084
    };
    const uint16_t *offsets = write ? writes : reads;
    size_t count = write ? SEMU_ARRAY_LEN(writes) : SEMU_ARRAY_LEN(reads);
    size_t i;

    for (i = 0u; i < count; ++i) {
        if (offsets[i] == offset)
            return 1;
    }
    return 0;
}

static semu_status disp_validate(const semu_ulsan_disp *s, uint32_t offset,
                                 unsigned width, int write, semu_error *error)
{
    if (s->scanout != NULL) {
        if (width == 4u && disp_244_word_allowed(offset, write))
            return SEMU_OK;
    } else if ((width == 1u || width == 2u || width == 4u) &&
               (offset % width) == 0u && (offset & 3u) + width <= 4u) {
        return SEMU_OK;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan DISP %s at +0x%x width %u is unsupported",
                   write ? "write" : "read", offset, width);
    return SEMU_ERR_UNSUPPORTED;
}

/* The class's ReadDoubleWord for the aligned word containing offset. */
static uint32_t disp_doubleword(const semu_ulsan_disp *s, uint32_t aligned)
{
    if (aligned == DISP_HWID_OFFSET) {
        return 0x87452365u; /* UlsanExpectedHardwareId                */
    }
    if (aligned == DISP_PANEL_OFFSET) {
        return 0x00000077u; /* UlsanPanelReadyStatus                  */
    }
    return s->registers[aligned / 4u];
}

static void disp_set_line(semu_ulsan_disp *s, unsigned level)
{
    if (s->irq_sink == NULL || ((s->line_high != 0u) == (level != 0u))) {
        return; /* GPIO edge semantics: only transitions reach nvic   */
    }
    s->line_high = level != 0u ? 1u : 0u;
    s->irq_sink(s->irq_context, ULSAN_DISP_IRQ, s->line_high);
}

static uint16_t disp_rgb565(uint32_t rgb888)
{
    return (uint16_t)(((rgb888 >> 8u) & 0xf800u) |
                      ((rgb888 >> 5u) & 0x07e0u) |
                      ((rgb888 >> 3u) & 0x001fu));
}

/* E-ULS-0054 and E-ULS-0075. Copy the entire indexed source before
 * changing the visible image or acknowledging PLAY. */
static semu_status disp_244_scanout(semu_ulsan_disp *s, semu_error *error)
{
    struct ulsan_disp_scanout *scanout = s->scanout;
    const uint32_t mode = s->registers[0x30u / 4u];
    const uint32_t start = s->registers[0x34u / 4u];
    const uint32_t size = s->registers[0x38u / 4u];
    const uint32_t source = s->registers[0x3cu / 4u];
    const uint32_t stride = s->registers[0x40u / 4u];
    const uint32_t x0 = start >> 16u, y0 = start & 0xffffu;
    const uint32_t width = size >> 16u, height = size & 0xffffu;
    uint16_t palette[16];
    uint32_t i, x, y, packed_size;
    semu_status status;

    if (mode != DISP_244_L4_MODE || stride != DISP_244_L4_STRIDE ||
        width == 0u || height == 0u || x0 + width > DISP_244_PANEL_SIZE ||
        y0 + height > DISP_244_PANEL_SIZE) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan DISP unsupported L4 scanout configuration");
        return SEMU_ERR_UNSUPPORTED;
    }
    packed_size = stride * height;
    if (source < DISP_244_SRAM_START ||
        source > DISP_244_SRAM_END - packed_size ||
        scanout->frame.generation == UINT64_MAX) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "Ulsan DISP scanout range or generation overflow");
        return SEMU_ERR_RANGE;
    }
    status = semu_bus_copy_out(s->bus, source, scanout->packed, packed_size,
                               error);
    if (status != SEMU_OK)
        return status;

    for (i = 0u; i < SEMU_ARRAY_LEN(palette); ++i)
        palette[i] = disp_rgb565(s->registers[(0x400u + i * 0x44u) / 4u]);
    memset(scanout->pixels, 0, sizeof(scanout->pixels));
    for (y = 0u; y < height; ++y) {
        const uint8_t *row = scanout->packed + y * stride;
        uint8_t *out = scanout->pixels + (y0 + y) * DISP_244_RGB_STRIDE + x0 * 2u;
        for (x = 0u; x < width; ++x) {
            const unsigned index = (row[x / 2u] >> (4u * (x & 1u))) & 15u;
            out[x * 2u] = (uint8_t)palette[index];
            out[x * 2u + 1u] = (uint8_t)(palette[index] >> 8u);
        }
    }
    return SEMU_OK;
}

/* The class's WriteDoubleWord: store first, then the PLAY / line
 * clear side effects. Validate a 2.44.52 scanout before either. */
static semu_status disp_store(semu_ulsan_disp *s, uint32_t aligned,
                              uint32_t value, semu_error *error)
{
    int publish = 0;

    if (aligned == DISP_PLAY_OFFSET && s->scanout != NULL) {
        if (value == DISP_244_PLAY_LAYER) {
            semu_status status = disp_244_scanout(s, error);
            if (status != SEMU_OK)
                return status;
            publish = s->frame_callback != NULL;
        } else if (value != 0u && value != 0x00020008u) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "Ulsan DISP unsupported PLAY command 0x%08x", value);
            return SEMU_ERR_UNSUPPORTED;
        }
    }
    s->registers[aligned / 4u] = value;
    if (aligned == DISP_PLAY_OFFSET) {
        s->registers[DISP_INT_OFFSET / 4u] |= DISP_VSYNC_BIT;
        disp_set_line(s, 1u);
    } else if (aligned == DISP_INT_OFFSET && (value & DISP_VSYNC_BIT) == 0u) {
        disp_set_line(s, 0u);
    }
    if (publish) {
        ++s->scanout->frame.generation;
        s->frame_callback(s->frame_context, &s->scanout->frame);
    }
    return SEMU_OK;
}

static semu_status disp_read(void *context, uint32_t offset, unsigned width,
                             uint32_t *value, semu_error *error)
{
    semu_ulsan_disp *s = context;
    uint32_t word;

    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan DISP read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (disp_validate(s, offset, width, 0, error) != SEMU_OK) {
        return SEMU_ERR_UNSUPPORTED;
    }
    word = disp_doubleword(s, offset & ~3u);
    if (width == 4u) {
        *value = word;
        return SEMU_OK;
    }
    *value = (word >> ((offset & 3u) * 8u)) &
             (width == 1u ? 0xFFu : 0xFFFFu);
    return SEMU_OK;
}

static semu_status disp_write(void *context, uint32_t offset, unsigned width,
                              uint32_t value, semu_error *error)
{
    semu_ulsan_disp *s = context;
    uint32_t aligned = offset & ~3u;
    uint32_t merged;

    if (disp_validate(s, offset, width, 1, error) != SEMU_OK) {
        return SEMU_ERR_UNSUPPORTED;
    }
    if (width == 4u) {
        return disp_store(s, aligned, value, error);
    }
    /* Engine translation: merge into the doubleword the peripheral
     * currently reads (identity words included) and write it back
     * through the same path, effects and all. */
    merged = disp_doubleword(s, aligned);
    if (width == 1u) {
        merged = (merged & ~(0xFFu << ((offset & 3u) * 8u))) |
                 ((value & 0xFFu) << ((offset & 3u) * 8u));
    } else {
        merged = (merged & ~(0xFFFFu << ((offset & 3u) * 8u))) |
                 ((value & 0xFFFFu) << ((offset & 3u) * 8u));
    }
    return disp_store(s, aligned, merged, error);
}

static void disp_reset(void *context)
{
    semu_ulsan_disp *s = context;
    unsigned prior_level = s->line_high;

    memset(s->registers, 0, sizeof(s->registers));
    if (s->scanout != NULL) {
        memset(s->scanout->pixels, 0, sizeof(s->scanout->pixels));
        s->scanout->frame.generation = 0u;
    }
    s->line_high = 0u;
    if (prior_level != 0u && s->irq_sink != NULL)
        s->irq_sink(s->irq_context, ULSAN_DISP_IRQ, 0);
}

static const semu_bus_device_ops disp_ops = {
    disp_read,
    disp_write,
    disp_reset
};

semu_status semu_ulsan_244_disp_map(semu_bus *bus, semu_ulsan_disp *state,
                                    semu_apollo4_irq_fn irq, void *irq_context,
                                    semu_frame_callback frame,
                                    void *frame_context, semu_error *error)
{
    struct ulsan_disp_scanout *scanout;
    semu_status status;

    if (state == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Ulsan DISP needs state");
        return SEMU_ERR_ARGUMENT;
    }
    scanout = calloc(1u, sizeof(*scanout));
    if (scanout == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate Ulsan scanout");
        return SEMU_ERR_NOMEM;
    }
    status = semu_bus_map_device(bus, "ulsan.disp", DISP_BASE, DISP_SIZE,
                                 &disp_ops, state, error);
    if (status != SEMU_OK) {
        free(scanout);
        return status;
    }
    scanout->frame.format = SEMU_PIXEL_RGB565_LE;
    scanout->frame.width = DISP_244_PANEL_SIZE;
    scanout->frame.height = DISP_244_PANEL_SIZE;
    scanout->frame.stride = DISP_244_RGB_STRIDE;
    scanout->frame.pixels = scanout->pixels;
    scanout->frame.size = sizeof(scanout->pixels);
    memset(state, 0, sizeof(*state));
    state->bus = bus;
    state->irq_sink = irq;
    state->irq_context = irq_context;
    state->frame_callback = frame;
    state->frame_context = frame_context;
    state->scanout = scanout;
    return SEMU_OK;
}

void semu_ulsan_disp_destroy_state(semu_ulsan_disp *state)
{
    free(state->scanout);
    state->scanout = NULL;
}

void semu_ulsan_disp_set_irq_sink(semu_apollo4_irq_fn sink, void *context)
{
    disp_instance.irq_sink = sink;
    disp_instance.irq_context = context;
}

semu_status semu_ulsan_disp_map(semu_bus *bus, semu_error *error)
{
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Ulsan DISP needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    disp_reset(&disp_instance);
    return semu_bus_map_device(bus, "ulsan.disp", DISP_BASE, DISP_SIZE,
                               &disp_ops, &disp_instance, error);
}
