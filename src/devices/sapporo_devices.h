#ifndef SEMU_SAPPORO_DEVICES_H
#define SEMU_SAPPORO_DEVICES_H

#include "semu/apollo4.h"
#include "semu/compat.h"
#include "semu/cpu.h"
#include "semu/log.h"
#include "semu/peripheral.h"
#include "semu/scheduler.h"
#include "semu/storage.h"
#include "../soc/apollo4/uart.h"

/*
 * Sapporo 2.22 device factory (ticket 420).
 * Owns all verified peripheral instances and exposes endpoints that
 * machine.c attaches to Apollo4 IOM/MSPI/UART controllers.  Multi-device
 * I2C buses (IOM2, IOM4) are multiplexed by I2C address.  The GPS UART
 * bridge wraps the CXD5610 serial endpoint in a byte-at-a-time interface.
 */

typedef struct semu_sapporo_devices semu_sapporo_devices;

static inline int semu_sapporo_devices_compat_hook_pc(uint32_t pc)
{
    switch (pc) {
    case UINT32_C(0x001145be): case UINT32_C(0x0009d166):
    case UINT32_C(0x001145e8): case UINT32_C(0x0011469c):
    case UINT32_C(0x0011470a): case UINT32_C(0x0010f6d8):
    case UINT32_C(0x0010f4fc): case UINT32_C(0x0010f610):
    case UINT32_C(0x0010f7c2): case UINT32_C(0x0010f7b8):
    case UINT32_C(0x0009aaec): case UINT32_C(0x0009a3b8):
    case UINT32_C(0x0010fbde):
        return 1;
    default:
        return 0;
    }
}

semu_sapporo_devices *semu_sapporo_devices_create(
    semu_scheduler *scheduler, const semu_storage *flash_storage,
    semu_error *error);

/* Selects version-specific devices. Must be called at most once, pre-attach. */
semu_status semu_sapporo_devices_select_profile(
    semu_sapporo_devices *devices, const char *profile_id,
    semu_error *error);
void semu_sapporo_devices_destroy(semu_sapporo_devices *devices);
void semu_sapporo_devices_reset(semu_sapporo_devices *devices);
void semu_sapporo_devices_set_logger(semu_sapporo_devices *devices,
                                     semu_logger *logger);
semu_status semu_sapporo_devices_bind_bus(semu_sapporo_devices *devices,
                                           semu_bus *bus, semu_error *error);

/*
 * Enables the explicitly selected 2.22 startup fixtures.  Without this
 * binding, the GPS and OHR endpoints remain fail-closed.
 */
semu_status semu_sapporo_devices_bind_no_device_fixtures(
    semu_sapporo_devices *devices, semu_layer_state *state,
    semu_logger *logger, semu_error *error);

semu_status semu_sapporo_devices_apply_compat_hook(
    semu_sapporo_devices *devices, semu_bus *bus, semu_cpu_state *cpu_state,
    semu_layer_state *state, semu_logger *logger, semu_error *error);

/*
 * Attaches all verified device endpoints to the Apollo4 SoC controllers.
 * A NULL flash_storage intentionally leaves MSPI2 on its refusal endpoint;
 * the machine passes the validated resources base to enable the observed
 * startup flash-read contract.  The caller must have called
 * semu_apollo4_init first.
 */
semu_status semu_sapporo_devices_attach(semu_sapporo_devices *devices,
                                         semu_apollo4 *soc,
                                         semu_error *error);

/*
 * Returns a pointer to the serial endpoint for the IOM at the given
 * controller instance.  Multi-device IOMs return an I2C multiplexer
 * endpoint that dispatches by transaction address.  Instances with no
 * verified device return a refusal endpoint.  The pointer is valid for
 * the lifetime of the devices struct.
 */
const semu_serial_endpoint *semu_sapporo_devices_iom_endpoint(
    semu_sapporo_devices *devices, unsigned instance);

/*
 * Returns a pointer to the serial endpoint for the MSPI2 external flash.
 * When no storage was supplied, this is an explicit refusal endpoint.
 */
const semu_serial_endpoint *semu_sapporo_devices_mspi_flash_endpoint(
    semu_sapporo_devices *devices);

/* Returns the evidenced MSPI1 DIAP4 completion-only endpoint. */
const semu_serial_endpoint *semu_sapporo_devices_mspi1_endpoint(
    semu_sapporo_devices *devices);

/*
 * Returns a pointer to the UART endpoint for the CXD5610 GPS transport.
 * The bridge wraps the CXD5610 serial endpoint in a byte-at-a-time
 * transmit function.
 */
const semu_apollo4_uart_endpoint *semu_sapporo_devices_uart_endpoint(
    semu_sapporo_devices *devices);

#endif
