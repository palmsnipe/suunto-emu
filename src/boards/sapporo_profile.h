#ifndef SEMU_BOARDS_SAPPORO_PROFILE_H
#define SEMU_BOARDS_SAPPORO_PROFILE_H

#include <stddef.h>
#include <stdint.h>

/*
 * Sapporo 2.22.60 immutable profile table (ticket 400).
 * All values cite verified evidence row E-SAP-PROFILE-001.  The table
 * is read-only: no function here maps memory, creates devices, or
 * changes machine state.  Use machine.c for runtime wiring.
 */

typedef enum {
    SEMU_SAPPORO_REGION_MRAM,
    SEMU_SAPPORO_REGION_SRAM,
    SEMU_SAPPORO_REGION_EXTERNAL_FLASH,
    SEMU_SAPPORO_REGION_COUNT
} semu_sapporo_region_id;

typedef enum {
    SEMU_SAPPORO_COMPONENT_RESIDENT,
    SEMU_SAPPORO_COMPONENT_APPLICATION,
    SEMU_SAPPORO_COMPONENT_RESOURCES,
    SEMU_SAPPORO_COMPONENT_COUNT
} semu_sapporo_component_id;

typedef struct {
    semu_sapporo_region_id id;
    const char *name;
    uint32_t base;
    uint32_t size;
} semu_sapporo_region;

typedef struct {
    semu_sapporo_component_id id;
    const char *role;
    uint32_t load_address;
    uint32_t size;
    const char *sha256;
} semu_sapporo_component;

typedef struct {
    const char *id;
    const char *board;
    const char *product;
    const char *version;
    uint32_t vector_table;
    uint32_t nvic_priority_mask;
    uint32_t display_width;
    uint32_t display_height;
} semu_sapporo_profile;

const semu_sapporo_profile *semu_sapporo_profile_get(void);

const semu_sapporo_region *semu_sapporo_profile_regions(size_t *count);
const semu_sapporo_region *semu_sapporo_profile_find_region(
    semu_sapporo_region_id id);

const semu_sapporo_component *semu_sapporo_profile_components(size_t *count);
const semu_sapporo_component *semu_sapporo_profile_find_component(
    semu_sapporo_component_id id);

/*
 * Validation helpers.  semu_sapporo_profile_validate checks that
 * memory regions do not overlap and that all component load addresses
 * fall inside a known region.  Returns SEMU_OK on success, an error
 * code otherwise.
 */
#include "semu/types.h"
semu_status semu_sapporo_profile_validate(semu_error *error);

#endif
