#include "ulsan_iom2.h"

/* E-ULS-0083. The pressure sensor exposes its native identification;
 * the companion devices implement the exact transactions in the lane. */
static int sensor_write(semu_ulsan_iom2 *state,
                        const semu_serial_transaction *transaction)
{
    const uint8_t *p = transaction->tx;

    if (transaction->tx_size != 2u || p == NULL)
        return 0;
    if (transaction->address == 0x49u) {
        return (p[0] == 0x11u && p[1] == 0x80u) ||
               (p[0] == 0x0eu && p[1] == 0x03u) ||
               (p[0] == 0x0fu && p[1] == 0xa5u) ||
               (p[0] == 0x13u && p[1] == 0x3cu);
    }
    if (transaction->address == 0x35u && p[0] == 0x11u && p[1] == 0x9du) {
        state->aux_high = p[1];
        state->aux_ready |= 1u;
        return 1;
    }
    if (transaction->address == 0x35u && p[0] == 0x10u && p[1] == 0xa8u) {
        state->aux_low = p[1];
        state->aux_ready |= 2u;
        return 1;
    }
    return 0;
}

static int sensor_read(semu_ulsan_iom2 *state, semu_serial_transaction *transaction)
{
    unsigned address = transaction->address;

    if (transaction->rx == NULL)
        return 0;
    if (address == 0x35u && transaction->rx_size == 23u &&
        transaction->tx_size == 0u && state->aux_ready == 3u) {
        const uint8_t word[] = {state->aux_low, state->aux_high, 0u, 0u};
        size_t i;
        for (i = 0u; i < transaction->rx_size; ++i)
            transaction->rx[i] = word[i % sizeof(word)];
        return 1;
    }
    if (transaction->rx_size != 1u || transaction->tx_size != 1u ||
        transaction->tx == NULL)
        return 0;
    if ((address == 0x48u || address == 0x4au) && transaction->tx[0] == 0u) {
        transaction->rx[0] = 0u;
        return 1;
    }
    if (address == 0x49u && (transaction->tx[0] == 0u ||
                             transaction->tx[0] == 0x1cu)) {
        transaction->rx[0] = transaction->tx[0] == 0u ? 0x49u : 0x41u;
        return 1;
    }
    return 0;
}

static semu_transaction_result sensor_transfer(void *context,
    semu_serial_transaction *transaction, semu_error *error)
{
    semu_ulsan_iom2 *state = context;
    int accepted = transaction->rx_size != 0u
        ? sensor_read(state, transaction) : sensor_write(state, transaction);

    if (!accepted) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Race S IOM2 transaction refused for 0x%02x (tx=%zu rx=%zu)",
                       (unsigned)transaction->address, transaction->tx_size,
                       transaction->rx_size);
        return SEMU_TRANSACTION_REFUSE;
    }
    return SEMU_TRANSACTION_OK;
}

semu_status semu_ulsan_244_iom2_map(semu_bus *bus, semu_ulsan_iom2 *state,
    semu_scheduler *scheduler, semu_apollo4_iom_irq_fn irq, void *context,
    semu_error *error)
{
    state->dma = semu_apollo4_dma_create(bus, scheduler, error);
    if (state->dma == NULL)
        return error != NULL ? error->code : SEMU_ERR_STATE;
    state->controller = semu_apollo4_iom_create(bus, SEMU_APOLLO4_IOM2_BASE,
        SEMU_APOLLO4_IOM2_IRQ, irq, context, semu_apollo4_dma_execute,
        state->dma, scheduler, error);
    if (state->controller == NULL)
        return error != NULL ? error->code : SEMU_ERR_STATE;
    semu_apollo4_iom_select_race_s(state->controller);
    state->endpoint = (semu_serial_endpoint){"ulsan.iom2", sensor_transfer, state};
    return semu_apollo4_iom_attach_endpoint(state->controller, &state->endpoint, error);
}

void semu_ulsan_244_iom2_reset(semu_ulsan_iom2 *state)
{
    semu_apollo4_iom_reset(state->controller);
    state->aux_low = state->aux_high = 0u;
    state->aux_ready = 0u;
}

void semu_ulsan_244_iom2_destroy(semu_ulsan_iom2 *state)
{
    semu_apollo4_iom_destroy(state->controller);
    semu_apollo4_dma_destroy(state->dma);
}
