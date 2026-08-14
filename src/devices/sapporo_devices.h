#ifndef SEMU_SAPPORO_DEVICES_H
#define SEMU_SAPPORO_DEVICES_H

#include "semu/peripheral.h"

typedef enum semu_sapporo_device_kind {
    SEMU_SAPPORO_PRESSURE = 0,
    SEMU_SAPPORO_ACCELEROMETER,
    SEMU_SAPPORO_WRIST_MAGNETOMETER,
    SEMU_SAPPORO_HAPTIC,
    SEMU_SAPPORO_AMBIENT_LIGHT,
    SEMU_SAPPORO_OHR2
} semu_sapporo_device_kind;

typedef struct semu_sapporo_device semu_sapporo_device;

semu_sapporo_device *semu_sapporo_device_create(
    semu_sapporo_device_kind kind, semu_error *error);
void semu_sapporo_device_destroy(semu_sapporo_device *device);
semu_serial_endpoint semu_sapporo_device_endpoint(semu_sapporo_device *device);
void semu_sapporo_device_reset(semu_sapporo_device *device);

#endif
