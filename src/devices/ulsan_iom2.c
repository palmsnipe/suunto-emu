#include "ulsan_iom2.h"

#include <string.h>

/* E-ULS-0090. At this boundary the lane receives only the boot-mode query.
 * Repeating it leaves the identical response queued and GPIO62 high. */
static semu_transaction_result ohr_body(void *context,
    semu_sapporo_ohr2_command command, uint16_t sequence,
    semu_sapporo_ohr2_state mode, const uint8_t *request,
    uint8_t *response, semu_error *error)
{
    (void)context;
    (void)request;
    if (command != SEMU_SAPPORO_OHR2_COMMAND_BOOT_MODE || sequence != 0u ||
        mode != SEMU_SAPPORO_OHR2_BSL) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "Race S OHR2 startup body refused");
        return SEMU_TRANSACTION_REFUSE;
    }
    memset(response, 0, SEMU_SAPPORO_OHR2_PAYLOAD_SIZE);
    return SEMU_TRANSACTION_OK;
}

static void ohr_ready(void *context, unsigned signal, int level)
{
    semu_ulsan_iom2 *state = context;
    (void)signal;
    (void)semu_apollo4_gpio_set_input(state->gpio, 62u, level, NULL);
}

static semu_transaction_result ohr_transfer(semu_ulsan_iom2 *state,
    semu_serial_transaction *t, semu_error *error)
{
    static const uint8_t prefix[] = {0u, 0x10u, 0u, 0u, 0u, 1u};
    static const uint8_t crc[] = {0x5du, 0x08u, 0xd0u, 0x67u};
    semu_layer_state *layer = state->startup_layer;
    size_t i;

    if (t->tx == NULL || t->tx_size != SEMU_SAPPORO_OHR2_REQUEST_SIZE ||
        t->rx_size != 0u || memcmp(t->tx, prefix, sizeof(prefix)) != 0 ||
        memcmp(t->tx + 55u, crc, sizeof(crc)) != 0) goto refused;
    for (i = sizeof(prefix); i < 55u; ++i)
        if (t->tx[i] != 0xffu) goto refused;
    if (!layer->enabled || layer->hits >= layer->descriptor->maximum_hits)
        goto refused;
    if (!state->boot_request_queued) {
        semu_serial_endpoint endpoint = semu_sapporo_ohr2_endpoint(state->ohr);
        if (endpoint.transfer(endpoint.context, t, error) != SEMU_TRANSACTION_OK)
            return SEMU_TRANSACTION_REFUSE;
        state->boot_request_queued = 1;
    }
    return semu_layer_hit(layer, state->logger,
        "trigger=ohr2-boot-mode synthetic-ready-gpio62", error) == SEMU_OK
        ? SEMU_TRANSACTION_OK : SEMU_TRANSACTION_REFUSE;

refused:
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "Race S OHR2 startup transaction refused");
    return SEMU_TRANSACTION_REFUSE;
}

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
    if (address == 0x49u && state->ohr != NULL &&
        transaction->tx_size == 1u && transaction->tx != NULL) {
        static const uint8_t pressure[] = {0xf8u, 0x41u, 0u};
        static const uint8_t temperature[] = {0u, 0x19u};
        if (transaction->tx[0] == 4u && transaction->rx_size == sizeof(pressure)) {
            memcpy(transaction->rx, pressure, sizeof(pressure));
            return 1;
        }
        if (transaction->tx[0] == 9u && transaction->rx_size == sizeof(temperature)) {
            memcpy(transaction->rx, temperature, sizeof(temperature));
            return 1;
        }
        if (transaction->tx[0] == 3u && transaction->rx_size == 1u) {
            transaction->rx[0] = 0x11u;
            return 1;
        }
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
    int accepted;
    if (transaction->address == SEMU_SAPPORO_OHR2_ADDRESS && state->ohr != NULL)
        return ohr_transfer(state, transaction, error);
    accepted = transaction->rx_size != 0u
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

semu_status semu_ulsan_244_iom2_enable_startup(semu_ulsan_iom2 *state,
    semu_apollo4_gpio *gpio, semu_layer_state *layer, semu_logger *logger,
    semu_error *error)
{
    state->gpio = gpio;
    state->startup_layer = layer;
    state->logger = logger;
    state->ohr = semu_sapporo_ohr2_create(ohr_ready, state, ohr_body, state, error);
    if (state->ohr == NULL) return error != NULL ? error->code : SEMU_ERR_STATE;
    semu_apollo4_iom_enable_race_s_iom2_startup(state->controller);
    return semu_apollo4_gpio_set_input(gpio, 62u, 0, error);
}

void semu_ulsan_244_iom2_reset(semu_ulsan_iom2 *state)
{
    semu_apollo4_iom_reset(state->controller);
    semu_sapporo_ohr2_reset(state->ohr);
    state->boot_request_queued = 0;
    state->aux_low = state->aux_high = 0u;
    state->aux_ready = 0u;
}

void semu_ulsan_244_iom2_destroy(semu_ulsan_iom2 *state)
{
    semu_apollo4_iom_destroy(state->controller);
    semu_apollo4_dma_destroy(state->dma);
    semu_sapporo_ohr2_destroy(state->ohr);
}
