#ifndef SEMU_DEVICES_ULSAN_IOM4_H
#define SEMU_DEVICES_ULSAN_IOM4_H

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "ulsan_iom4_endpoints.h"

typedef struct semu_ulsan_iom4 {
    uint32_t fifo_threshold;   /* +0x104: FIFORTHR[5:0], FIFOWTHR[13:8] */
    uint32_t io_clock;         /* +0x118: reserved bits drop, rest stores */
    uint32_t submodule;        /* +0x11C: SMOD0EN bit 0, SMOD1EN bit 4   */
    uint32_t interrupts_enable; /* +0x200: INTENi[14:0]                  */
    uint32_t dma_trigger;      /* +0x210: store & 0x3 (wrapper masked)   */
    uint32_t i2c_config;       /* +0x2C0: 0x0000F373 field mask          */
    uint32_t module_err;       /* +0x248 bit 0 ERR (tagged flag stores)  */
    uint32_t offset_high;      /* +0x128: OFFSETHI full 32-bit value     */
    uint32_t dcx_control;      /* +0x124: DCXEN bit 4 (tag DCXSEL no)   */
    uint32_t command;          /* +0x120: CMD|OFFSETCNT|CONT|TSIZE|
                                *         CMDSEL|OFFSETLO (no [22,24))  */
    uint32_t i2c_device;       /* +0x2C4: DEVADDR[6:0] (7-bit truncated) */
    uint32_t dma_config;       /* +0x218: DMAEN|DMADIR|DMAPRI|DPWROFF 0x303 */
    uint32_t dma_total;        /* +0x21C: TOTCOUNT[11:0]                */
    uint32_t dma_target;       /* +0x220: TARGADDR[28:0]                */
    uint32_t dma_status;       /* +0x224 mirror: never read in the
                                * observed window (refused)           */
    uint32_t interrupt_status; /* +0x204: read-only INTSTAT            */
    semu_bus *bus;             /* self-reference for DMA payloads     */
    semu_apollo4_irq_fn irq_sink;   /* machine line sink (IRQ 10)   */
    void *irq_context;              /* sink context                 */
    unsigned irq_level;             /* last driven line level       */
    ulsan_iom4_endpoints endpoints;
    int race_s_244;
    int actuator_enabled;           /* enabled by the Race S full UI boot */
} semu_ulsan_iom4;

semu_status semu_ulsan_244_iom4_map(semu_bus *bus, semu_ulsan_iom4 *state,
                                    semu_apollo4_irq_fn sink, void *context,
                                    semu_error *error);

/* Map the Ulsan 2.35.36 IOM4 block (0x40054000, 4 KiB window). See the
 * source for the observed register and doorbell data-plane semantics
 * (ticket 730, E-ULS-0023, E-ULS-0025, E-ULS-0029). */
semu_status semu_ulsan_iom4_map(semu_bus *bus, semu_error *error);

/* Attach the machine IRQ sink for the IOM4 line (NVIC IRQ 10,
 * E-ULS-0028). The asserted level is (INTSTAT & INTEN) != 0
 * (E-ULS-0030). Until this is called the line is inert and the block
 * behaves exactly as before the interrupt plane existed. */
void semu_ulsan_iom4_set_irq_sink(semu_apollo4_irq_fn sink, void *context);

#endif /* SEMU_DEVICES_ULSAN_IOM4_H */
