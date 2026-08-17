#include "sapporo_haptic.h"

#include <stdlib.h>
#include <string.h>

/*
 * E-SAP-HAPTIC-001 verified trace
 * (sapporo-apollo4-haptic-boundary, sha256
 *  4eecc0e41dd25b3a01dd0c6cdf1c5801e5f423093b9331874a278fdc0ca2ac5c):
 *   65 IOM4 I2C transactions at address 0x50 with M2P writes (count=2/5)
 *   and P2M reads (count=1), plus 4 feedback completion responses (status=200).
 *
 * C# model (SapporoHapticPmic.cs) protocol:
 *   Write: first byte selects register, subsequent bytes write with
 *          auto-increment. Writing bit 0 to the autotune control register
 *          0x22 immediately sets the autotune-complete bit (bit 1).
 *   Read:  reads from selected register with auto-increment.
 *   FinishTransmission: no-op.
 *   Reset: all registers cleared to 0x00.
 *
 * The autotune state machine is: idle (0x00) → triggered (bit 0 written)
 * → complete (bit 1 set). Sample data is zero (synthetic no-fault state).
 * The Renode endpoint retains the complete zero-backed 256-byte register
 * array. Wrong address and register-window overflow refuse before mutation.
 */

enum {
    HAPTIC_REG_COUNT = 256u,
    HAPTIC_AUTOTUNE_REG = 0x22u,
    HAPTIC_AUTOTUNE_TRIGGER = 0x01u,
    HAPTIC_AUTOTUNE_COMPLETE = 0x02u
};

struct semu_sapporo_haptic {
    uint8_t address;
    uint8_t registers[HAPTIC_REG_COUNT];
    uint8_t selected;
};

static semu_transaction_result refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "Haptic PMIC refuses %s", reason);
    return SEMU_TRANSACTION_REFUSE;
}

static void reset_state(semu_sapporo_haptic *sensor)
{
    memset(sensor->registers, 0, sizeof(sensor->registers));
    sensor->selected = 0u;
}

static semu_transaction_result transfer(void *context,
                                         semu_serial_transaction *t,
                                         semu_error *error)
{
    semu_sapporo_haptic *sensor = (semu_sapporo_haptic *)context;
    size_t i;

    if (sensor == NULL || t == NULL) {
        return refuse(error, "null transaction");
    }
    if (t->address != sensor->address) {
        return refuse(error, "wrong address");
    }
    if (t->rx_size > 0u) {
        if (t->tx_size == 0u) {
            return refuse(error, "read without register selection");
        }
        if ((size_t)t->tx[0] + t->rx_size > HAPTIC_REG_COUNT) {
            return refuse(error, "read register overflow");
        }
        sensor->selected = t->tx[0];
        for (i = 0u; i < t->rx_size; ++i) {
            uint16_t idx = (uint16_t)(sensor->selected + (uint16_t)i);
            t->rx[i] = sensor->registers[idx];
        }
        sensor->selected = (uint8_t)(sensor->selected + (uint8_t)t->rx_size);
    } else {
        if (t->tx_size == 0u) {
            semu_error_clear(error);
            return SEMU_TRANSACTION_OK;
        }
        if ((size_t)t->tx[0] + t->tx_size - 1u > HAPTIC_REG_COUNT) {
            return refuse(error, "write register overflow");
        }
        sensor->selected = t->tx[0];
        for (i = 1u; i < t->tx_size; ++i) {
            uint16_t idx = (uint16_t)(sensor->selected + (uint16_t)(i - 1u));
            sensor->registers[idx] = t->tx[i];
            if ((uint8_t)idx == HAPTIC_AUTOTUNE_REG &&
                (t->tx[i] & HAPTIC_AUTOTUNE_TRIGGER) != 0u) {
                sensor->registers[idx] |= HAPTIC_AUTOTUNE_COMPLETE;
            }
        }
        if (t->tx_size > 1u) {
            sensor->selected = (uint8_t)(sensor->selected +
                                         (uint8_t)(t->tx_size - 1u));
        }
    }
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

semu_sapporo_haptic *semu_sapporo_haptic_create(uint8_t address,
                                                 semu_error *error)
{
    semu_sapporo_haptic *sensor;
    sensor = (semu_sapporo_haptic *)calloc(1u, sizeof(*sensor));
    if (sensor == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate Haptic PMIC");
        return NULL;
    }
    sensor->address = address;
    reset_state(sensor);
    semu_error_clear(error);
    return sensor;
}

void semu_sapporo_haptic_destroy(semu_sapporo_haptic *sensor)
{
    free(sensor);
}

void semu_sapporo_haptic_reset(void *context)
{
    semu_sapporo_haptic *sensor = (semu_sapporo_haptic *)context;
    if (sensor != NULL) {
        reset_state(sensor);
    }
}

semu_serial_endpoint semu_sapporo_haptic_endpoint(
    semu_sapporo_haptic *sensor)
{
    semu_serial_endpoint endpoint;
    endpoint.name = sensor != NULL ? "Haptic PMIC" : "invalid";
    endpoint.transfer = transfer;
    endpoint.context = sensor;
    return endpoint;
}
