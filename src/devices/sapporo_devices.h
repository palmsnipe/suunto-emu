#ifndef SEMU_SAPPORO_DEVICES_H
#define SEMU_SAPPORO_DEVICES_H

#include "semu/apollo4.h"
#include "semu/peripheral.h"
#include "semu/scheduler.h"
#include "../soc/apollo4/uart.h"

/*
 * Sapporo 2.22 device factory (ticket 420).
 * Owns all verified peripheral instances and exposes endpoints that
 * machine.c attaches to Apollo4 IOM/MSPI/UART controllers.  Multi-device
 * I2C buses (IOM2, IOM4) are multiplexed by I2C address.  The GPS UART
 * bridge wraps the CXD5610 serial endpoint in a byte-at-a-time interface.
 */

typedef struct semu_sapporo_devices semu_sapporo_devices;

semu_sapporo_devices *semu_sapporo_devices_create(
    semu_scheduler *scheduler, semu_error *error);
void semu_sapporo_devices_destroy(semu_sapporo_devices *devices);
void semu_sapporo_devices_reset(semu_sapporo_devices *devices);

/*
 * Attaches all verified device endpoints to the Apollo4 SoC controllers.
 * Flash evidence (E-SAP-FLASH-001) is missing, so MSPI2 receives a
 * refusal endpoint.  The caller must have called semu_apollo4_init first.
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
 * The flash evidence (E-SAP-FLASH-001) is missing, so this endpoint
 * refuses all transfers fail-closed.
 */
const semu_serial_endpoint *semu_sapporo_devices_mspi_flash_endpoint(
    semu_sapporo_devices *devices);

/*
 * Returns a pointer to the UART endpoint for the CXD5610 GPS transport.
 * The bridge wraps the CXD5610 serial endpoint in a byte-at-a-time
 * transmit function.
 */
const semu_apollo4_uart_endpoint *semu_sapporo_devices_uart_endpoint(
    semu_sapporo_devices *devices);

#endif
