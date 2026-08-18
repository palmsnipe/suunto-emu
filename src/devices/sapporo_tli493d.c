#include "sapporo_tli493d.h"

#include <stdlib.h>
#include <string.h>

/*
 * E-SAP-TLI493D-001 verified trace
 * (sapporo-apollo4-tli493d-boundary, sha256
 *  6c9929457a3a83ab8b8432f5d41758ab74738f480a73e89fd523d0f216dd1ad0):
 *   3 IOM2 I2C transactions at address 0x35: M2P config writes (count=2,
 *   mod1 0x11/0x9d, config 0x10/0xa8) and P2M initialization read (count=23),
 *   plus 7 startup responses (status=200).
 *
 * C# model (SapporoTli493dW2bw.cs) protocol:
 *   Write: the observed two-byte transactions select register 0x10 or 0x11
 *          and write one value byte. Pointer bits outside the 0x1f register
 *          address are invalid; they are not silently aliased.
 *   Read:  always starts from register 0, reads count bytes with
 *          auto-increment. The native driver uses seven-byte sample reads
 *          and a 23-byte initialization read.
 *   FinishTransmission: no-op.
 *   Reset: DIAG 0x06=0x44 (power-down bit 6 + frame-valid bit 2).
 *
 * Sample data is zero (synthetic zero-field fixture). Register 23 (0x17)
 * and above refuse. Config writes to 0x10-0x11 are accepted; unknown
 * registers refuse before mutation. No wildcard mask is used.
 */

enum {
    TLI_REG_COUNT = 32u,
    TLI_REGISTER_MASK = 0x1fu,
    TLI_READ_LAST = 0x16u,
    TLI_SAMPLE_READ_SIZE = 7u,
    TLI_INITIALIZATION_READ_SIZE = 23u,
    TLI_DIAG_REG = 0x06u,
    TLI_DIAG_VAL = 0x44u,
    TLI_CONFIG_FIRST = 0x10u,
    TLI_CONFIG_LAST = 0x11u
};

struct semu_sapporo_tli493d {
    uint8_t address;
    uint8_t registers[TLI_REG_COUNT];
};

static int is_readable(uint8_t reg)
{
    switch (reg) {
    case 0x00u:
    case 0x01u:
    case 0x02u:
    case 0x03u:
    case 0x04u:
    case 0x05u:
    case 0x06u:
    case 0x07u:
    case 0x08u:
    case 0x09u:
    case 0x0au:
    case 0x0bu:
    case 0x0cu:
    case 0x0du:
    case 0x0eu:
    case 0x0fu:
    case 0x10u:
    case 0x11u:
    case 0x12u:
    case 0x13u:
    case 0x14u:
    case 0x15u:
    case TLI_READ_LAST:
        return 1;
    default:
        return 0;
    }
}

static int is_writable(uint8_t reg)
{
    return reg >= TLI_CONFIG_FIRST && reg <= TLI_CONFIG_LAST;
}

static int is_supported_read_size(size_t size)
{
    return size == TLI_SAMPLE_READ_SIZE ||
           size == TLI_INITIALIZATION_READ_SIZE;
}

static semu_transaction_result refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "TLI493D refuses %s", reason);
    return SEMU_TRANSACTION_REFUSE;
}

static void reset_state(semu_sapporo_tli493d *sensor)
{
    memset(sensor->registers, 0, sizeof(sensor->registers));
    sensor->registers[TLI_DIAG_REG] = TLI_DIAG_VAL;
}

static semu_transaction_result transfer(void *context,
                                         semu_serial_transaction *t,
                                         semu_error *error)
{
    semu_sapporo_tli493d *sensor = (semu_sapporo_tli493d *)context;
    size_t i;

    if (sensor == NULL || t == NULL) {
        return refuse(error, "null transaction");
    }
    if (t->address != sensor->address) {
        return refuse(error, "wrong address");
    }
    if (t->rx_size > 0u) {
        if (t->tx != NULL || t->tx_size != 0u || t->rx == NULL ||
            !is_supported_read_size(t->rx_size)) {
            return refuse(error, "unsupported read shape");
        }
        for (i = 0u; i < t->rx_size; ++i) {
            if (!is_readable((uint8_t)i)) {
                return refuse(error, "read boundary");
            }
        }
        for (i = 0u; i < t->rx_size; ++i) {
            t->rx[i] = sensor->registers[i];
        }
    } else {
        uint8_t reg;
        if (t->rx != NULL || t->tx == NULL || t->tx_size != 2u) {
            return refuse(error, "unsupported write shape");
        }
        if ((t->tx[0] & (uint8_t)~TLI_REGISTER_MASK) != 0u) {
            return refuse(error, "invalid register pointer");
        }
        reg = t->tx[0];
        if (!is_writable(reg)) {
            return refuse(error, "unknown write register");
        }
        /* The complete two-byte frame was validated before this mutation. */
        sensor->registers[reg] = t->tx[1];
    }
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

semu_sapporo_tli493d *semu_sapporo_tli493d_create(uint8_t address,
                                                   semu_error *error)
{
    semu_sapporo_tli493d *sensor;
    sensor = (semu_sapporo_tli493d *)calloc(1u, sizeof(*sensor));
    if (sensor == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate TLI493D sensor");
        return NULL;
    }
    sensor->address = address;
    reset_state(sensor);
    semu_error_clear(error);
    return sensor;
}

void semu_sapporo_tli493d_destroy(semu_sapporo_tli493d *sensor)
{
    free(sensor);
}

void semu_sapporo_tli493d_reset(void *context)
{
    semu_sapporo_tli493d *sensor = (semu_sapporo_tli493d *)context;
    if (sensor != NULL) {
        reset_state(sensor);
    }
}

semu_serial_endpoint semu_sapporo_tli493d_endpoint(
    semu_sapporo_tli493d *sensor)
{
    semu_serial_endpoint endpoint;
    endpoint.name = sensor != NULL ? "TLI493D" : "invalid";
    endpoint.transfer = transfer;
    endpoint.context = sensor;
    return endpoint;
}
