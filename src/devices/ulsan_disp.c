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
#include <string.h>

#define DISP_BASE 0x400a0000u
#define DISP_SIZE 0x9000u /* class Size, DSI PHY window included     */
#define DISP_WORDS (DISP_SIZE / 4u)
#define DISP_HWID_OFFSET 0xF4u    /* class reads 0x87452365           */
#define DISP_PANEL_OFFSET 0xECu   /* class reads 0x00000077           */
#define DISP_PLAY_OFFSET 0x00u    /* PLAY: raises status bit 4 + IRQ  */
#define DISP_INT_OFFSET 0xF8u     /* frame status / line clear word   */
#define DISP_VSYNC_BIT 0x10u      /* 1u << 4, class VsyncInterruptBit */
#define ULSAN_DISP_IRQ 29u        /* lane repl nvic@29 (exc 45)       */

typedef struct {
    uint32_t registers[DISP_WORDS]; /* store-through dictionary       */
    semu_apollo4_irq_fn irq_sink;
    void *irq_context;
    unsigned line_high;
} disp_state;

static disp_state disp_instance;

static semu_status disp_validate(uint32_t offset, unsigned width,
                                 const char *kind, semu_error *error)
{
    if ((width != 1u && width != 2u && width != 4u) ||
        (offset % width) != 0u || (offset & 3u) + width > 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan DISP %s at +0x%x width %u is unsupported",
                       kind, offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    return SEMU_OK;
}

/* The class's ReadDoubleWord for the aligned word containing offset. */
static uint32_t disp_doubleword(disp_state *s, uint32_t aligned)
{
    if (aligned == DISP_HWID_OFFSET) {
        return 0x87452365u; /* UlsanExpectedHardwareId                */
    }
    if (aligned == DISP_PANEL_OFFSET) {
        return 0x00000077u; /* UlsanPanelReadyStatus                  */
    }
    return s->registers[aligned / 4u];
}

static void disp_set_line(disp_state *s, unsigned level)
{
    if (s->irq_sink == NULL || ((s->line_high != 0u) == (level != 0u))) {
        return; /* GPIO edge semantics: only transitions reach nvic   */
    }
    s->line_high = level != 0u ? 1u : 0u;
    s->irq_sink(s->irq_context, ULSAN_DISP_IRQ, s->line_high);
}

/* The class's WriteDoubleWord: store first, then the PLAY / line
 * clear side effects. */
static void disp_store(disp_state *s, uint32_t aligned, uint32_t value)
{
    s->registers[aligned / 4u] = value;
    if (aligned == DISP_PLAY_OFFSET) {
        s->registers[DISP_INT_OFFSET / 4u] |= DISP_VSYNC_BIT;
        disp_set_line(s, 1u);
    } else if (aligned == DISP_INT_OFFSET && (value & DISP_VSYNC_BIT) == 0u) {
        disp_set_line(s, 0u);
    }
}

static semu_status disp_read(void *context, uint32_t offset, unsigned width,
                             uint32_t *value, semu_error *error)
{
    disp_state *s = (disp_state *)context;
    uint32_t word;

    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan DISP read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (disp_validate(offset, width, "read", error) != SEMU_OK) {
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
    disp_state *s = (disp_state *)context;
    uint32_t aligned = offset & ~3u;
    uint32_t merged;

    if (disp_validate(offset, width, "write", error) != SEMU_OK) {
        return SEMU_ERR_UNSUPPORTED;
    }
    if (width == 4u) {
        disp_store(s, aligned, value); /* +0xF4/+0xEC still store; the
                                          * class ignores its own
                                          * identity words on reads    */
        return SEMU_OK;
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
    disp_store(s, aligned, merged);
    return SEMU_OK;
}

static void disp_reset(void *context)
{
    disp_state *s = (disp_state *)context;
    semu_apollo4_irq_fn sink = s->irq_sink;
    void *sink_context = s->irq_context;
    unsigned prior_level = s->line_high;

    memset(s, 0, sizeof(*s));
    s->irq_sink = sink;
    s->irq_context = sink_context;
    if (prior_level != 0u && sink != NULL) {
        sink(sink_context, ULSAN_DISP_IRQ, 0); /* Reset: IRQ.Unset     */
    }
}

static const semu_bus_device_ops disp_ops = {
    disp_read,
    disp_write,
    disp_reset
};

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
