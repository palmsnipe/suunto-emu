#include "ulsan_motion.h"

#include <string.h>

/* E-ULS-0085/E-ULS-0094. LSM6DSOX setup, empty FIFO and LIS2MDL startup. */
static int motion_write(semu_ulsan_motion *state, const semu_serial_transaction *t)
{
    static const uint16_t writes[] = {
        0x0100u, 0x0140u, 0x0180u, 0x0704u, 0x0800u, 0x0903u, 0x0a00u,
        0x0a06u, 0x0b00u, 0x0c00u, 0x0d00u, 0x0e00u, 0x1000u, 0x100cu,
        0x103cu, 0x1040u, 0x1100u, 0x1201u, 0x1204u, 0x1244u, 0x1245u,
        0x1304u, 0x1408u, 0x140cu, 0x153cu, 0x153du, 0x164fu, 0x1660u,
        0x16cfu, 0x1701u, 0x1802u, 0x2142u, 0x5800u, 0x5880u, 0x5b01u,
        0x5e00u, 0x5e20u, 0x5f00u, 0x6200u
    };
    uint16_t pair;
    size_t i;
    unsigned reg;

    if (t->tx == NULL || t->tx_size != 2u)
        return 0;
    pair = (uint16_t)((uint16_t)t->tx[0] << 8u | t->tx[1]);
    for (i = 0u; i < SEMU_ARRAY_LEN(writes); ++i)
        if (writes[i] == pair) break;
    if (i == SEMU_ARRAY_LEN(writes))
        return 0;
    reg = t->tx[0];
    state->registers[reg] = reg == 0x12u ? (uint8_t)(t->tx[1] & 0xfeu) : t->tx[1];
    if (pair == 0x153du) state->hub_setup |= 1u;
    if (pair == 0x164fu) state->hub_setup |= 2u;
    if (pair == 0x1701u) {
        if ((state->hub_setup & 3u) == 3u) state->hub_setup |= 4u;
        state->hub_consumed = 0;
    }
    return 1;
}

static int motion_read(semu_ulsan_motion *state, semu_serial_transaction *t)
{
    static const uint8_t selectors[] = {
        0x81u, 0x82u, 0x84u, 0x85u, 0x88u, 0x89u, 0x8au, 0x8bu, 0x8cu, 0x8du, 0x8eu, 0x8fu,
        0x90u, 0x91u, 0x92u, 0x93u, 0x94u, 0x95u, 0x96u, 0x97u, 0x98u, 0x9eu,
        0xa2u, 0xbau, 0xbbu, 0xd8u, 0xdbu, 0xdeu, 0xdfu, 0xe2u
    };
    uint8_t selector;
    unsigned reg;
    size_t i;

    if (t->tx == NULL || t->tx_size != 1u || t->rx == NULL)
        return 0;
    selector = t->tx[0];
    if (t->rx_size == 1u) {
        for (i = 0u; i < SEMU_ARRAY_LEN(selectors); ++i)
            if (selectors[i] == selector) break;
        if (i == SEMU_ARRAY_LEN(selectors)) return 0;
    } else if (!(selector == 0xc6u && t->rx_size == 2u) &&
               !(selector == 0xa8u && t->rx_size == 6u)) {
        return 0;
    }
    reg = selector & 0x7fu;
    memcpy(t->rx, state->registers + reg, t->rx_size);
    if ((state->hub_setup & 4u) != 0u) {
        if (reg == 0x1eu) t->rx[0] = 1u;
        if (reg == 0x02u) t->rx[0] = 0x40u;
        if (reg == 0x22u) {
            t->rx[0] = state->hub_consumed ? 1u : 0x40u;
            state->hub_consumed = 1;
        }
    }
    return 1;
}

static semu_transaction_result motion_transfer(void *context,
    semu_serial_transaction *transaction, semu_error *error)
{
    semu_ulsan_motion *state = context;
    if (transaction->chip_select == 0u &&
        (transaction->rx_size != 0u ? motion_read(state, transaction) :
                                     motion_write(state, transaction)))
        return SEMU_TRANSACTION_OK;
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "Race S IOM0 motion transfer is unobserved");
    return SEMU_TRANSACTION_REFUSE;
}

semu_status semu_ulsan_244_motion_map(semu_bus *bus, semu_ulsan_motion *state,
    semu_apollo4_dma *dma, semu_scheduler *scheduler, semu_apollo4_iom_irq_fn irq,
    void *context, semu_error *error)
{
    state->controller = semu_apollo4_iom_create(bus, SEMU_APOLLO4_IOM0_BASE,
        SEMU_APOLLO4_IOM0_IRQ, irq, context, semu_apollo4_dma_execute,
        dma, scheduler, error);
    if (state->controller == NULL)
        return error != NULL ? error->code : SEMU_ERR_STATE;
    semu_apollo4_iom_select_race_s(state->controller);
    state->endpoint = (semu_serial_endpoint){"ulsan.motion", motion_transfer, state};
    return semu_apollo4_iom_attach_endpoint(state->controller, &state->endpoint, error);
}

void semu_ulsan_244_motion_reset(semu_ulsan_motion *state)
{
    semu_apollo4_iom_reset(state->controller);
    memset(state->registers, 0, sizeof(state->registers));
    state->registers[0x0fu] = 0x6cu;
    state->registers[0x3bu] = 0x10u;
    state->hub_setup = 0u;
    state->hub_consumed = 0;
}
