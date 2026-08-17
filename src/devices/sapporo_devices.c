/*
 * Sapporo 2.22 device factory (ticket 420).
 * Owns all verified peripheral instances and exposes endpoints that
 * machine.c attaches to Apollo4 IOM/MSPI/UART controllers.  Multi-device
 * I2C buses (IOM2, IOM4) are multiplexed by I2C address.
 */

#include "sapporo_devices.h"

#include "sapporo_cxd5610.h"
#include "sapporo_flash.h"
#include "sapporo_hsppad143.h"
#include "sapporo_haptic.h"
#include "sapporo_lsm6dsl.h"
#include "sapporo_max17050.h"
#include "sapporo_ohr2.h"
#include "sapporo_opt3007.h"
#include "sapporo_tli493d.h"
#include "sapporo_gps_compat.h"
#include "../compat/sapporo_222.h"
#include "../soc/apollo4/apollo4_internal.h"

#include <stdlib.h>
#include <string.h>

#define SEMU_SAPPORO_MAX_I2C_CHILDREN 4u

typedef struct {
    uint8_t address;
    semu_serial_endpoint endpoint;
} i2c_mux_entry;

typedef struct {
    i2c_mux_entry entries[SEMU_SAPPORO_MAX_I2C_CHILDREN];
    size_t count;
} i2c_bus;

struct semu_sapporo_devices {
    semu_scheduler *scheduler;
    semu_apollo4 *soc;
    semu_sapporo_hsppad143 *pressure;
    semu_sapporo_lsm6dsl *accelerometer;
    semu_sapporo_tli493d *magnetometer;
    semu_sapporo_haptic *haptic;
    semu_sapporo_opt3007 *ambient_light;
    semu_sapporo_max17050 *battery_gauge;
    semu_sapporo_cxd5610 *gps;
    semu_sapporo_ohr2 *ohr2;
    semu_sapporo_flash *flash;
    i2c_bus iom2_bus;
    i2c_bus iom4_bus;
    semu_serial_endpoint iom0_ep;
    semu_serial_endpoint iom2_ep;
    semu_serial_endpoint iom3_ep;
    semu_serial_endpoint iom4_ep;
    semu_serial_endpoint iom4_observed_ep;
    semu_serial_endpoint mspi1_ep;
    semu_serial_endpoint mspi_ep;
    semu_serial_endpoint refuse_ep;
    semu_apollo4_uart_endpoint uart_ep;
    semu_sapporo_222_fixture_context fixture_context;
};

/* E-SAP-OHR2-001: OHR2 ready is wired to the firmware's GPIO 62 input. */
static void ohr_ready_signal(void *context, unsigned signal, int level)
{
    semu_sapporo_devices *devices = (semu_sapporo_devices *)context;
    semu_error error;

    if (devices == NULL || devices->soc == NULL ||
        signal != SEMU_SAPPORO_OHR2_READY_SIGNAL) {
        return;
    }
    semu_error_clear(&error);
    (void)semu_apollo4_set_gpio_input(devices->soc, 62u, level, &error);
}

/* E-A4-IOM-001 records the first IOM4 target as an unidentified 0x28
 * endpoint.  The Renode boundary model accepts the observed 4-byte writes and
 * 1-byte reads and returns zeroes; no device identity is inferred here. */
static semu_transaction_result observed_iom4_transfer(
    void *context, semu_serial_transaction *transaction, semu_error *error)
{
    size_t i;
    (void)context;
    if (transaction == NULL ||
        (transaction->tx_size != 4u && transaction->rx_size != 1u) ||
        (transaction->tx_size != 0u && transaction->rx_size != 0u)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "IOM4 observed 0x28 transaction shape is unsupported");
        return SEMU_TRANSACTION_REFUSE;
    }
    if (transaction->rx_size != 0u && transaction->rx == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "IOM4 observed response buffer is required");
        return SEMU_TRANSACTION_REFUSE;
    }
    for (i = 0u; i < transaction->rx_size; ++i) {
        transaction->rx[i] = 0u;
    }
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static semu_transaction_result refuse_transfer(
    void *context, semu_serial_transaction *transaction, semu_error *error)
{
    (void)context;
    (void)transaction;
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "no verified device at this IOM instance");
    return SEMU_TRANSACTION_REFUSE;
}

/* E-SAP-PANEL-001 is still missing.  MSPI1 nevertheless has an evidenced
 * DIAP4 completion boundary: the controller stages a 16-byte descriptor
 * head, writes control=3, and receives IRQ21 bit 9.  Accept that completion
 * only; do not interpret it as a panel transfer or fabricate pixels. */
static semu_transaction_result mspi1_completion_transfer(
    void *context, semu_serial_transaction *transaction, semu_error *error)
{
    (void)context;
    if (transaction == NULL || transaction->tx == NULL ||
        transaction->tx_size != 16u || transaction->rx != NULL ||
        transaction->rx_size != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "MSPI1 DIAP4 completion descriptor shape is unsupported");
        return SEMU_TRANSACTION_REFUSE;
    }
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static semu_transaction_result i2c_mux_transfer(
    void *context, semu_serial_transaction *transaction, semu_error *error)
{
    i2c_bus *bus = (i2c_bus *)context;
    size_t i;
    if (bus == NULL || transaction == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "I2C mux null argument");
        return SEMU_TRANSACTION_REFUSE;
    }
    for (i = 0u; i < bus->count; ++i) {
        if (bus->entries[i].address == transaction->address) {
            return bus->entries[i].endpoint.transfer(
                bus->entries[i].endpoint.context, transaction, error);
        }
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "I2C address 0x%02x has no verified device",
                   (unsigned)transaction->address);
    return SEMU_TRANSACTION_REFUSE;
}

static semu_transaction_result no_device_ohr_provider(
    void *context, semu_sapporo_ohr2_command command, uint16_t sequence,
    semu_sapporo_ohr2_state state,
    const uint8_t request_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    uint8_t response_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    semu_error *error)
{
    semu_sapporo_222_fixture_context *fixture =
        (semu_sapporo_222_fixture_context *)context;
    if (fixture == NULL || fixture->state == NULL ||
        fixture->logger == NULL || !fixture->state->enabled) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "OHR2 compatibility fixture is disabled");
        return SEMU_TRANSACTION_REFUSE;
    }
    return semu_sapporo_222_ohr_body_provider(
        fixture, command, sequence, state, request_payload,
        response_payload, error);
}

static semu_transaction_result uart_bridge_transmit(
    void *context, uint8_t value, semu_error *error)
{
    semu_sapporo_cxd5610 *gps = (semu_sapporo_cxd5610 *)context;
    semu_serial_endpoint ep;
    semu_serial_transaction txn;
    if (gps == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "UART bridge null context");
        return SEMU_TRANSACTION_REFUSE;
    }
    ep = semu_sapporo_cxd5610_endpoint(gps);
    memset(&txn, 0, sizeof(txn));
    txn.tx = &value;
    txn.tx_size = 1u;
    return ep.transfer(ep.context, &txn, error);
}

static void uart_bridge_receive(void *context, uint8_t value,
                                uint64_t virtual_time_ns)
{
    semu_apollo4_uart *uart = (semu_apollo4_uart *)context;
    semu_error error;
    (void)virtual_time_ns;
    semu_error_clear(&error);
    (void)semu_apollo4_uart_schedule_rx(uart, 0u, &value, 1u, &error);
}

static void i2c_bus_attach(i2c_bus *bus, uint8_t address,
                            const semu_serial_endpoint *endpoint)
{
    if (bus->count < SEMU_SAPPORO_MAX_I2C_CHILDREN) {
        bus->entries[bus->count].address = address;
        bus->entries[bus->count].endpoint = *endpoint;
        ++bus->count;
    }
}

semu_sapporo_devices *semu_sapporo_devices_create(
    semu_scheduler *scheduler, const semu_storage *flash_storage,
    semu_error *error)
{
    semu_sapporo_devices *devices;
    if (scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "sapporo devices require a scheduler");
        return NULL;
    }
    devices = (semu_sapporo_devices *)calloc(1u, sizeof(*devices));
    if (devices == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate sapporo devices");
        return NULL;
    }
    devices->scheduler = scheduler;

    devices->pressure = semu_sapporo_hsppad143_create(0x48u, error);
    if (devices->pressure == NULL) goto fail;
    devices->accelerometer = semu_sapporo_lsm6dsl_create(0u, error);
    if (devices->accelerometer == NULL) goto fail;
    devices->magnetometer = semu_sapporo_tli493d_create(0x35u, error);
    if (devices->magnetometer == NULL) goto fail;
    devices->haptic = semu_sapporo_haptic_create(0x50u, error);
    if (devices->haptic == NULL) goto fail;
    devices->ambient_light = semu_sapporo_opt3007_create(0x45u, error);
    if (devices->ambient_light == NULL) goto fail;
    devices->battery_gauge = semu_sapporo_max17050_create(0x36u, error);
    if (devices->battery_gauge == NULL) goto fail;
    devices->gps = semu_sapporo_cxd5610_create(
        scheduler, NULL, NULL, NULL, NULL, NULL, NULL, error);
    if (devices->gps == NULL) goto fail;
    devices->ohr2 = semu_sapporo_ohr2_create(
        ohr_ready_signal, devices, no_device_ohr_provider,
        &devices->fixture_context, error);
    if (devices->ohr2 == NULL) goto fail;
    if (flash_storage != NULL) {
        devices->flash = semu_sapporo_flash_create(
            flash_storage, UINT32_C(0x02000000), UINT32_C(0x1000),
            UINT32_C(0x100), error);
        if (devices->flash == NULL) goto fail;
    }

    {
        semu_serial_endpoint ep;
        ep = semu_sapporo_hsppad143_endpoint(devices->pressure);
        i2c_bus_attach(&devices->iom2_bus, 0x48u, &ep);
        ep = semu_sapporo_tli493d_endpoint(devices->magnetometer);
        i2c_bus_attach(&devices->iom2_bus, 0x35u, &ep);
        ep = semu_sapporo_ohr2_endpoint(devices->ohr2);
        i2c_bus_attach(&devices->iom2_bus, 0x10u, &ep);
        ep = semu_sapporo_haptic_endpoint(devices->haptic);
        i2c_bus_attach(&devices->iom4_bus, 0x50u, &ep);
        ep = semu_sapporo_max17050_endpoint(devices->battery_gauge);
        i2c_bus_attach(&devices->iom4_bus, 0x36u, &ep);
    }

    devices->iom0_ep = semu_sapporo_lsm6dsl_endpoint(devices->accelerometer);
    devices->iom2_ep.name = "sapporo.iom2";
    devices->iom2_ep.transfer = i2c_mux_transfer;
    devices->iom2_ep.context = &devices->iom2_bus;
    devices->iom3_ep = semu_sapporo_opt3007_endpoint(devices->ambient_light);
    devices->iom4_ep.name = "sapporo.iom4";
    devices->iom4_ep.transfer = i2c_mux_transfer;
    devices->iom4_ep.context = &devices->iom4_bus;
    devices->iom4_observed_ep.name = "sapporo.iom4.observed-0x28";
    devices->iom4_observed_ep.transfer = observed_iom4_transfer;
    devices->iom4_observed_ep.context = NULL;
    i2c_bus_attach(&devices->iom4_bus, 0x28u, &devices->iom4_observed_ep);
    devices->mspi1_ep.name = "sapporo.mspi1.diap4-completion";
    devices->mspi1_ep.transfer = mspi1_completion_transfer;
    devices->mspi1_ep.context = NULL;
    if (devices->flash != NULL) {
        devices->mspi_ep = semu_sapporo_flash_endpoint(devices->flash);
    } else {
        devices->mspi_ep.name = "sapporo.mspi2.refuse";
        devices->mspi_ep.transfer = refuse_transfer;
        devices->mspi_ep.context = NULL;
    }
    devices->refuse_ep.name = "sapporo.refuse";
    devices->refuse_ep.transfer = refuse_transfer;
    devices->refuse_ep.context = NULL;
    devices->uart_ep.name = "sapporo.gps";
    devices->uart_ep.transmit = uart_bridge_transmit;
    devices->uart_ep.context = devices->gps;

    semu_error_clear(error);
    return devices;

fail:
    semu_sapporo_devices_destroy(devices);
    return NULL;
}

void semu_sapporo_devices_destroy(semu_sapporo_devices *devices)
{
    if (devices == NULL) return;
    semu_sapporo_ohr2_destroy(devices->ohr2);
    semu_sapporo_flash_destroy(devices->flash);
    semu_sapporo_cxd5610_destroy(devices->gps);
    semu_sapporo_max17050_destroy(devices->battery_gauge);
    semu_sapporo_opt3007_destroy(devices->ambient_light);
    semu_sapporo_haptic_destroy(devices->haptic);
    semu_sapporo_tli493d_destroy(devices->magnetometer);
    semu_sapporo_lsm6dsl_destroy(devices->accelerometer);
    semu_sapporo_hsppad143_destroy(devices->pressure);
    free(devices);
}

void semu_sapporo_devices_reset(semu_sapporo_devices *devices)
{
    if (devices == NULL) return;
    devices->fixture_context.state = NULL;
    devices->fixture_context.logger = NULL;
    semu_sapporo_cxd5610_set_exchange(devices->gps, NULL, NULL);
    devices->fixture_context.gps_running_status_armed = 0;
    semu_sapporo_hsppad143_reset(devices->pressure);
    semu_sapporo_lsm6dsl_reset(devices->accelerometer);
    semu_sapporo_tli493d_reset(devices->magnetometer);
    semu_sapporo_haptic_reset(devices->haptic);
    semu_sapporo_opt3007_reset(devices->ambient_light);
    semu_sapporo_max17050_reset(devices->battery_gauge);
    semu_sapporo_cxd5610_reset(devices->gps);
    semu_sapporo_ohr2_reset(devices->ohr2);
    semu_sapporo_flash_reset(devices->flash);
}

semu_status semu_sapporo_devices_bind_bus(semu_sapporo_devices *devices,
                                           semu_bus *bus, semu_error *error)
{
    if (devices == NULL || bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo device bus binding is incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    semu_sapporo_flash_bind_overlay_bus(devices->flash, bus);
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_sapporo_devices_bind_no_device_fixtures(
    semu_sapporo_devices *devices, semu_layer_state *state,
    semu_logger *logger, semu_error *error)
{
    if (devices == NULL || state == NULL || logger == NULL ||
        !state->enabled) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo compatibility fixture binding is incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    devices->fixture_context.state = state;
    devices->fixture_context.logger = logger;
    devices->fixture_context.gps_running_status_armed = 0;
    semu_sapporo_cxd5610_set_exchange(
        devices->gps, semu_sapporo_222_gps_exchange,
        &devices->fixture_context);
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_sapporo_devices_apply_compat_hook(
    semu_sapporo_devices *devices, semu_bus *bus, semu_cpu_state *cpu_state,
    semu_layer_state *state, semu_logger *logger, semu_error *error)
{
    uint64_t state_hook_hits;
    semu_status status;
    if (devices == NULL || bus == NULL || cpu_state == NULL || state == NULL ||
        logger == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo compatibility hook binding is incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    state_hook_hits = state->descriptor != NULL &&
        state->descriptor->interventions != NULL &&
        SEMU_SAPPORO_222_IV_GPS_STATE_STARTUP <
            state->descriptor->intervention_count
        ? state->descriptor->interventions[
              SEMU_SAPPORO_222_IV_GPS_STATE_STARTUP].hits : 0u;
    status = semu_sapporo_222_apply_firmware_hook(bus, cpu_state, state,
                                                   logger, error);
    if (status != SEMU_OK) {
        return status;
    }
    status = semu_sapporo_gps_compat_apply(
        devices->soc != NULL ? devices->soc->uart : NULL, bus, cpu_state,
        devices->gps, &devices->fixture_context, error);
    if (status != SEMU_OK) return status;
    if (state->descriptor != NULL && state->descriptor->interventions != NULL &&
        cpu_state->r[15] == UINT32_C(0x0010f610) && state_hook_hits == 1u &&
        state->descriptor->interventions[
            SEMU_SAPPORO_222_IV_GPS_STARTUP].hits == 0u) {
        return semu_sapporo_222_arm_gps_startup(devices->gps, state, logger,
                                                error);
    }
    return SEMU_OK;
}

semu_status semu_sapporo_devices_attach(semu_sapporo_devices *devices,
                                         semu_apollo4 *soc,
                                         semu_error *error)
{
    if (devices == NULL || soc == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "attach requires devices and soc");
        return SEMU_ERR_ARGUMENT;
    }
    devices->soc = soc;
    semu_sapporo_cxd5610_set_rx_sink(devices->gps,
                                     uart_bridge_receive, soc->uart);
    if (semu_apollo4_iom_attach_endpoint(soc->iom0,
            semu_sapporo_devices_iom_endpoint(devices, 0u), error) != SEMU_OK)
        return error->code;
    if (semu_apollo4_iom_attach_endpoint(soc->iom2,
            semu_sapporo_devices_iom_endpoint(devices, 2u), error) != SEMU_OK)
        return error->code;
    if (semu_apollo4_iom_attach_endpoint(soc->iom3,
            semu_sapporo_devices_iom_endpoint(devices, 3u), error) != SEMU_OK)
        return error->code;
    if (semu_apollo4_iom_attach_endpoint(soc->iom4,
            semu_sapporo_devices_iom_endpoint(devices, 4u), error) != SEMU_OK)
        return error->code;
    if (semu_apollo4_mspi_attach_endpoint(soc->mspi1,
            &devices->mspi1_ep, error) != SEMU_OK)
        return error->code;
    if (semu_apollo4_mspi_attach_endpoint(soc->mspi2,
            semu_sapporo_devices_mspi_flash_endpoint(devices),
            error) != SEMU_OK)
        return error->code;
    if (semu_apollo4_uart_attach_endpoint(soc->uart,
            semu_sapporo_devices_uart_endpoint(devices),
            error) != SEMU_OK)
        return error->code;
    return SEMU_OK;
}

const semu_serial_endpoint *semu_sapporo_devices_iom_endpoint(
    semu_sapporo_devices *devices, unsigned instance)
{
    if (devices == NULL) return NULL;
    switch (instance) {
    case 0u: return &devices->iom0_ep;
    case 2u: return &devices->iom2_ep;
    case 3u: return &devices->iom3_ep;
    case 4u: return &devices->iom4_ep;
    default: return &devices->refuse_ep;
    }
}

const semu_serial_endpoint *semu_sapporo_devices_mspi_flash_endpoint(
    semu_sapporo_devices *devices)
{
    return devices != NULL ? &devices->mspi_ep : NULL;
}

const semu_serial_endpoint *semu_sapporo_devices_mspi1_endpoint(
    semu_sapporo_devices *devices)
{
    return devices != NULL ? &devices->mspi1_ep : NULL;
}

const semu_apollo4_uart_endpoint *semu_sapporo_devices_uart_endpoint(
    semu_sapporo_devices *devices)
{
    return devices != NULL ? &devices->uart_ep : NULL;
}
