/*
 * Ulsan MSPI1 command-queue controller (ticket 730, E-ULS-0031/0033/0034).
 *
 * The lane repl wires `ulsan_mspi1: SPI.UlsanApollo4Mspi1 @ sysbus
 * 0x40061000` with `IRQ -> nvic@21`. Queue starts retire through the
 * stage-gated native response machine (ulsan_mspi1_populate.c /
 * ulsan_mspi1_identity.c / ulsan_mspi1_pio.c, ported from
 * SapporoApollo4Mspi1.cs); anything the lane refuses — or that faults a
 * bus probe — completes with no status mutation and no interrupt,
 * exactly the lane's refusal shape, so the guest can never observe an
 * unproven flash byte.
 *
 * Register semantics mirror the lane plugin's command base class:
 * INTEN readable, INTSTAT readable, INTCLR write-one-to-clear, INTSET
 * set-only, queue registers plain stores; all other offsets refuse
 * (the lane's generic store-through is per-offset untraced, so those
 * offsets stay unmodelled rather than guessed).
 */

#include "ulsan_mspi1.h"

#include <string.h>

#include "semu/bus.h"
#include "ulsan_mspi1_native.h"

#define ULSAN_MSPI1_BASE 0x40061000u
#define ULSAN_MSPI1_SIZE 0x1000u
#define ULSAN_MSPI1_IRQ 21u /* E-ULS-0031 (lane repl nvic@21) */

/* Lane command base: SRAM window + queue-head probe (lines 196-198);
 * the end matches the doorbell DMA bound proven in E-ULS-0029. */
#define MSPI1_SRAM_START 0x10000000u
#define MSPI1_SRAM_END 0x10267000u
#define MSPI1_QUEUE_PROBE_LENGTH 16u
#define MSPI1_QUEUE_START_MASK 0x13u

static ulsan_mspi1_state mspi1_instance;

static void mspi1_update_irq(ulsan_mspi1_state *s)
{
    unsigned level = (s->interrupt_status & s->interrupt_enable) != 0u;
    if (level == s->irq_level) {
        return;
    }
    s->irq_level = level;
    if (s->irq_sink != NULL) {
        s->irq_sink(s->irq_context, ULSAN_MSPI1_IRQ, level != 0u);
    }
}

static uint32_t mspi1_register_value(ulsan_mspi1_state *s, uint32_t offset)
{
    switch (offset) {
    case 0x0u:   return s->registers[4]; /* PIO control (read unobserved) */
    case 0x8u:   return s->registers[5];
    case 0xCu:   return s->pio_command;
    case 0x100u: return s->registers[0];
    case 0x108u: return s->registers[1];
    case 0x10Cu: return s->registers[2];
    case 0x110u: return s->registers[3];
    default:     return 0u;
    }
}

/* Lane CompleteObservedCommandQueue (base class lines 125-159): bounds
 * and the queue-count contract gate first, then the 16-byte head probe,
 * then the native response; only a populated response raises bit 6. */
static void mspi1_queue_start(ulsan_mspi1_state *s)
{
    uint32_t address = s->registers[1];
    uint32_t count = s->registers[3];
    uint8_t probe;
    unsigned index;

    if (address < MSPI1_SRAM_START ||
        address > MSPI1_SRAM_END - MSPI1_QUEUE_PROBE_LENGTH ||
        !ulsan_mspi1_native_count_allowed(s, s->registers[2], count)) {
        return; /* lane: Refusing ... outside the Race S contract */
    }
    for (index = 0u; index < MSPI1_QUEUE_PROBE_LENGTH; index++) {
        if (ulsan_mspi1_guest_read(s, address + index, &probe) != 0) {
            return;
        }
    }
    if (ulsan_mspi1_native_try_populate(s, s->registers[0], address,
                                        count) == 0) {
        return; /* lane: product state or payload not proven */
    }
    s->interrupt_status |= ULSAN_MSPI1_INT_QUEUE_COMPLETE;
    mspi1_update_irq(s);
}

/* Lane base default write case: plain store, then the product PIO
 * handler sees the same (offset, value). */
static void mspi1_plain_store(ulsan_mspi1_state *s, uint32_t offset,
                              uint32_t value)
{
    uint32_t raised;
    switch (offset) {
    case 0x0u:  s->registers[4] = value; break;
    case 0x8u:  s->registers[5] = value; break;
    case 0xCu:  s->pio_command = value; break;
    case 0x100u: s->registers[0] = value; break;
    case 0x108u: s->registers[1] = value; break;
    case 0x10Cu: s->registers[2] = value; break;
    case 0x110u: s->registers[3] = value; break;
    default: break;
    }
    raised = ulsan_mspi1_native_pio_write(s, offset, value);
    if (raised != 0u) {
        s->interrupt_status |= raised;
        mspi1_update_irq(s);
    }
}

static semu_status mspi1_read(void *context, uint32_t offset,
                              unsigned width, uint32_t *value,
                              semu_error *error)
{
    ulsan_mspi1_state *s = (ulsan_mspi1_state *)context;

    if (offset >= ULSAN_MSPI1_SIZE || width != 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "ulsan mspi1 read outside the observed plane");
        return SEMU_ERR_UNSUPPORTED;
    }
    switch (offset) {
    case 0x100u:
    case 0x108u:
    case 0x10Cu:
    case 0x110u:
    case 0x200u: /* INTEN */
    case 0x204u: /* INTSTAT */
        if (offset == 0x200u) {
            *value = s->interrupt_enable;
        } else if (offset == 0x204u) {
            *value = s->interrupt_status;
        } else {
            *value = mspi1_register_value(s, offset);
        }
        return SEMU_OK;
    default:
        break;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "ulsan mspi1 unobserved register read");
    return SEMU_ERR_UNSUPPORTED;
}

static semu_status mspi1_write(void *context, uint32_t offset,
                               unsigned width, uint32_t value,
                               semu_error *error)
{
    ulsan_mspi1_state *s = (ulsan_mspi1_state *)context;

    if (offset >= ULSAN_MSPI1_SIZE || width != 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "ulsan mspi1 write outside the observed plane");
        return SEMU_ERR_UNSUPPORTED;
    }
    switch (offset) {
    case 0x0u:
    case 0x8u:
    case 0xCu:
    case 0x100u:
    case 0x108u:
    case 0x10Cu:
    case 0x110u:
        mspi1_plain_store(s, offset, value);
        if (offset == 0x100u &&
            (value & MSPI1_QUEUE_START_MASK) == MSPI1_QUEUE_START_MASK) {
            mspi1_queue_start(s);
        }
        return SEMU_OK;
    case 0x200u:
        s->interrupt_enable = value;
        mspi1_update_irq(s);
        return SEMU_OK;
    case 0x204u:
        break; /* INTSTAT is not directly writable (INTCLR/INTSET only) */
    case 0x208u:
        s->interrupt_status &= ~value;
        mspi1_update_irq(s);
        return SEMU_OK;
    case 0x20Cu:
        s->interrupt_status |= value;
        mspi1_update_irq(s);
        return SEMU_OK;
    default:
        break;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "ulsan mspi1 unobserved register write");
    return SEMU_ERR_UNSUPPORTED;
}

static void mspi1_reset(void *context)
{
    ulsan_mspi1_state *s = (ulsan_mspi1_state *)context;
    semu_apollo4_irq_fn sink = s->irq_sink;
    void *sink_context = s->irq_context;
    semu_bus *bus = s->bus;
    unsigned prior_level = s->irq_level;
    uint8_t backing[ULSAN_MSPI1_BACKING_LENGTH];
    unsigned synthetic = s->synthetic_initialized;

    /* Emulated NOR is non-volatile across the emulated reset (lane
     * InitializeSyntheticPersistenceRange comment, line 1268). */
    memcpy(backing, s->backing, sizeof(backing));
    memset(s, 0, sizeof(*s));
    memcpy(s->backing, backing, sizeof(s->backing));
    s->synthetic_initialized = synthetic;
    s->bus = bus;
    s->irq_sink = sink;
    s->irq_context = sink_context;
    ulsan_mspi1_native_reset(s);
    if (prior_level != 0u && sink != NULL) {
        sink(sink_context, ULSAN_MSPI1_IRQ, 0); /* drop a raised line */
    }
}

void semu_ulsan_mspi1_set_irq_sink(semu_apollo4_irq_fn sink, void *context)
{
    mspi1_instance.irq_sink = sink;
    mspi1_instance.irq_context = context;
}

semu_status semu_ulsan_mspi1_map(semu_bus *bus, semu_error *error)
{
    static const semu_bus_device_ops mspi1_ops = { mspi1_read, mspi1_write,
                                                   mspi1_reset };

    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan MSPI1 needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    mspi1_reset(&mspi1_instance);
    mspi1_instance.bus = bus;
    return semu_bus_map_device(bus, "ulsan.mspi1", ULSAN_MSPI1_BASE,
                               ULSAN_MSPI1_SIZE, &mspi1_ops,
                               &mspi1_instance, error);
}
