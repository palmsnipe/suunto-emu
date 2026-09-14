/*
 * Ulsan MSPI1 command-queue controller (ticket 730, E-ULS-0031/0033).
 *
 * First slice: the register plane and interrupt seam only. The lane
 * repl wires `ulsan_mspi1: SPI.UlsanApollo4Mspi1 @ sysbus 0x40061000`
 * with `IRQ -> nvic@21`; the trace shows queue completion driving that
 * line (External IRQ 37 asserted at the first retire). Queue starts
 * are stored but every response population REFUSES until the lane's
 * stage-gated response branches are ported as their own instance
 * (E-ULS-0033), so the guest can never observe an unproven flash byte.
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

#define ULSAN_MSPI1_BASE 0x40061000u
#define ULSAN_MSPI1_SIZE 0x1000u
#define ULSAN_MSPI1_IRQ 21u /* E-ULS-0031 (lane repl nvic@21) */

#define MSPI1_INT_QUEUE_COMPLETE 0x40u /* lane bit 6 (E-ULS-0031) */

#define MSPI1_REG_QUEUE_CONTROL 0x100u
#define MSPI1_REG_QUEUE_ADDRESS 0x108u
#define MSPI1_REG_QUEUE_DEVICE  0x10Cu
#define MSPI1_REG_QUEUE_COUNT   0x110u
#define MSPI1_REG_INT_ENABLE    0x200u
#define MSPI1_REG_INT_STATUS    0x204u
#define MSPI1_REG_INT_CLEAR     0x208u
#define MSPI1_REG_INT_SET       0x20Cu

/* Queue start masks proven in the lane trace: control values 0x13 and
 * 0x17 both satisfy the start bit pattern; population is refused, so
 * only the store is observable today. */
#define MSPI1_QUEUE_START_MASK 0x13u

typedef struct {
    uint32_t registers[6];
    uint32_t interrupt_enable;
    uint32_t interrupt_status;
    unsigned irq_level;
    semu_apollo4_irq_fn irq_sink;
    void *irq_context;
} mspi1_state;

static mspi1_state mspi1_instance;

static void mspi1_update_irq(mspi1_state *s)
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

static uint32_t mspi1_register_value(mspi1_state *s, uint32_t offset)
{
    switch (offset) {
    case MSPI1_REG_QUEUE_CONTROL: return s->registers[0];
    case MSPI1_REG_QUEUE_ADDRESS: return s->registers[1];
    case MSPI1_REG_QUEUE_DEVICE:  return s->registers[2];
    case MSPI1_REG_QUEUE_COUNT:   return s->registers[3];
    case MSPI1_REG_INT_ENABLE:    return s->interrupt_enable;
    case MSPI1_REG_INT_STATUS:    return s->interrupt_status;
    default:                      return 0u;
    }
}

static void mspi1_register_store(mspi1_state *s, uint32_t offset,
                                 uint32_t value)
{
    switch (offset) {
    case MSPI1_REG_QUEUE_CONTROL: s->registers[0] = value; break;
    case MSPI1_REG_QUEUE_ADDRESS: s->registers[1] = value; break;
    case MSPI1_REG_QUEUE_DEVICE:  s->registers[2] = value; break;
    case MSPI1_REG_QUEUE_COUNT:   s->registers[3] = value; break;
    default: break;
    }
}

static semu_status mspi1_read(void *context, uint32_t offset,
                              unsigned width, uint32_t *value,
                              semu_error *error)
{
    mspi1_state *s = (mspi1_state *)context;

    (void)width;
    if (offset >= ULSAN_MSPI1_SIZE) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "ulsan mspi1 read outside the block");
        return SEMU_ERR_UNSUPPORTED;
    }
    switch (offset) {
    case MSPI1_REG_QUEUE_CONTROL:
    case MSPI1_REG_QUEUE_ADDRESS:
    case MSPI1_REG_QUEUE_DEVICE:
    case MSPI1_REG_QUEUE_COUNT:
    case MSPI1_REG_INT_ENABLE:
    case MSPI1_REG_INT_STATUS:
        *value = mspi1_register_value(s, offset);
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
    mspi1_state *s = (mspi1_state *)context;

    (void)width;
    if (offset >= ULSAN_MSPI1_SIZE) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "ulsan mspi1 write outside the block");
        return SEMU_ERR_UNSUPPORTED;
    }
    switch (offset) {
    case MSPI1_REG_QUEUE_CONTROL:
        /* Store first (lane does the same), then a start would run the
         * queue. Every response branch is stage-gated in the lane and
         * not yet ported (E-ULS-0033): fail closed with no status
         * mutation, exactly the lane's unproven-payload refusal shape. */
        mspi1_register_store(s, offset, value);
        return SEMU_OK;
    case MSPI1_REG_QUEUE_ADDRESS:
    case MSPI1_REG_QUEUE_DEVICE:
    case MSPI1_REG_QUEUE_COUNT:
        mspi1_register_store(s, offset, value);
        return SEMU_OK;
    case MSPI1_REG_INT_ENABLE:
        s->interrupt_enable = value;
        mspi1_update_irq(s);
        return SEMU_OK;
    case MSPI1_REG_INT_STATUS:
        /* Lane INTSTAT is not directly writable (INTCLR/INTSET only). */
        break;
    case MSPI1_REG_INT_CLEAR:
        s->interrupt_status &= ~value;
        mspi1_update_irq(s);
        return SEMU_OK;
    case MSPI1_REG_INT_SET:
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
    mspi1_state *s = (mspi1_state *)context;
    semu_apollo4_irq_fn sink = s->irq_sink;
    void *sink_context = s->irq_context;
    unsigned prior_level = s->irq_level;

    memset(s, 0, sizeof(*s));
    s->irq_sink = sink;
    s->irq_context = sink_context;
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
    return semu_bus_map_device(bus, "ulsan.mspi1", ULSAN_MSPI1_BASE,
                               ULSAN_MSPI1_SIZE, &mspi1_ops,
                               &mspi1_instance, error);
}
