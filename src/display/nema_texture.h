#ifndef SEMU_NEMA_TEXTURE_H
#define SEMU_NEMA_TEXTURE_H

#include "semu/bus.h"
#include "semu/types.h"

/*
 * Nema texture descriptor validation and decode (ticket 504).
 * Validates and decodes only evidenced RGB565 and A2LE texture
 * descriptors into bounded integer samples.  TSC6A, RGBA4444, and
 * unobserved filtering remain unsupported.
 *
 * Evidence: E-NEMA-TEXTURE-001 (verified),
 *           E-NEMA-A2LE-001 (missing — endpoint coverage only).
 */

#define NEMA_TEX_FMT_RGB565  0x04u
#define NEMA_TEX_FMT_A2LE     0x28u
#define NEMA_TEX_FMT_TSC6A    0x17u

/* Only the observed point/nearest sampling mode is implemented. */
#define NEMA_TEX_SAMPLING_NEAREST 0x00u

#define NEMA_TEX_MAX_DIM 512u

typedef struct {
    uint32_t base;
    uint32_t format;
    uint32_t sampling;
    uint32_t stride;
    uint32_t width;
    uint32_t height;
} nema_texture_desc;

typedef struct {
    uint8_t r, g, b, a;
} nema_texel;

/*
 * Validate descriptor fields and confirm the complete texture range
 * is bus-accessible.  Checks format, dimensions, stride sufficiency,
 * and arithmetic overflow before any sample read.
 */
semu_status nema_texture_validate(semu_bus *bus,
                                   const nema_texture_desc *desc,
                                   semu_error *error);

/*
 * Sample one texel at integer coordinate (x, y).  Returns REFUSE
 * for out-of-range coordinates or unsupported formats.  Must be
 * called after nema_texture_validate succeeds.
 */
semu_status nema_texture_sample(semu_bus *bus,
                                 const nema_texture_desc *desc,
                                 uint32_t x, uint32_t y,
                                 nema_texel *out, semu_error *error);

/*
 * A2LE decoder (exposed for direct testing).
 * 4 two-bit alpha samples per byte, LSB first.
 * Endpoint values: 0, 85, 170, 255.
 */
semu_status nema_a2le_sample(semu_bus *bus, uint32_t base, uint32_t stride,
                              uint32_t width, uint32_t height,
                              uint32_t x, uint32_t y,
                              uint8_t *alpha, semu_error *error);

#endif
