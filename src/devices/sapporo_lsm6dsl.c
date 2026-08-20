#include "sapporo_lsm6dsl.h"

#include <stdlib.h>
#include <string.h>

/*
 * E-SAP-LSM6DSL-001 verified trace
 * (sapporo-apollo4-lsm6dsl-boundary, sha256
 *  2f132b851cfd458f93964c2ebc3f9c962bcb4e0ac878cb907dab9e2c410c9b38):
 *   207 SPI transactions (chip_select=0), 207 completion IRQs,
 *   71 held-cs commands, 131 two-byte DMA commands, 11 startup responses.
 *
 * C# model (SapporoLsm6Dsl.cs) protocol:
 *   Transmit: first byte is command (bit7=read, bit6=increment,
 *             bits0-5=register). Subsequent bytes are data with
 *             auto-increment when enabled.
 *   FinishTransmission: retains decoded register across held-CS phases.
 *   EndNativeTransaction: clears command state.
 *   Reset: WHO_AM_I 0x0f=0x6a, FIFO_STATUS2 0x3b=0x10 (EMPTY bit).
 *
 * The firmware reads WHO_AM_I (0x0f) and FIFO_STATUS1-2 (0x3a-0x3b)
 * via two-phase held-CS SPI. CTRL3_C (0x12) is the documented startup
 * configuration register. Output and FIFO payload bytes are zero (synthetic).
 * The public register map is explicit: reserved and unobserved control
 * registers refuse instead of becoming a wildcard register file.
 */

enum {
    LSM6_READ_BIT = 0x80u,
    LSM6_INCREMENT_BIT = 0x40u,
    LSM6_REGISTER_MASK = 0x3fu,
    LSM6_WHO_AM_I_REG = 0x0fu,
    LSM6_WHO_AM_I_VAL = 0x6au,
    LSM6_CTRL3_C_REG = 0x12u,
    LSM6_CTRL3_C_RESET = 0x04u,
    LSM6_CTRL3_C_SW_RESET = 0x01u,
    LSM6_CTRL3_C_WRITE_MASK = 0xfeu,
    LSM6_FIFO_STATUS1_REG = 0x3au,
    LSM6_FIFO_STATUS1_VAL = 0x00u,
    LSM6_FIFO_STATUS2_REG = 0x3bu,
    LSM6_FIFO_EMPTY = 0x10u,
    LSM6_OUTPUT_FIRST = 0x20u,
    LSM6_OUTPUT_LAST = 0x3fu
};

struct semu_sapporo_lsm6dsl {
    uint8_t chip_select;
    uint8_t config[0x1fu];
    uint8_t reg;
    int have_command;
    int read;
    int increment;
};

static int is_known_register(uint8_t reg)
{
    /* E-SAP-LSM6DSL-001: these control spans are present in the native
       startup/configuration transcript; unobserved registers remain closed. */
    return (reg >= 0x01u && reg <= 0x0au) || reg == 0x0du ||
           reg == 0x0eu || (reg >= 0x10u && reg <= 0x1au) ||
           reg == 0x1bu || reg == 0x1cu || reg == 0x1eu ||
           reg == LSM6_WHO_AM_I_REG ||
           (reg >= LSM6_OUTPUT_FIRST && reg <= LSM6_OUTPUT_LAST);
}

static int is_writable_register(uint8_t reg)
{
    return (reg >= 0x01u && reg <= 0x0au) || reg == 0x0du ||
           reg == 0x0eu || (reg >= 0x10u && reg <= 0x1au) ||
           reg == 0x1bu || reg == 0x1cu || reg == 0x1eu;
}

static semu_transaction_result refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "LSM6DSL refuses %s", reason);
    return SEMU_TRANSACTION_REFUSE;
}

static void reset_state(semu_sapporo_lsm6dsl *sensor)
{
    memset(sensor->config, 0, sizeof(sensor->config));
    sensor->config[LSM6_CTRL3_C_REG] = LSM6_CTRL3_C_RESET;
    sensor->reg = 0u;
    sensor->have_command = 0;
    sensor->read = 0;
    sensor->increment = 0;
}

static uint8_t register_value(const semu_sapporo_lsm6dsl *sensor,
                              uint8_t reg)
{
    if (reg == LSM6_WHO_AM_I_REG) {
        return LSM6_WHO_AM_I_VAL;
    }
    if (reg == LSM6_CTRL3_C_REG) {
        return sensor->config[reg];
    }
    if (reg <= 0x1eu && is_writable_register(reg)) {
        return sensor->config[reg];
    }
    if (reg == LSM6_FIFO_STATUS2_REG) {
        return LSM6_FIFO_EMPTY;
    }
    if (reg == LSM6_FIFO_STATUS1_REG) {
        return LSM6_FIFO_STATUS1_VAL;
    }
    return 0u;
}

static int span_is_known(uint8_t start, size_t count, int writable,
                         int increment)
{
    size_t i;

    if (count == 0u || count > 128u) {
        return 0;
    }
    for (i = 0u; i < count; ++i) {
        uint16_t index = (uint16_t)start +
                         (uint16_t)(increment ? i : 0u);
        if (index > 0xffu || !is_known_register((uint8_t)index) ||
            (writable && !is_writable_register((uint8_t)index))) {
            return 0;
        }
    }
    return 1;
}

static void write_register(semu_sapporo_lsm6dsl *sensor, uint8_t reg,
                           uint8_t value)
{
    if (reg == LSM6_CTRL3_C_REG) {
        if ((value & LSM6_CTRL3_C_SW_RESET) != 0u) {
            reset_state(sensor);
        } else {
            sensor->config[reg] = (uint8_t)(value & LSM6_CTRL3_C_WRITE_MASK);
        }
    } else if (is_writable_register(reg)) {
        sensor->config[reg] = value;
    }
}

static uint8_t next_register(uint8_t reg, size_t count, int increment)
{
    if (!increment) {
        return reg;
    }
    return (uint8_t)((uint16_t)reg + (uint16_t)count);
}

static semu_transaction_result transfer(void *context,
                                         semu_serial_transaction *t,
                                         semu_error *error)
{
    semu_sapporo_lsm6dsl *sensor = (semu_sapporo_lsm6dsl *)context;
    uint8_t reg;
    int read;
    int increment;
    size_t i;

    if (sensor == NULL || t == NULL) {
        return refuse(error, "null transaction");
    }
    if (t->chip_select != sensor->chip_select) {
        return refuse(error, "wrong chip select");
    }
    if ((t->tx_size > 0u && t->tx == NULL) ||
        (t->rx_size > 0u && t->rx == NULL) ||
        (t->tx_size == 0u && t->tx != NULL) ||
        (t->rx_size == 0u && t->rx != NULL)) {
        return refuse(error, "null or empty buffer");
    }
    if (t->tx_size == 0u && !sensor->have_command) {
        return refuse(error, "no command");
    }
    if (t->tx_size == 0u) {
        reg = sensor->reg;
        read = sensor->read;
        increment = sensor->increment;
    } else {
        uint8_t command = t->tx[0];
        reg = (uint8_t)(command & LSM6_REGISTER_MASK);
        read = (command & LSM6_READ_BIT) != 0u;
        increment = read || (command & LSM6_INCREMENT_BIT) != 0u;
    }
    if (t->rx_size > 0u) {
        if (t->tx_size > 1u || !read ||
            !span_is_known(reg, t->rx_size, 0, increment)) {
            return refuse(error, "read without read flag");
        }
        for (i = 0u; i < t->rx_size; ++i) {
            t->rx[i] = register_value(sensor, (uint8_t)(reg + i));
        }
        reg = next_register(reg, t->rx_size, increment);
    } else {
        if (t->tx_size == 0u) {
            return refuse(error, "empty phase");
        }
        if (t->tx_size == 1u) {
            if (!is_known_register(reg)) {
                return refuse(error, "unknown command register");
            }
            sensor->reg = reg;
            sensor->read = read;
            sensor->increment = increment;
            sensor->have_command = 1;
            semu_error_clear(error);
            return SEMU_TRANSACTION_OK;
        }
        if (read || !span_is_known(reg, t->tx_size - 1u, 1, increment)) {
            return refuse(error, "write with read flag or unknown register");
        }
        for (i = 1u; i < t->tx_size; ++i) {
            write_register(sensor, (uint8_t)(reg + (increment ? i - 1u : 0u)),
                           t->tx[i]);
        }
        if (t->tx_size > 1u) {
            reg = next_register(reg, t->tx_size - 1u, increment);
        }
    }
    sensor->reg = reg;
    sensor->read = read;
    sensor->increment = increment;
    sensor->have_command = 1;
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

semu_sapporo_lsm6dsl *semu_sapporo_lsm6dsl_create(uint8_t chip_select,
                                                   semu_error *error)
{
    semu_sapporo_lsm6dsl *sensor;
    sensor = (semu_sapporo_lsm6dsl *)calloc(1u, sizeof(*sensor));
    if (sensor == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate LSM6DSL sensor");
        return NULL;
    }
    sensor->chip_select = chip_select;
    reset_state(sensor);
    semu_error_clear(error);
    return sensor;
}

void semu_sapporo_lsm6dsl_destroy(semu_sapporo_lsm6dsl *sensor)
{
    free(sensor);
}

void semu_sapporo_lsm6dsl_reset(void *context)
{
    semu_sapporo_lsm6dsl *sensor = (semu_sapporo_lsm6dsl *)context;
    if (sensor != NULL) {
        reset_state(sensor);
    }
}

semu_serial_endpoint semu_sapporo_lsm6dsl_endpoint(
    semu_sapporo_lsm6dsl *sensor)
{
    semu_serial_endpoint endpoint;
    endpoint.name = sensor != NULL ? "LSM6DSL" : "invalid";
    endpoint.transfer = transfer;
    endpoint.context = sensor;
    return endpoint;
}

semu_status semu_sapporo_lsm6dsl_snapshot_write(
    const semu_sapporo_lsm6dsl *sensor, semu_snapshot_writer *writer,
    semu_error *error)
{
    if (sensor == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "LSM6 snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (semu_snapshot_writer_u8(writer, sensor->chip_select, error) != SEMU_OK ||
        semu_snapshot_writer_bytes(writer, sensor->config,
                                   sizeof(sensor->config), error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, sensor->reg, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)(sensor->have_command != 0), error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)(sensor->read != 0), error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)(sensor->increment != 0), error) != SEMU_OK)
        return error->code;
    return SEMU_OK;
}

semu_status semu_sapporo_lsm6dsl_snapshot_read(
    semu_sapporo_lsm6dsl *sensor, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_sapporo_lsm6dsl candidate;
    uint8_t have_command, read, increment;
    if (sensor == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "LSM6 snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *sensor;
    if (semu_snapshot_reader_u8(reader, &candidate.chip_select, error) != SEMU_OK ||
        semu_snapshot_reader_bytes(reader, candidate.config,
                                   sizeof(candidate.config), error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &candidate.reg, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &have_command, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &read, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &increment, error) != SEMU_OK)
        return error->code;
    if (candidate.chip_select != sensor->chip_select || have_command > 1u ||
        read > 1u || increment > 1u) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "LSM6 snapshot identity/state mismatch");
        return SEMU_ERR_CONFLICT;
    }
    if ((candidate.config[LSM6_CTRL3_C_REG] & LSM6_CTRL3_C_SW_RESET) != 0u) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "LSM6 snapshot has a pending software reset");
        return SEMU_ERR_FORMAT;
    }
    if (have_command == 0u && (read != 0u || increment != 0u)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "LSM6 snapshot has command flags without command");
        return SEMU_ERR_FORMAT;
    }
    candidate.have_command = have_command;
    candidate.read = read;
    candidate.increment = increment;
    *sensor = candidate;
    return SEMU_OK;
}
