#ifndef SEMU_PRODUCTION_RECORDS_H
#define SEMU_PRODUCTION_RECORDS_H

#include "semu/bus.h"

#define SEMU_PRODUCTION_SECTOR_SIZE 4096u

/* Use the checksum table from the already validated guest application. */
semu_status semu_build_production_sector(semu_bus *bus, uint32_t table_address,
    uint8_t sector[SEMU_PRODUCTION_SECTOR_SIZE], semu_error *error);

#endif
