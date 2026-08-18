#ifndef SEMU_SAPPORO_222_COMPAT_H
#define SEMU_SAPPORO_222_COMPAT_H

#include "semu/bus.h"
#include "semu/compat.h"
#include "semu/cpu.h"
#include "sapporo_cxd5610.h"
#include "sapporo_ohr2.h"

extern const semu_layer_descriptor semu_sapporo_222_no_device_layer;

semu_status semu_sapporo_222_install_no_device(
    semu_bus *bus, semu_layer_state *state, semu_logger *logger,
    semu_error *error);

/* Intervention indices for the no-device layer. */
enum {
    SEMU_SAPPORO_222_IV_PRODUCTION = 0u,
    SEMU_SAPPORO_222_IV_GPS_STARTUP,
    SEMU_SAPPORO_222_IV_GPS_STATE_STARTUP,
    SEMU_SAPPORO_222_IV_GPS_STATE_DISPATCH,
    SEMU_SAPPORO_222_IV_GPS_RUNNING_STATUS,
    SEMU_SAPPORO_222_IV_GPS_AWAKE_PULSE,
    SEMU_SAPPORO_222_IV_OHR_STARTUP,
    SEMU_SAPPORO_222_IV_RESOURCE_STATUS,
    SEMU_SAPPORO_222_IV_DIAP_WORKER_WAKE,
    SEMU_SAPPORO_222_IV_DIAP_WORKER_IRQ,
    SEMU_SAPPORO_222_IV_COUNT
};

/* Fixture context passed to GPS/OHR provider functions.  The caller
 * (machine wiring or test) owns the lifetime. */
typedef struct semu_sapporo_222_fixture_context {
    semu_layer_state *state;
    semu_logger *logger;
    int gps_running_status_armed;
} semu_sapporo_222_fixture_context;

/* GPS fixture provider: matches @VER request, injects $PSS0000 response
 * after 10 ms, and records the GPS-startup intervention hit. */
semu_transaction_result semu_sapporo_222_gps_exchange(
    void *context, const uint8_t *request, size_t count,
    semu_sapporo_cxd5610 *transport, semu_error *error);

semu_status semu_sapporo_222_arm_gps_startup(
    semu_sapporo_cxd5610 *transport, semu_layer_state *state,
    semu_logger *logger, semu_error *error);

/* Arm the observed later GPS open at 0x10f7c2: an unsolicited $PSS line and
 * an exact @GSR -> $PSS response are delivered through the normal UART path. */
semu_status semu_sapporo_222_arm_gps_running_status(
    semu_sapporo_cxd5610 *transport,
    semu_sapporo_222_fixture_context *context, semu_error *error);

semu_status semu_sapporo_222_arm_gps_awake_pulse(
    semu_sapporo_cxd5610 *transport,
    semu_sapporo_222_fixture_context *context, semu_error *error);

/* Apply the one version-pinned startup hook recovered from the native run:
 * after service request 7 succeeds, enter the firmware's existing GPS
 * UART-open routine.  A non-trigger PC is a no-op; an unexpected register
 * state at the exact trigger refuses. */
semu_status semu_sapporo_222_apply_firmware_hook(
    semu_bus *bus, semu_cpu_state *cpu_state, semu_layer_state *state,
    semu_logger *logger, semu_error *error);

/* OHR2 body provider: supplies synthetic startup responses for
 * identity/configure/echo/result commands and records the OHR-startup
 * intervention hit.  Unknown commands refuse. */
semu_transaction_result semu_sapporo_222_ohr_body_provider(
    void *context, semu_sapporo_ohr2_command command, uint16_t sequence,
    semu_sapporo_ohr2_state state,
    const uint8_t request_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    uint8_t response_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    semu_error *error);

#endif
