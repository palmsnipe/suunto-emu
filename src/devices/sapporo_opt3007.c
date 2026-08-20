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
 * The public OPT3007 datasheet defines six 16-bit registers. This model keeps
 * the result deterministic at zero lux and defers conversion timing, but
 * implements the documented register map, power-on values, read-only IDs, and
 * configuration write mask. All register data is big-endian on the wire.
 *
 * The native Sapporo path uses only config 0x01 and result 0x00. The remaining
 * registers are included for hardware-compatible inspection and are not used
 * as an unobserved startup requirement.
 */

enum {
    OPT_RESULT_PTR = 0x00u,
    OPT_CONFIG_PTR = 0x01u,
    OPT_LOW_LIMIT_PTR = 0x02u,
    OPT_HIGH_LIMIT_PTR = 0x03u,
    OPT_MANUFACTURER_ID_PTR = 0x7eu,
    OPT_DEVICE_ID_PTR = 0x7fu,
    OPT_REG_SIZE = 2u
};

#define OPT_CONFIG_RESET 0xc810u
#define OPT_LOW_LIMIT_RESET 0xc000u
#define OPT_HIGH_LIMIT_RESET 0xbfffu
#define OPT_MANUFACTURER_ID 0x5449u
#define OPT_DEVICE_ID 0x3001u

/* RN[3:0], CT, M[1:0], POL, ME, and FC[1:0] are writable. */
#define OPT_CONFIG_WRITE_MASK 0xfe0fu

struct semu_sapporo_opt3007 {
    uint8_t address;
    uint16_t result;
    uint16_t config;
    uint16_t low_limit;
    uint16_t high_limit;
};

static int is_known_pointer(uint8_t ptr)
{
    return ptr == OPT_RESULT_PTR || ptr == OPT_CONFIG_PTR ||
           ptr == OPT_LOW_LIMIT_PTR || ptr == OPT_HIGH_LIMIT_PTR ||
           ptr == OPT_MANUFACTURER_ID_PTR || ptr == OPT_DEVICE_ID_PTR;
}

static int is_writable_pointer(uint8_t ptr)
{
    return ptr == OPT_CONFIG_PTR || ptr == OPT_LOW_LIMIT_PTR ||
           ptr == OPT_HIGH_LIMIT_PTR;
}

static semu_transaction_result refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "OPT3007 refuses %s", reason);
    return SEMU_TRANSACTION_REFUSE;
}

static void reset_state(semu_sapporo_opt3007 *sensor)
{
    sensor->result = 0u;
    sensor->config = OPT_CONFIG_RESET;
    sensor->low_limit = OPT_LOW_LIMIT_RESET;
    sensor->high_limit = OPT_HIGH_LIMIT_RESET;
}

static uint16_t register_value(const semu_sapporo_opt3007 *sensor,
                               uint8_t ptr)
{
    switch (ptr) {
    case OPT_RESULT_PTR:
        return sensor->result;
    case OPT_CONFIG_PTR:
        return sensor->config;
    case OPT_LOW_LIMIT_PTR:
        return sensor->low_limit;
    case OPT_HIGH_LIMIT_PTR:
        return sensor->high_limit;
    case OPT_MANUFACTURER_ID_PTR:
        return OPT_MANUFACTURER_ID;
    case OPT_DEVICE_ID_PTR:
        return OPT_DEVICE_ID;
    default:
        return 0u;
    }
}

static void write_be16(uint8_t *bytes, uint16_t value)
{
    bytes[0u] = (uint8_t)(value >> 8u);
    bytes[1u] = (uint8_t)value;
}

static semu_transaction_result transfer(void *context,
                                         semu_serial_transaction *t,
                                         semu_error *error)
{
    semu_sapporo_opt3007 *sensor = (semu_sapporo_opt3007 *)context;
    uint8_t ptr;
    uint16_t value;

    if (sensor == NULL || t == NULL) {
        return refuse(error, "null transaction");
    }
    if (t->address != sensor->address) {
        return refuse(error, "wrong address");
    }
    if (t->tx == NULL || t->tx_size == 0u) {
        return refuse(error, "missing register pointer");
    }
    ptr = t->tx[0];
    if (!is_known_pointer(ptr)) {
        return refuse(error, "unknown pointer");
    }
    if (t->rx_size > 0u) {
        if (t->tx_size != 1u || t->rx_size != OPT_REG_SIZE ||
            t->rx == NULL) {
            return refuse(error, "read requires one pointer and two bytes");
        }
        value = register_value(sensor, ptr);
        write_be16(t->rx, value);
    } else {
        if (t->rx != NULL || t->tx_size != 1u + OPT_REG_SIZE) {
            return refuse(error, "write requires one pointer and two bytes");
        }
        if (!is_writable_pointer(ptr)) {
            return refuse(error, "register is read-only");
        }
        value = (uint16_t)(((uint16_t)t->tx[1u] << 8u) | t->tx[2u]);
        switch (ptr) {
        case OPT_CONFIG_PTR:
            sensor->config = (uint16_t)((sensor->config &
                                         (uint16_t)~OPT_CONFIG_WRITE_MASK) |
                                        (value & OPT_CONFIG_WRITE_MASK));
            break;
        case OPT_LOW_LIMIT_PTR:
            sensor->low_limit = value;
            break;
        case OPT_HIGH_LIMIT_PTR:
            sensor->high_limit = value;
            break;
        default:
            return refuse(error, "register is not writable");
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

semu_status semu_sapporo_opt3007_snapshot_write(
    const semu_sapporo_opt3007 *sensor, semu_snapshot_writer *writer,
    semu_error *error)
{
    if (sensor == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "OPT3007 snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (semu_snapshot_writer_u8(writer, sensor->address, error) != SEMU_OK ||
        semu_snapshot_writer_u16(writer, sensor->result, error) != SEMU_OK ||
        semu_snapshot_writer_u16(writer, sensor->config, error) != SEMU_OK ||
        semu_snapshot_writer_u16(writer, sensor->low_limit, error) != SEMU_OK ||
        semu_snapshot_writer_u16(writer, sensor->high_limit, error) != SEMU_OK)
        return error->code;
    return SEMU_OK;
}

semu_status semu_sapporo_opt3007_snapshot_read(
    semu_sapporo_opt3007 *sensor, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_sapporo_opt3007 candidate;
    if (sensor == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "OPT3007 snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *sensor;
    if (semu_snapshot_reader_u8(reader, &candidate.address, error) != SEMU_OK ||
        semu_snapshot_reader_u16(reader, &candidate.result, error) != SEMU_OK ||
        semu_snapshot_reader_u16(reader, &candidate.config, error) != SEMU_OK ||
        semu_snapshot_reader_u16(reader, &candidate.low_limit, error) != SEMU_OK ||
        semu_snapshot_reader_u16(reader, &candidate.high_limit, error) != SEMU_OK)
        return error->code;
    if (candidate.address != sensor->address) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "OPT3007 snapshot identity mismatch");
        return SEMU_ERR_CONFLICT;
    }
    if (candidate.result != 0u ||
        (candidate.config & (uint16_t)~OPT_CONFIG_WRITE_MASK) !=
            (OPT_CONFIG_RESET & (uint16_t)~OPT_CONFIG_WRITE_MASK)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "invalid OPT3007 deterministic state");
        return SEMU_ERR_FORMAT;
    }
    *sensor = candidate;
    return SEMU_OK;
}
