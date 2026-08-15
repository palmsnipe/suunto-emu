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
 * via two-phase held-CS SPI. Sample data is zero (synthetic).
 * Unknown register, wrong chip-select, wrong direction, and overflow
 * refuse before mutation. No wildcard readable/writable mask is used.
 */

enum {
    LSM6_REG_COUNT = 128u,
    LSM6_READ_BIT = 0x80u,
    LSM6_INCREMENT_BIT = 0x40u,
    LSM6_REGISTER_MASK = 0x3fu,
    LSM6_WHO_AM_I_REG = 0x0fu,
    LSM6_WHO_AM_I_VAL = 0x6au,
    LSM6_FIFO_STATUS1_REG = 0x3au,
    LSM6_FIFO_STATUS1_VAL = 0x00u,
    LSM6_FIFO_STATUS2_REG = 0x3bu,
    LSM6_FIFO_EMPTY = 0x10u
};

struct semu_sapporo_lsm6dsl {
    uint8_t chip_select;
    uint8_t registers[LSM6_REG_COUNT];
    uint8_t reg;
    int have_command;
    int read;
    int increment;
};

static int is_known_register(uint8_t reg)
{
    return reg == LSM6_WHO_AM_I_REG ||
           reg == LSM6_FIFO_STATUS1_REG ||
           reg == LSM6_FIFO_STATUS2_REG;
}

static semu_transaction_result refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "LSM6DSL refuses %s", reason);
    return SEMU_TRANSACTION_REFUSE;
}

static void reset_state(semu_sapporo_lsm6dsl *sensor)
{
    memset(sensor->registers, 0, sizeof(sensor->registers));
    sensor->registers[LSM6_WHO_AM_I_REG] = LSM6_WHO_AM_I_VAL;
    sensor->registers[LSM6_FIFO_STATUS2_REG] = LSM6_FIFO_EMPTY;
    sensor->reg = 0u;
    sensor->have_command = 0;
    sensor->read = 0;
    sensor->increment = 0;
}

static void decode_command(semu_sapporo_lsm6dsl *sensor, uint8_t cmd)
{
    sensor->read = (cmd & LSM6_READ_BIT) != 0u;
    sensor->increment = sensor->read || (cmd & LSM6_INCREMENT_BIT) != 0u;
    sensor->reg = (uint8_t)(cmd & LSM6_REGISTER_MASK);
    sensor->have_command = 1;
}

static semu_transaction_result transfer(void *context,
                                         semu_serial_transaction *t,
                                         semu_error *error)
{
    semu_sapporo_lsm6dsl *sensor = (semu_sapporo_lsm6dsl *)context;
    size_t i;

    if (sensor == NULL || t == NULL) {
        return refuse(error, "null transaction");
    }
    if (t->chip_select != sensor->chip_select) {
        return refuse(error, "wrong chip select");
    }
    if (t->tx_size == 0u && !sensor->have_command) {
        return refuse(error, "no command");
    }
    if (t->tx_size > 0u) {
        decode_command(sensor, t->tx[0]);
    }
    if (t->rx_size > 0u) {
        if (!sensor->read) {
            return refuse(error, "read without read flag");
        }
        for (i = 0u; i < t->rx_size; ++i) {
            if (!is_known_register(sensor->reg)) {
                return refuse(error, "unknown read register");
            }
            t->rx[i] = sensor->registers[sensor->reg];
            if (sensor->increment) {
                sensor->reg = (uint8_t)((sensor->reg + 1u) & LSM6_REGISTER_MASK);
            }
        }
    } else if (t->tx_size > 1u) {
        if (sensor->read) {
            return refuse(error, "write with read flag");
        }
        for (i = 1u; i < t->tx_size; ++i) {
            if (!is_known_register(sensor->reg)) {
                return refuse(error, "unknown write register");
            }
            sensor->registers[sensor->reg] = t->tx[i];
            if (sensor->increment) {
                sensor->reg = (uint8_t)((sensor->reg + 1u) & LSM6_REGISTER_MASK);
            }
        }
    }
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
