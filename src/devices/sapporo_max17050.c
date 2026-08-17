#include "sapporo_max17050.h"

#include <stdlib.h>
#include <string.h>

/*
 * E-SAP-MAX17050-001 verified trace
 * (sapporo-apollo4-max17050-boundary, sha256
 *  8a7b565f7cec7e6631707c99cc549361b131f26e1541706136a650f849cbde14):
 *   12 IOM4 I2C transactions at address 0x36, all P2M reads (count=2),
 *   including status (reg 0x00), VCell (reg 0x09), and RepSOC (reg 0x06)
 *   reads, plus 1 normal reset entry.
 *
 * C# model (SapporoApollo4Iom4.cs:SapporoMax17050) protocol:
 *   Write: first byte selects register, subsequent bytes write 16-bit
 *          registers in little-endian pairs (low byte, then high byte,
 *          then auto-increment).
 *   Read:  reads 16-bit registers from selected pointer in little-endian
 *          pairs with auto-increment.
 *   FinishTransmission: resets byte index and expects new register byte.
 *   Reset: Status 0x00=0x0000 (POR=0), RepSOC 0x06=0x3200 (50%),
 *          Temperature 0x08=0x1900, VCell 0x09=0xC000 (3.84V).
 *
 * Register values are deterministic host-side battery fixtures, not
 * physical gauge evidence. The Renode model exposes a 256-entry
 * zero-backed register array; only the four observed registers are non-zero.
 * Wrong address and invalid transactions refuse before mutation.
 */

enum {
    MAX_REG_COUNT = 256u,
    MAX_STATUS_REG = 0x00u,
    MAX_STATUS_VAL = 0x0000u,
    MAX_REPSOC_REG = 0x06u,
    MAX_REPSOC_VAL = 0x3200u,
    MAX_TEMP_REG = 0x08u,
    MAX_TEMP_VAL = 0x1900u,
    MAX_VCELL_REG = 0x09u,
    MAX_VCELL_VAL = 0xC000u
};

struct semu_sapporo_max17050 {
    uint8_t address;
    uint16_t registers[MAX_REG_COUNT];
};

static semu_transaction_result refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "MAX17050 refuses %s", reason);
    return SEMU_TRANSACTION_REFUSE;
}

static void reset_state(semu_sapporo_max17050 *sensor)
{
    memset(sensor->registers, 0, sizeof(sensor->registers));
    sensor->registers[MAX_STATUS_REG] = MAX_STATUS_VAL;
    sensor->registers[MAX_REPSOC_REG] = MAX_REPSOC_VAL;
    sensor->registers[MAX_TEMP_REG] = MAX_TEMP_VAL;
    sensor->registers[MAX_VCELL_REG] = MAX_VCELL_VAL;
}

static semu_transaction_result transfer(void *context,
                                         semu_serial_transaction *t,
                                         semu_error *error)
{
    semu_sapporo_max17050 *sensor = (semu_sapporo_max17050 *)context;
    uint8_t reg;
    size_t i;

    if (sensor == NULL || t == NULL) {
        return refuse(error, "null transaction");
    }
    if (t->address != sensor->address) {
        return refuse(error, "wrong address");
    }
    if (t->tx_size == 0u) {
        return refuse(error, "no register pointer");
    }
    reg = t->tx[0];
    if (t->rx_size > 0u) {
        for (i = 0u; i < t->rx_size; ++i) {
            if ((i & 1u) == 0u) {
                t->rx[i] = (uint8_t)(sensor->registers[reg] & 0xFFu);
            } else {
                t->rx[i] = (uint8_t)((sensor->registers[reg] >> 8u) & 0xFFu);
                reg = (uint8_t)(reg + 1u);
            }
        }
    } else {
        for (i = 1u; i < t->tx_size; ++i) {
            size_t byte_idx = i - 1u;
            if ((byte_idx & 1u) == 0u) {
                sensor->registers[reg] = (uint16_t)(
                    (sensor->registers[reg] & 0xFF00u) | t->tx[i]);
            } else {
                sensor->registers[reg] = (uint16_t)(
                    (sensor->registers[reg] & 0x00FFu) |
                    ((uint16_t)t->tx[i] << 8u));
                reg = (uint8_t)(reg + 1u);
            }
        }
    }
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

semu_sapporo_max17050 *semu_sapporo_max17050_create(uint8_t address,
                                                     semu_error *error)
{
    semu_sapporo_max17050 *sensor;
    sensor = (semu_sapporo_max17050 *)calloc(1u, sizeof(*sensor));
    if (sensor == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate MAX17050 fuel gauge");
        return NULL;
    }
    sensor->address = address;
    reset_state(sensor);
    semu_error_clear(error);
    return sensor;
}

void semu_sapporo_max17050_destroy(semu_sapporo_max17050 *sensor)
{
    free(sensor);
}

void semu_sapporo_max17050_reset(void *context)
{
    semu_sapporo_max17050 *sensor = (semu_sapporo_max17050 *)context;
    if (sensor != NULL) {
        reset_state(sensor);
    }
}

semu_serial_endpoint semu_sapporo_max17050_endpoint(
    semu_sapporo_max17050 *sensor)
{
    semu_serial_endpoint endpoint;
    endpoint.name = sensor != NULL ? "MAX17050" : "invalid";
    endpoint.transfer = transfer;
    endpoint.context = sensor;
    return endpoint;
}
