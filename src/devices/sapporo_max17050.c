#include "sapporo_max17050.h"

#include <stdlib.h>

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
 * Register values are deterministic host-side battery fixtures, not physical
 * gauge evidence. The public datasheet has a 256-word map, but this endpoint
 * promotes only the selectors observed in the authentic Sapporo startup
 * command trace: 0x00, 0x06, 0x08, 0x09, 0x0b, 0x19, and 0x21.
 * Unobserved writes, reserved registers, and partial words refuse.
 */

enum {
    MAX_STATUS_REG = 0x00u,
    MAX_STATUS_VAL = 0x0000u,
    MAX_REPSOC_REG = 0x06u,
    MAX_REPSOC_VAL = 0x3200u,
    MAX_TEMP_REG = 0x08u,
    MAX_TEMP_VAL = 0x1900u,
    MAX_VCELL_REG = 0x09u,
    MAX_VCELL_VAL = 0xC000u,
    MAX_AVERAGE_VCELL_REG = 0x19u,
    MAX_AVERAGE_VCELL_VAL = 0xC000u,
    MAX_OBSERVED_ZERO_REG = 0x0bu,
    MAX_OBSERVED_LATER_REG = 0x21u
};

struct semu_sapporo_max17050 {
    uint8_t address;
    uint16_t status;
    uint16_t repsoc;
    uint16_t temperature;
    uint16_t vcell;
};

static semu_transaction_result refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "MAX17050 refuses %s", reason);
    return SEMU_TRANSACTION_REFUSE;
}

static void reset_state(semu_sapporo_max17050 *sensor)
{
    sensor->status = MAX_STATUS_VAL;
    sensor->repsoc = MAX_REPSOC_VAL;
    sensor->temperature = MAX_TEMP_VAL;
    sensor->vcell = MAX_VCELL_VAL;
}

static int is_observed_register(uint8_t reg)
{
    return reg == MAX_STATUS_REG || reg == MAX_REPSOC_REG ||
           reg == MAX_TEMP_REG || reg == MAX_VCELL_REG ||
           reg == MAX_AVERAGE_VCELL_REG || reg == MAX_OBSERVED_ZERO_REG ||
           reg == MAX_OBSERVED_LATER_REG;
}

static uint16_t register_value(const semu_sapporo_max17050 *sensor,
                               uint8_t reg)
{
    switch (reg) {
    case MAX_STATUS_REG:
        return sensor->status;
    case MAX_REPSOC_REG:
        return sensor->repsoc;
    case MAX_TEMP_REG:
        return sensor->temperature;
    case MAX_VCELL_REG:
        return sensor->vcell;
    case MAX_AVERAGE_VCELL_REG:
        return MAX_AVERAGE_VCELL_VAL;
    case MAX_OBSERVED_ZERO_REG:
    case MAX_OBSERVED_LATER_REG:
        return 0u;
    default:
        return 0u;
    }
}

static void write_little_endian(uint8_t *bytes, uint16_t value)
{
    bytes[0u] = (uint8_t)value;
    bytes[1u] = (uint8_t)(value >> 8u);
}

static semu_transaction_result transfer(void *context,
                                         semu_serial_transaction *t,
                                         semu_error *error)
{
    semu_sapporo_max17050 *sensor = (semu_sapporo_max17050 *)context;
    uint8_t reg;

    if (sensor == NULL || t == NULL) {
        return refuse(error, "null transaction");
    }
    if (t->address != sensor->address) {
        return refuse(error, "wrong address");
    }
    if (t->tx == NULL || t->tx_size == 0u ||
        (t->rx_size > 0u && t->rx == NULL) ||
        (t->rx_size == 0u && t->rx != NULL)) {
        return refuse(error, "invalid transaction buffers");
    }
    if (t->tx_size != 1u) {
        return refuse(error, "unobserved write transaction");
    }
    reg = t->tx[0u];
    if (t->rx_size > 0u) {
        if (t->rx_size != 2u || !is_observed_register(reg)) {
            return refuse(error, "unsupported read register or length");
        }
        write_little_endian(t->rx, register_value(sensor, reg));
    } else {
        return refuse(error, "unobserved write transaction");
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
