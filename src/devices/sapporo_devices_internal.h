#ifndef SEMU_SAPPORO_DEVICES_INTERNAL_H
#define SEMU_SAPPORO_DEVICES_INTERNAL_H

#include "sapporo_devices.h"
#include "sapporo_cxd5610.h"
#include "sapporo_flash.h"
#include "sapporo_hsppad143.h"
#include "sapporo_haptic.h"
#include "sapporo_lsm6dsl.h"
#include "sapporo_lps22.h"
#include "sapporo_max17050.h"
#include "sapporo_ohr2.h"
#include "sapporo_opt3007.h"
#include "sapporo_tli493d.h"
#include "../compat/sapporo_222.h"
#include "../compat/sapporo_239_gps.h"
#include "../soc/apollo4/apollo4_internal.h"

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
    semu_sapporo_lps22 *lps22;
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
    semu_sapporo_239_gps_context gps_239_context;
    int profile_selected;
    int ohr2_profile_239;
};

semu_status semu_sapporo_devices_snapshot_write(
    const semu_sapporo_devices *devices, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_sapporo_devices_snapshot_read(
    semu_sapporo_devices *devices, semu_snapshot_reader *reader,
    semu_error *error);
semu_status semu_sapporo_devices_snapshot_resolve_event(
    semu_sapporo_devices *devices, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error);
semu_status semu_sapporo_devices_snapshot_event_id_matches(
    const semu_sapporo_devices *devices, uint32_t subject,
    semu_event_id event_id, semu_error *error);
semu_status semu_sapporo_devices_snapshot_event_links_match(
    const semu_sapporo_devices *devices,
    const semu_scheduled_event_state *events, size_t count,
    semu_error *error);

#endif
