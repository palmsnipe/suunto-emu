/*
 * Sapporo 2.22 backlight (ticket 404).
 * Evidence: E-SAP-BACKLIGHT-001 (verified).
 * CTIMER9: OUTCFG104=0x12, control=0xA44, compare at 0x328/0x32c.
 * The backlight is on when OUTCFG is configured and compare values
 * are written.  Level is derived from the compare A value.
 */

#include "sapporo_backlight.h"

#include <stdlib.h>

enum {
    BL_OFFSET_OUTCFG = 0x320u,
    BL_OFFSET_CMPA   = 0x328u,
    BL_OFFSET_CMPB   = 0x32cu,
    BL_OFFSET_CMPC   = 0x330u
};

#define BL_EVIDENCED_OUTCFG 0x12u
#define BL_EVIDENCED_CTRL   0xA44u

struct semu_sapporo_backlight {
    uint32_t outcfg;
    uint32_t cmpa;
    uint32_t cmpb;
    uint32_t cmpc;
    int configured;
};

semu_sapporo_backlight *semu_sapporo_backlight_create(semu_error *error)
{
    semu_sapporo_backlight *bl;
    (void)error;
    bl = (semu_sapporo_backlight *)calloc(1u, sizeof(*bl));
    return bl;
}

void semu_sapporo_backlight_destroy(semu_sapporo_backlight *backlight)
{
    free(backlight);
}

void semu_sapporo_backlight_reset(semu_sapporo_backlight *backlight)
{
    if (backlight == NULL) {
        return;
    }
    backlight->outcfg = 0u;
    backlight->cmpa = 0u;
    backlight->cmpb = 0u;
    backlight->cmpc = 0u;
    backlight->configured = 0;
}

void semu_sapporo_backlight_on_timer_write(
    semu_sapporo_backlight *backlight, uint32_t offset, uint32_t value)
{
    if (backlight == NULL) {
        return;
    }
    switch (offset) {
    case BL_OFFSET_OUTCFG:
        backlight->outcfg = value;
        break;
    case BL_OFFSET_CMPA:
        backlight->cmpa = value;
        break;
    case BL_OFFSET_CMPB:
        backlight->cmpb = value;
        break;
    case BL_OFFSET_CMPC:
        backlight->cmpc = value;
        break;
    default:
        return;
    }
    if (backlight->outcfg == BL_EVIDENCED_OUTCFG &&
        (backlight->cmpa != 0u || backlight->cmpb != 0u)) {
        backlight->configured = 1;
    }
}

int semu_sapporo_backlight_is_on(const semu_sapporo_backlight *backlight)
{
    return backlight != NULL && backlight->configured;
}

uint32_t semu_sapporo_backlight_level(
    const semu_sapporo_backlight *backlight)
{
    if (backlight == NULL || !backlight->configured) {
        return 0u;
    }
    return backlight->cmpa;
}
