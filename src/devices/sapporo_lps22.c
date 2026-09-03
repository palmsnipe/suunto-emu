#include "sapporo_lps22.h"

#include <stdlib.h>

/*
 * E-SAP-LPS22-239-001: Sapporo 2.39 probes an LPS22HB at I2C address
 * 0x5c, reads WHO_AM_I/CTRL_REG1/CTRL_REG2/0x33, and writes only the two
 * control registers.  CTRL_REG2 reset and one-shot bits clear immediately.
 * Sample register 0x33 is a deterministic zero fixture; no conversion engine
 * or nonzero physical sample is claimed.
 */

enum {
    LPS22_WHO_AM_I = 0x0fu,
    LPS22_CTRL_REG1 = 0x10u,
    LPS22_CTRL_REG2 = 0x11u,
    LPS22_SAMPLE_REG = 0x33u,
    LPS22HB_ID = 0xb1u,
    LPS22_CTRL_REG2_SELF_CLEAR = 0x84u
};

struct semu_sapporo_lps22 {
    uint8_t address;
    uint8_t selected;
    uint8_t ctrl1;
    uint8_t ctrl2;
};

static semu_transaction_result refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "LPS22 refuses %s", reason);
    return SEMU_TRANSACTION_REFUSE;
}

static int readable_register(uint8_t reg)
{
    return reg == LPS22_WHO_AM_I || reg == LPS22_CTRL_REG1 ||
           reg == LPS22_CTRL_REG2 || reg == LPS22_SAMPLE_REG;
}

static int writable_register(uint8_t reg)
{
    return reg == LPS22_CTRL_REG1 || reg == LPS22_CTRL_REG2;
}

static uint8_t register_value(const semu_sapporo_lps22 *sensor, uint8_t reg)
{
    switch (reg) {
    case LPS22_WHO_AM_I: return LPS22HB_ID;
    case LPS22_CTRL_REG1: return sensor->ctrl1;
    case LPS22_CTRL_REG2: return sensor->ctrl2;
    case LPS22_SAMPLE_REG: return 0u;
    default: return 0u;
    }
}

static semu_transaction_result transfer(void *context,
                                         semu_serial_transaction *transaction,
                                         semu_error *error)
{
    semu_sapporo_lps22 *sensor = (semu_sapporo_lps22 *)context;
    uint8_t reg;
    uint8_t value;

    if (sensor == NULL || transaction == NULL) {
        return refuse(error, "null transaction");
    }
    if (transaction->address != sensor->address) {
        return refuse(error, "wrong address");
    }
    if (transaction->rx_size == 1u) {
        if (transaction->tx == NULL || transaction->tx_size != 1u ||
            transaction->rx == NULL) {
            return refuse(error, "read shape");
        }
        reg = (uint8_t)(transaction->tx[0u] & 0x7fu);
        if (!readable_register(reg)) {
            return refuse(error, "unknown read register");
        }
        transaction->rx[0u] = register_value(sensor, reg);
        sensor->selected = reg;
        semu_error_clear(error);
        return SEMU_TRANSACTION_OK;
    }
    if (transaction->rx != NULL || transaction->rx_size != 0u ||
        transaction->tx == NULL || transaction->tx_size != 2u) {
        return refuse(error, "write shape");
    }
    reg = (uint8_t)(transaction->tx[0u] & 0x7fu);
    value = transaction->tx[1u];
    if (!writable_register(reg)) {
        return refuse(error, "unknown write register");
    }
    if (reg == LPS22_CTRL_REG1) {
        sensor->ctrl1 = value;
    } else {
        sensor->ctrl2 = (uint8_t)(value &
            (uint8_t)~LPS22_CTRL_REG2_SELF_CLEAR);
    }
    sensor->selected = reg;
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static void reset_state(semu_sapporo_lps22 *sensor)
{
    sensor->selected = 0u;
    sensor->ctrl1 = 0u;
    sensor->ctrl2 = 0u;
}

semu_sapporo_lps22 *semu_sapporo_lps22_create(uint8_t address,
                                               semu_error *error)
{
    semu_sapporo_lps22 *sensor;
    if (address != 0x5cu) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "LPS22 requires evidenced address 0x5c");
        return NULL;
    }
    sensor = (semu_sapporo_lps22 *)calloc(1u, sizeof(*sensor));
    if (sensor == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate LPS22 sensor");
        return NULL;
    }
    sensor->address = address;
    reset_state(sensor);
    semu_error_clear(error);
    return sensor;
}

void semu_sapporo_lps22_destroy(semu_sapporo_lps22 *sensor)
{
    free(sensor);
}

void semu_sapporo_lps22_reset(void *context)
{
    semu_sapporo_lps22 *sensor = (semu_sapporo_lps22 *)context;
    if (sensor != NULL) reset_state(sensor);
}

semu_serial_endpoint semu_sapporo_lps22_endpoint(
    semu_sapporo_lps22 *sensor)
{
    semu_serial_endpoint endpoint;
    endpoint.name = sensor != NULL ? "LPS22HB" : "invalid";
    endpoint.transfer = transfer;
    endpoint.context = sensor;
    return endpoint;
}

semu_status semu_sapporo_lps22_snapshot_write(
    const semu_sapporo_lps22 *sensor, semu_snapshot_writer *writer,
    semu_error *error)
{
    if (sensor == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "LPS22 snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (semu_snapshot_writer_u8(writer, sensor->address, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, sensor->selected, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, sensor->ctrl1, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, sensor->ctrl2, error) != SEMU_OK) {
        return error->code;
    }
    return SEMU_OK;
}

semu_status semu_sapporo_lps22_snapshot_read(
    semu_sapporo_lps22 *sensor, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_sapporo_lps22 candidate;
    if (sensor == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "LPS22 snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *sensor;
    if (semu_snapshot_reader_u8(reader, &candidate.address, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &candidate.selected, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &candidate.ctrl1, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &candidate.ctrl2, error) != SEMU_OK) {
        return error->code;
    }
    if (candidate.address != sensor->address) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "LPS22 snapshot identity mismatch");
        return SEMU_ERR_CONFLICT;
    }
    if ((candidate.selected != 0u &&
         !readable_register(candidate.selected)) ||
        (candidate.ctrl2 & LPS22_CTRL_REG2_SELF_CLEAR) != 0u) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "LPS22 snapshot state is invalid");
        return SEMU_ERR_FORMAT;
    }
    *sensor = candidate;
    return SEMU_OK;
}
