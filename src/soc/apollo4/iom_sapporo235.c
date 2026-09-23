#include "iom_internal.h"

/* E-SAP-0039: the lane has no LPS22 at either probed address. Its wrapper
 * drains the empty RX FIFO and completes DMA with FIFO-underflow status.
 * This is one exact negative identity probe, not an absent-device fallback. */
void semu_apollo4_iom_set_pressure235(semu_apollo4_iom *iom, int enabled)
{
    if (iom != NULL)
        iom->pressure235 = enabled != 0 && iom->irq == SEMU_APOLLO4_IOM2_IRQ;
}

semu_status semu_apollo4_iom_pressure235_command(semu_apollo4_iom *iom,
    uint32_t command, semu_error *error)
{
    semu_status status;
    if (iom == NULL || !iom->pressure235 ||
        (iom->device_config != 0x5cu && iom->device_config != 0x5du) ||
        command != UINT32_C(0x0f000112) || iom->dma_count != 1u ||
        iom->dma_config != 0x101u || !iom->endpoint_attached ||
        iom->dma_sink == NULL ||
        (iom->observed_registers[2u] & 0x1fu) != 0x10u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "2.35 pressure identity probe shape/state is unsupported");
        return SEMU_ERR_UNSUPPORTED;
    }
    /* Refuse holes, ROM and device overlays before touching any state. */
    status = semu_bus_validate_write(iom->bus, iom->dma_target, 1u, error);
    if (status != SEMU_OK) return status;
    status = semu_bus_write(iom->bus, iom->dma_target, 1u, 0u, error);
    if (status != SEMU_OK) return status;
    iom->dma_config &= ~UINT32_C(1);
    iom->dma_status = 2u;
    iom->dma_trig_stat |= 4u;
    /* Reuse the controller's ordinary IRQ propagation after the RAM commit. */
    return semu_apollo4_iom_write(iom, 0x20cu, 4u, 0x442u, error);
}
