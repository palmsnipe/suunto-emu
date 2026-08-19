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
 *   Read:  reads from selected register with auto-increment; a repeated-start
 *          read may omit the selector and retain the previous register.
 *   FinishTransmission: keeps selected register across repeated-start.
 *   Reset: identity 0x00=0x49, ready 0x03=0x11, variant 0x1c=0xe0.
 *
 * The Alps HSPPAD143A register map defines the explicit identity, status,
 * output, control, and product-number fields below. Pressure output is a
 * synthetic zero fixture; no live conversion or FIFO engine is claimed.
 * Unobserved action-command registers and reserved gaps refuse.
 */

enum {
    HSPPAD_IDENTITY_REG = 0x00u,
    HSPPAD_IDENTITY_VAL = 0x49u,
    HSPPAD_INFO_REG = 0x01u,
    HSPPAD_INFO_VAL = 0x31u,
    HSPPAD_FIFO_STATUS_REG = 0x02u,
    HSPPAD_READY_REG = 0x03u,
    HSPPAD_READY_VAL = 0x11u,
    HSPPAD_PRESSURE_LOW_REG = 0x04u,
    HSPPAD_PRESSURE_MIDDLE_REG = 0x05u,
    HSPPAD_PRESSURE_HIGH_REG = 0x06u,
    HSPPAD_TEMPERATURE_LOW_REG = 0x09u,
    HSPPAD_TEMPERATURE_HIGH_REG = 0x0au,
    HSPPAD_CTL1_REG = 0x0eu,
    HSPPAD_CTL2_REG = 0x0fu,
    HSPPAD_ACTION1_REG = 0x10u,
    HSPPAD_ACTION2_REG = 0x11u,
    HSPPAD_FIFO_CONTROL_REG = 0x12u,
    HSPPAD_AVERAGE_CONTROL_REG = 0x13u,
    HSPPAD_VARIANT_REG = 0x1cu,
    HSPPAD_VARIANT_VAL = 0xe0u,
    HSPPAD_CTL1_RESET = 0x13u,
    HSPPAD_CTL2_RESET = 0xa0u,
    HSPPAD_ACTION1_RESET = 0x00u,
    HSPPAD_ACTION2_RESET = 0x00u,
    HSPPAD_FIFO_CONTROL_RESET = 0x10u,
    HSPPAD_AVERAGE_CONTROL_RESET = 0x38u,
    HSPPAD_CTL1_WRITE_MASK = 0x03u,
    HSPPAD_CTL2_WRITE_MASK = 0xafu,
    HSPPAD_ACTION1_WRITE_MASK = 0x0au,
    HSPPAD_ACTION2_WRITE_MASK = 0x80u,
    HSPPAD_FIFO_CONTROL_WRITE_MASK = 0x9fu,
    HSPPAD_AVERAGE_CONTROL_WRITE_MASK = 0x3fu,
    HSPPAD_WRITABLE_FIRST = HSPPAD_CTL1_REG,
    HSPPAD_WRITABLE_COUNT = 6u
};

struct semu_sapporo_hsppad143 {
    uint8_t address;
    uint8_t selected;
    uint8_t status;
    uint8_t writable[HSPPAD_WRITABLE_COUNT];
};

static const uint8_t HSPPAD_REGISTER_RESET[HSPPAD_WRITABLE_COUNT] = {
    HSPPAD_CTL1_RESET, HSPPAD_CTL2_RESET, HSPPAD_ACTION1_RESET,
    HSPPAD_ACTION2_RESET, HSPPAD_FIFO_CONTROL_RESET,
    HSPPAD_AVERAGE_CONTROL_RESET
};

static const uint8_t HSPPAD_REGISTER_MASK[HSPPAD_WRITABLE_COUNT] = {
    HSPPAD_CTL1_WRITE_MASK, HSPPAD_CTL2_WRITE_MASK,
    HSPPAD_ACTION1_WRITE_MASK, HSPPAD_ACTION2_WRITE_MASK,
    HSPPAD_FIFO_CONTROL_WRITE_MASK, HSPPAD_AVERAGE_CONTROL_WRITE_MASK
};

static semu_transaction_result refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "HSPPAD143 refuses %s", reason);
    return SEMU_TRANSACTION_REFUSE;
}

static void reset_state(semu_sapporo_hsppad143 *sensor)
{
    sensor->selected = 0u;
    sensor->status = HSPPAD_READY_VAL;
    memcpy(sensor->writable, HSPPAD_REGISTER_RESET,
           sizeof(sensor->writable));
}

static int is_supported_register(uint8_t reg)
{
    static const uint8_t registers[] = {
        HSPPAD_IDENTITY_REG, HSPPAD_INFO_REG, HSPPAD_FIFO_STATUS_REG,
        HSPPAD_READY_REG, HSPPAD_PRESSURE_LOW_REG,
        HSPPAD_PRESSURE_MIDDLE_REG, HSPPAD_PRESSURE_HIGH_REG,
        HSPPAD_TEMPERATURE_LOW_REG, HSPPAD_TEMPERATURE_HIGH_REG,
        HSPPAD_CTL1_REG, HSPPAD_CTL2_REG, HSPPAD_ACTION1_REG,
        HSPPAD_ACTION2_REG, HSPPAD_FIFO_CONTROL_REG,
        HSPPAD_AVERAGE_CONTROL_REG, HSPPAD_VARIANT_REG
    };
    size_t i;

    for (i = 0u; i < sizeof(registers) / sizeof(registers[0]); ++i) {
        if (registers[i] == reg) {
            return 1;
        }
    }
    return 0;
}

static int is_writable_register(uint8_t reg)
{
    return reg >= HSPPAD_WRITABLE_FIRST &&
           reg < HSPPAD_WRITABLE_FIRST + HSPPAD_WRITABLE_COUNT;
}

static uint8_t register_value(const semu_sapporo_hsppad143 *sensor,
                              uint8_t reg)
{
    if (is_writable_register(reg)) {
        return sensor->writable[reg - HSPPAD_WRITABLE_FIRST];
    }
    switch (reg) {
    case HSPPAD_IDENTITY_REG:
        return HSPPAD_IDENTITY_VAL;
    case HSPPAD_INFO_REG:
        return HSPPAD_INFO_VAL;
    case HSPPAD_FIFO_STATUS_REG:
        return 0u;
    case HSPPAD_READY_REG:
        return sensor->status;
    case HSPPAD_PRESSURE_LOW_REG:
    case HSPPAD_PRESSURE_MIDDLE_REG:
    case HSPPAD_PRESSURE_HIGH_REG:
        return 0u;
    case HSPPAD_TEMPERATURE_LOW_REG:
        return 0u;
    case HSPPAD_TEMPERATURE_HIGH_REG:
        return 0x19u;
    case HSPPAD_VARIANT_REG:
        return HSPPAD_VARIANT_VAL;
    default:
        return 0u;
    }
}

static uint8_t write_mask(uint8_t reg)
{
    if (!is_writable_register(reg)) {
        return 0u;
    }
    return HSPPAD_REGISTER_MASK[reg - HSPPAD_WRITABLE_FIRST];
}

static void write_register(semu_sapporo_hsppad143 *sensor, uint8_t reg,
                           uint8_t value)
{
    if (is_writable_register(reg)) {
        uint8_t mask = write_mask(reg);
        uint8_t old_value = sensor->writable[reg - HSPPAD_WRITABLE_FIRST];
        sensor->writable[reg - HSPPAD_WRITABLE_FIRST] =
            (uint8_t)((old_value & (uint8_t)~mask) | (value & mask));
    }
}

static int span_is_valid(uint8_t start, size_t count, int writable)
{
    size_t i;
    if (count == 0u || count > 256u) {
        return 0;
    }
    for (i = 0u; i < count; ++i) {
        uint16_t index = (uint16_t)start + (uint16_t)i;
        if (index > 0xffu ||
            !is_supported_register((uint8_t)index) ||
            (writable && !is_writable_register((uint8_t)index))) {
            return 0;
        }
    }
    return 1;
}

static semu_transaction_result handle_read(semu_sapporo_hsppad143 *sensor,
                                           semu_serial_transaction *t,
                                           semu_error *error)
{
    uint8_t start = sensor->selected;
    size_t i;
    if (t->tx_size > 0u) {
        if (t->tx_size != 1u || t->tx == NULL) {
            return refuse(error, "read selector shape");
        }
        start = t->tx[0];
    }
    if (t->rx == NULL || !span_is_valid(start, t->rx_size, 0)) {
        return refuse(error, "unsupported read span");
    }
    for (i = 0u; i < t->rx_size; ++i) {
        t->rx[i] = register_value(sensor, (uint8_t)(start + i));
    }
    sensor->selected = (uint8_t)(start + t->rx_size);
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static semu_transaction_result handle_write(semu_sapporo_hsppad143 *sensor,
                                             semu_serial_transaction *t,
                                             semu_error *error)
{
    uint8_t start;
    size_t i;
    if (t->tx == NULL || t->tx_size == 0u) {
        return refuse(error, "empty write");
    }
    start = t->tx[0];
    if (!is_supported_register(start)) {
        return refuse(error, "unknown register selector");
    }
    if (t->tx_size == 1u) {
        sensor->selected = start;
    } else {
        if (!span_is_valid(start, t->tx_size - 1u, 1)) {
            return refuse(error, "unsupported write span");
        }
        /* Validate the whole span before changing any register. */
        for (i = 1u; i < t->tx_size; ++i) {
            write_register(sensor, (uint8_t)(start + i - 1u), t->tx[i]);
        }
        sensor->selected = (uint8_t)(start + t->tx_size - 1u);
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
    if (t->rx != NULL && t->rx_size == 0u) {
        return refuse(error, "write has receive buffer");
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

semu_status semu_sapporo_hsppad143_snapshot_write(
    const semu_sapporo_hsppad143 *sensor, semu_snapshot_writer *writer,
    semu_error *error)
{
    if (sensor == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "HSPPAD snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (semu_snapshot_writer_u8(writer, sensor->address, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, sensor->selected, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, sensor->status, error) != SEMU_OK ||
        semu_snapshot_writer_bytes(writer, sensor->writable,
                                   sizeof(sensor->writable), error) != SEMU_OK)
        return error->code;
    return SEMU_OK;
}

semu_status semu_sapporo_hsppad143_snapshot_read(
    semu_sapporo_hsppad143 *sensor, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_sapporo_hsppad143 candidate;
    if (sensor == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "HSPPAD snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *sensor;
    if (semu_snapshot_reader_u8(reader, &candidate.address, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &candidate.selected, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &candidate.status, error) != SEMU_OK ||
        semu_snapshot_reader_bytes(reader, candidate.writable,
                                   sizeof(candidate.writable), error) != SEMU_OK)
        return error->code;
    if (candidate.address != sensor->address) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "HSPPAD snapshot identity mismatch");
        return SEMU_ERR_CONFLICT;
    }
    *sensor = candidate;
    return SEMU_OK;
}
