#include "sapporo_hsppad143.h"

#include <stdlib.h>
#include <string.h>

/*
 * E-SAP-HSPPAD143-001 verified trace
 * (sapporo-apollo4-pressure-boundary, sha256
 *  68d3832b5d2cf4c5292c0eef6fead660943199213c6bff571928db8a7aa36cd9):
 *   24 IOM2 I2C transactions at address 0x48, 4 M2P config writes (count=2),
 *   P2M reads (count=1/2/3), 11 completion responses (status=200).
 *
 * C# model (SapporoHsppad143.cs) protocol:
 *   Write: first byte selects register, subsequent bytes write with
 *          auto-increment.
 *   Read:  reads from selected register with auto-increment.
 *   FinishTransmission: keeps selected register across repeated-start.
 *   Reset: identity 0x00=0x49, ready 0x03=0x11, variant 0x1c=0xe0.
 *
 * Sample values are zero (synthetic configuration, not physical measurements).
 * Unknown register, wrong address, wrong length, and overflow refuse before
 * mutation. No wildcard readable/writable mask array is used.
 */

enum {
    HSPPAD_REG_COUNT = 256u,
    HSPPAD_IDENTITY_REG = 0x00u,
    HSPPAD_IDENTITY_VAL = 0x49u,
    HSPPAD_READY_REG = 0x03u,
    HSPPAD_READY_VAL = 0x11u,
    HSPPAD_VARIANT_REG = 0x1cu,
    HSPPAD_VARIANT_VAL = 0xe0u
};

struct semu_sapporo_hsppad143 {
    uint8_t address;
    uint8_t registers[HSPPAD_REG_COUNT];
    uint8_t selected;
};

static int is_known_register(uint8_t reg)
{
    return reg == HSPPAD_IDENTITY_REG ||
           reg == HSPPAD_READY_REG ||
           reg == HSPPAD_VARIANT_REG;
}

static semu_transaction_result refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "HSPPAD143 refuses %s", reason);
    return SEMU_TRANSACTION_REFUSE;
}

static void reset_state(semu_sapporo_hsppad143 *sensor)
{
    memset(sensor->registers, 0, sizeof(sensor->registers));
    sensor->registers[HSPPAD_IDENTITY_REG] = HSPPAD_IDENTITY_VAL;
    sensor->registers[HSPPAD_READY_REG] = HSPPAD_READY_VAL;
    sensor->registers[HSPPAD_VARIANT_REG] = HSPPAD_VARIANT_VAL;
    sensor->selected = 0u;
}

static semu_transaction_result handle_read(semu_sapporo_hsppad143 *sensor,
                                           semu_serial_transaction *t,
                                           semu_error *error)
{
    size_t i;
    if (t->tx_size == 0u) {
        return refuse(error, "read without register selection");
    }
    sensor->selected = t->tx[0];
    for (i = 0u; i < t->rx_size; ++i) {
        uint16_t idx = (uint16_t)(sensor->selected + (uint16_t)i);
        if (idx >= HSPPAD_REG_COUNT) {
            return refuse(error, "register overflow");
        }
        if (!is_known_register((uint8_t)idx)) {
            return refuse(error, "unknown read register");
        }
        t->rx[i] = sensor->registers[idx];
    }
    sensor->selected = (uint8_t)(sensor->selected + (uint8_t)t->rx_size);
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static semu_transaction_result handle_write(semu_sapporo_hsppad143 *sensor,
                                             semu_serial_transaction *t,
                                             semu_error *error)
{
    size_t i;
    if (t->tx_size == 0u) {
        semu_error_clear(error);
        return SEMU_TRANSACTION_OK;
    }
    sensor->selected = t->tx[0];
    for (i = 1u; i < t->tx_size; ++i) {
        uint16_t idx = (uint16_t)(sensor->selected + (uint16_t)(i - 1u));
        if (idx >= HSPPAD_REG_COUNT) {
            return refuse(error, "register overflow");
        }
        if (!is_known_register((uint8_t)idx)) {
            return refuse(error, "unknown write register");
        }
        sensor->registers[idx] = t->tx[i];
    }
    if (t->tx_size > 1u) {
        sensor->selected = (uint8_t)(sensor->selected + (uint8_t)(t->tx_size - 1u));
    }
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static semu_transaction_result transfer(void *context,
                                         semu_serial_transaction *t,
                                         semu_error *error)
{
    semu_sapporo_hsppad143 *sensor = (semu_sapporo_hsppad143 *)context;
    if (sensor == NULL || t == NULL) {
        return refuse(error, "null transaction");
    }
    if (t->address != sensor->address) {
        return refuse(error, "wrong address");
    }
    if (t->rx_size != 0u) {
        return handle_read(sensor, t, error);
    }
    return handle_write(sensor, t, error);
}

semu_sapporo_hsppad143 *semu_sapporo_hsppad143_create(uint8_t address,
                                                      semu_error *error)
{
    semu_sapporo_hsppad143 *sensor;
    sensor = (semu_sapporo_hsppad143 *)calloc(1u, sizeof(*sensor));
    if (sensor == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate HSPPAD143 sensor");
        return NULL;
    }
    sensor->address = address;
    reset_state(sensor);
    semu_error_clear(error);
    return sensor;
}

void semu_sapporo_hsppad143_destroy(semu_sapporo_hsppad143 *sensor)
{
    free(sensor);
}

void semu_sapporo_hsppad143_reset(void *context)
{
    semu_sapporo_hsppad143 *sensor = (semu_sapporo_hsppad143 *)context;
    if (sensor != NULL) {
        reset_state(sensor);
    }
}

semu_serial_endpoint semu_sapporo_hsppad143_endpoint(
    semu_sapporo_hsppad143 *sensor)
{
    semu_serial_endpoint endpoint;
    endpoint.name = sensor != NULL ? "HSPPAD143" : "invalid";
    endpoint.transfer = transfer;
    endpoint.context = sensor;
    return endpoint;
}
