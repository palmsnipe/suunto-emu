#ifndef SEMU_APOLLO4_WATCHDOG_H
#define SEMU_APOLLO4_WATCHDOG_H

#include <stdint.h>

#include "semu/bus.h"
#include "../../core/snapshot_io.h"

#define SEMU_APOLLO4_WATCHDOG_BASE UINT32_C(0x40024000)
#define SEMU_APOLLO4_WATCHDOG_SIZE UINT32_C(0x400)
#define SEMU_APOLLO4_WATCHDOG_CFG_RESET UINT32_C(0x00ffff00)
#define SEMU_APOLLO4_WATCHDOG_RESTART_OFFSET UINT32_C(0x4)
#define SEMU_APOLLO4_WATCHDOG_RESTART_KEY UINT32_C(0xb2)
#define SEMU_APOLLO4_WATCHDOG_INTEN_OFFSET UINT32_C(0x200)

typedef struct semu_apollo4_watchdog semu_apollo4_watchdog;

semu_apollo4_watchdog *semu_apollo4_watchdog_create(
    semu_bus *bus, semu_error *error);
void semu_apollo4_watchdog_destroy(semu_apollo4_watchdog *watchdog);
void semu_apollo4_watchdog_reset(void *context);

semu_status semu_apollo4_watchdog_read(
    void *context, uint32_t offset, unsigned width, uint32_t *value,
    semu_error *error);
semu_status semu_apollo4_watchdog_write(
    void *context, uint32_t offset, unsigned width, uint32_t value,
    semu_error *error);
semu_status semu_apollo4_watchdog_snapshot_write(
    const semu_apollo4_watchdog *watchdog, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_apollo4_watchdog_snapshot_read(
    semu_apollo4_watchdog *watchdog, semu_snapshot_reader *reader,
    semu_error *error);

#endif
