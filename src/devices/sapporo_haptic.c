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
 * The identity of the PMIC is unresolved, so only the registers and waveform
 * window observed by the native startup path are exposed. Unknown addresses
 * and invalid spans refuse before mutation.
 */

enum {
    HAPTIC_FAULT_REG = 0x01u,
    HAPTIC_STATUS_REG = 0x08u,
    HAPTIC_WAVEFORM_SELECT_REG = 0x09u,
    HAPTIC_CONFIG0_REG = 0x0du,
    HAPTIC_CONFIG1_REG = 0x11u,
    HAPTIC_CONFIG2_REG = 0x12u,
    HAPTIC_CONFIG3_REG = 0x13u,
    HAPTIC_CONFIG4_REG = 0x1du,
    HAPTIC_CONFIG5_REG = 0x1eu,
    HAPTIC_AUTOTUNE_REG = 0x22u,
    HAPTIC_AUTOTUNE_TRIGGER = 0x01u,
    HAPTIC_AUTOTUNE_COMPLETE = 0x02u,
    HAPTIC_WAVEFORM_FIRST = 0x40u,
    HAPTIC_WAVEFORM_LAST = 0x43u
};

struct semu_sapporo_haptic {
    uint8_t address;
    uint8_t selected;
    uint8_t waveform_select;
    uint8_t config0;
    uint8_t config1;
    uint8_t config2;
    uint8_t config3;
    uint8_t config4;
    uint8_t config5;
    uint8_t autotune;
    uint8_t waveform[4];
};

static semu_transaction_result refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "Haptic PMIC refuses %s", reason);
    return SEMU_TRANSACTION_REFUSE;
}

static void reset_state(semu_sapporo_haptic *sensor)
{
    sensor->selected = 0u;
    sensor->waveform_select = 0u;
    sensor->config0 = 0u;
    sensor->config1 = 0u;
    sensor->config2 = 0u;
    sensor->config3 = 0u;
    sensor->config4 = 0u;
    sensor->config5 = 0u;
    sensor->autotune = 0u;
    (void)memset(sensor->waveform, 0, sizeof(sensor->waveform));
}

static int is_known_register(uint8_t reg)
{
    return reg == HAPTIC_FAULT_REG || reg == HAPTIC_STATUS_REG ||
           reg == HAPTIC_WAVEFORM_SELECT_REG ||
           reg == HAPTIC_CONFIG0_REG ||
           reg == HAPTIC_CONFIG1_REG || reg == HAPTIC_CONFIG2_REG ||
           reg == HAPTIC_CONFIG3_REG || reg == HAPTIC_CONFIG4_REG ||
           reg == HAPTIC_CONFIG5_REG || reg == HAPTIC_AUTOTUNE_REG ||
           (reg >= HAPTIC_WAVEFORM_FIRST && reg <= HAPTIC_WAVEFORM_LAST);
}

static int is_writable_register(uint8_t reg)
{
    return reg != HAPTIC_FAULT_REG && reg != HAPTIC_STATUS_REG &&
           is_known_register(reg);
}

static uint8_t register_value(const semu_sapporo_haptic *sensor,
                              uint8_t reg)
{
    switch (reg) {
    case HAPTIC_STATUS_REG:
        return 0u;
    case HAPTIC_WAVEFORM_SELECT_REG:
        return sensor->waveform_select;
    case HAPTIC_CONFIG0_REG:
        return sensor->config0;
    case HAPTIC_CONFIG1_REG:
        return sensor->config1;
    case HAPTIC_CONFIG2_REG:
        return sensor->config2;
    case HAPTIC_CONFIG3_REG:
        return sensor->config3;
    case HAPTIC_CONFIG4_REG:
        return sensor->config4;
    case HAPTIC_CONFIG5_REG:
        return sensor->config5;
    case HAPTIC_AUTOTUNE_REG:
        return sensor->autotune;
    default:
        if (reg >= HAPTIC_WAVEFORM_FIRST &&
            reg <= HAPTIC_WAVEFORM_LAST) {
            return sensor->waveform[reg - HAPTIC_WAVEFORM_FIRST];
        }
        return 0u;
    }
}

static void write_register(semu_sapporo_haptic *sensor, uint8_t reg,
                           uint8_t value)
{
    switch (reg) {
    case HAPTIC_WAVEFORM_SELECT_REG:
        sensor->waveform_select = value;
        break;
    case HAPTIC_CONFIG0_REG:
        sensor->config0 = value;
        break;
    case HAPTIC_CONFIG1_REG:
        sensor->config1 = value;
        break;
    case HAPTIC_CONFIG2_REG:
        sensor->config2 = value;
        break;
    case HAPTIC_CONFIG3_REG:
        sensor->config3 = value;
        break;
    case HAPTIC_CONFIG4_REG:
        sensor->config4 = value;
        break;
    case HAPTIC_CONFIG5_REG:
        sensor->config5 = value;
        break;
    case HAPTIC_AUTOTUNE_REG:
        sensor->autotune = value;
        if ((value & HAPTIC_AUTOTUNE_TRIGGER) != 0u) {
            sensor->autotune |= HAPTIC_AUTOTUNE_COMPLETE;
        }
        break;
    default:
        if (reg >= HAPTIC_WAVEFORM_FIRST &&
            reg <= HAPTIC_WAVEFORM_LAST) {
            sensor->waveform[reg - HAPTIC_WAVEFORM_FIRST] = value;
        }
        break;
    }
}

static int span_is_valid(uint8_t start, size_t count, int writable)
{
    size_t i;

    if (count == 0u || count > 256u) {
        return 0;
    }
    for (i = 0u; i < count; ++i) {
        uint16_t reg = (uint16_t)start + (uint16_t)i;
        if (reg > 0xffu || !is_known_register((uint8_t)reg) ||
            (writable && !is_writable_register((uint8_t)reg))) {
            return 0;
        }
    }
    return 1;
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
    if ((t->tx_size > 0u && t->tx == NULL) ||
        (t->rx_size > 0u && t->rx == NULL) ||
        (t->tx_size == 0u && t->tx != NULL) ||
        (t->rx_size == 0u && t->rx != NULL)) {
        return refuse(error, "null or empty buffer");
    }
    if (t->rx_size > 0u) {
        uint8_t start = sensor->selected;
        if (t->tx_size > 1u) {
            return refuse(error, "read selector shape");
        }
        if (t->tx_size == 1u) {
            start = t->tx[0u];
        }
        if (!span_is_valid(start, t->rx_size, 0)) {
            return refuse(error, "unsupported read span");
        }
        for (i = 0u; i < t->rx_size; ++i) {
            t->rx[i] = register_value(sensor, (uint8_t)(start + i));
        }
        sensor->selected = (uint8_t)(start + t->rx_size);
    } else {
        uint8_t start;
        if (t->tx_size == 0u) {
            return refuse(error, "empty write");
        }
        start = t->tx[0u];
        if (!is_known_register(start)) {
            return refuse(error, "unknown register selector");
        }
        if (t->tx_size == 1u) {
            sensor->selected = start;
            semu_error_clear(error);
            return SEMU_TRANSACTION_OK;
        }
        if (!span_is_valid(start, t->tx_size - 1u, 1)) {
            return refuse(error, "unsupported write span");
        }
        /* Validate the full frame before applying any configuration byte. */
        for (i = 1u; i < t->tx_size; ++i) {
            write_register(sensor, (uint8_t)(start + i - 1u), t->tx[i]);
        }
        sensor->selected = (uint8_t)(start + t->tx_size - 1u);
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
