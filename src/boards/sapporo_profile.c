/*
 * Sapporo 2.22.60 immutable profile table (ticket 400).
 * Evidence: E-SAP-PROFILE-001 (verified), E-SAP-0005/0006/0007 (component
 * hashes/sizes/addresses), E-SAP-0001 (240x240 RGB565 display contract),
 * suunto-sapporo.repl (NVIC priority mask 0xE0, memory ranges).
 */

#include "sapporo_profile.h"

#include <string.h>

#include "semu/types.h"

static const semu_sapporo_profile profile = {
    .id = "sapporo-2.22.60",
    .board = "sapporo",
    .product = "Sapporo",
    .version = "2.22.60.3383-P",
    .vector_table = 0x00040000u,
    .nvic_priority_mask = 0xE0u,
    .display_width = 240u,
    .display_height = 240u
};

static const semu_sapporo_region regions[] = {
    { SEMU_SAPPORO_REGION_MRAM, "mram", 0x00000000u, 0x00200000u },
    { SEMU_SAPPORO_REGION_SRAM, "sram", 0x10000000u, 0x00180000u },
    { SEMU_SAPPORO_REGION_EXTERNAL_FLASH, "external-flash",
      0x14000000u, 0x02000000u }
};

static const semu_sapporo_component components[] = {
    { SEMU_SAPPORO_COMPONENT_RESIDENT, "resident",
      0x00019000u, 71504u,
      "a409b088a061c2fe61689c8f39a79b2c35ed0059cd66987646e0195e47a2f522" },
    { SEMU_SAPPORO_COMPONENT_APPLICATION, "application",
      0x00040000u, 1493234u,
      "c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc" },
    { SEMU_SAPPORO_COMPONENT_RESOURCES, "resources",
      0x14000000u, 16519168u,
      "ec2a4b1c472844ac6ff9cc575cb9383c29a74302107af3619f9a08fdf6abcaf1" }
};

const semu_sapporo_profile *semu_sapporo_profile_get(void)
{
    return &profile;
}

const semu_sapporo_region *semu_sapporo_profile_regions(size_t *count)
{
    if (count != NULL) {
        *count = sizeof(regions) / sizeof(regions[0]);
    }
    return regions;
}

const semu_sapporo_region *semu_sapporo_profile_find_region(
    semu_sapporo_region_id id)
{
    size_t i;
    for (i = 0u; i < sizeof(regions) / sizeof(regions[0]); ++i) {
        if (regions[i].id == id) {
            return &regions[i];
        }
    }
    return NULL;
}

const semu_sapporo_component *semu_sapporo_profile_components(size_t *count)
{
    if (count != NULL) {
        *count = sizeof(components) / sizeof(components[0]);
    }
    return components;
}

const semu_sapporo_component *semu_sapporo_profile_find_component(
    semu_sapporo_component_id id)
{
    size_t i;
    for (i = 0u; i < sizeof(components) / sizeof(components[0]); ++i) {
        if (components[i].id == id) {
            return &components[i];
        }
    }
    return NULL;
}

static int range_overlaps(uint32_t a_base, uint32_t a_end,
                          uint32_t b_base, uint32_t b_end)
{
    return a_base < b_end && b_base < a_end;
}

static int component_in_region(const semu_sapporo_component *c,
                               const semu_sapporo_region *r)
{
    uint32_t c_end = c->load_address + c->size;
    uint32_t r_end = r->base + r->size;
    return c->load_address >= r->base && c_end <= r_end;
}

semu_status semu_sapporo_profile_validate(semu_error *error)
{
    size_t i;
    size_t j;
    size_t region_count;
    const semu_sapporo_region *regs;

    regs = semu_sapporo_profile_regions(&region_count);

    for (i = 0u; i < region_count; ++i) {
        for (j = i + 1u; j < region_count; ++j) {
            if (range_overlaps(regs[i].base, regs[i].base + regs[i].size,
                               regs[j].base, regs[j].base + regs[j].size)) {
                semu_error_set(error, SEMU_ERR_STATE,
                               "profile regions %s and %s overlap",
                               regs[i].name, regs[j].name);
                return SEMU_ERR_STATE;
            }
        }
    }

    for (i = 0u; i < (size_t)SEMU_SAPPORO_COMPONENT_COUNT; ++i) {
        const semu_sapporo_component *c = semu_sapporo_profile_find_component(
            (semu_sapporo_component_id)i);
        int found = 0;
        if (c == NULL) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "profile component %zu missing", i);
            return SEMU_ERR_STATE;
        }
        for (j = 0u; j < region_count; ++j) {
            if (component_in_region(c, &regs[j])) {
                found = 1;
                break;
            }
        }
        if (!found) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "profile component %s load 0x%08x outside regions",
                           c->role, c->load_address);
            return SEMU_ERR_STATE;
        }
    }

    semu_error_clear(error);
    return SEMU_OK;
}
