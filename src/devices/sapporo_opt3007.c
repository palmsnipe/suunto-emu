#include "sapporo_opt3007.h"

#include <stdlib.h>
#include <string.h>

/*
 * E-SAP-OPT3007-001 verified trace
 * (sapporo-apollo4-als-boundary, sha256
 *  e4e4c8b9263dbd2c6560f03c7336580b7ec820a7110ff54c0bf746c7e7782c4e):
 *   7 IOM3 I2C transactions at address 0x45: M2P config write (count=3,
 *   register 0x01=0xC600 big-endian) and P2M result reads (count=2,
 *   pointer 0x00), plus 4 completion responses (status=200).
 *
 * C# model (SapporoOpt3007.cs) protocol:
 *   Write: first byte selects register pointer, subsequent bytes write
 *          with auto-increment. The OPT3007 uses 16-bit registers;
 *          config 0x01 is written as big-endian 0xC600 (0xC6, 0x00).
 *   Read:  reads from selected pointer with auto-increment.
 *   FinishTransmission: no-op.
 *   Reset: all registers cleared to 0x00.
 *
 * The result register (pointer 0x00) is an explicit zero-lux fixture
 * (0x0000). The config register (pointer 0x01) is readable/writable.
 * Unknown pointer, wrong address, wrong length, and wrong byte order
 * refuse before mutation. No wildcard readable/writable mask is used.
 */

enum {
    OPT_RESULT_PTR = 0x00u,
    OPT_CONFIG_PTR = 0x01u,
    OPT_REG_SIZE = 2u
};

struct semu_sapporo_opt3007 {
    uint8_t address;
    uint8_t result_high;
    uint8_t result_low;
    uint8_t config_high;
    uint8_t config_low;
};

static int is_known_pointer(uint8_t ptr)
{
    return ptr == OPT_RESULT_PTR || ptr == OPT_CONFIG_PTR;
}

static semu_transaction_result refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "OPT3007 refuses %s", reason);
    return SEMU_TRANSACTION_REFUSE;
}

static void reset_state(semu_sapporo_opt3007 *sensor)
{
    sensor->result_high = 0u;
    sensor->result_low = 0u;
    sensor->config_high = 0u;
    sensor->config_low = 0u;
}

static semu_transaction_result transfer(void *context,
                                         semu_serial_transaction *t,
                                         semu_error *error)
{
    semu_sapporo_opt3007 *sensor = (semu_sapporo_opt3007 *)context;
    uint8_t ptr;

    if (sensor == NULL || t == NULL) {
        return refuse(error, "null transaction");
    }
    if (t->address != sensor->address) {
        return refuse(error, "wrong address");
    }
    if (t->tx_size == 0u) {
        return refuse(error, "no pointer");
    }
    ptr = t->tx[0];
    if (!is_known_pointer(ptr)) {
        return refuse(error, "unknown pointer");
    }
    if (t->rx_size > 0u) {
        if (t->rx_size > OPT_REG_SIZE) {
            return refuse(error, "read length exceeds register");
        }
        if (ptr == OPT_RESULT_PTR) {
            t->rx[0u] = sensor->result_high;
            if (t->rx_size > 1u) {
                t->rx[1u] = sensor->result_low;
            }
        } else {
            t->rx[0u] = sensor->config_high;
            if (t->rx_size > 1u) {
                t->rx[1u] = sensor->config_low;
            }
        }
    } else {
        size_t data_count = t->tx_size - 1u;
        if (ptr == OPT_RESULT_PTR) {
            return refuse(error, "result register is read-only");
        }
        if (data_count > OPT_REG_SIZE) {
            return refuse(error, "write length exceeds register");
        }
        if (data_count >= 1u) {
            sensor->config_high = t->tx[1u];
        }
        if (data_count >= 2u) {
            sensor->config_low = t->tx[2u];
        }
    }
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

semu_sapporo_opt3007 *semu_sapporo_opt3007_create(uint8_t address,
                                                   semu_error *error)
{
    semu_sapporo_opt3007 *sensor;
    sensor = (semu_sapporo_opt3007 *)calloc(1u, sizeof(*sensor));
    if (sensor == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate OPT3007 sensor");
        return NULL;
    }
    sensor->address = address;
    reset_state(sensor);
    semu_error_clear(error);
    return sensor;
}

void semu_sapporo_opt3007_destroy(semu_sapporo_opt3007 *sensor)
{
    free(sensor);
}

void semu_sapporo_opt3007_reset(void *context)
{
    semu_sapporo_opt3007 *sensor = (semu_sapporo_opt3007 *)context;
    if (sensor != NULL) {
        reset_state(sensor);
    }
}

semu_serial_endpoint semu_sapporo_opt3007_endpoint(
    semu_sapporo_opt3007 *sensor)
{
    semu_serial_endpoint endpoint;
    endpoint.name = sensor != NULL ? "OPT3007" : "invalid";
    endpoint.transfer = transfer;
    endpoint.context = sensor;
    return endpoint;
}
