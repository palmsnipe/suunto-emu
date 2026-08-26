/* Gated by SEMU_SDL_PPM_DIR (sdl_frontend.ppm_dump_dir). Writes one PPM per
 * distinct frame content hash so the onboarding walk's screens can be
 * inspected without a display. Not part of any deterministic checkpoint;
 * purely a local, host-side diagnostic. Included by main_sdl.c, which provides
 * the semu_frame/semu pixel-format context. */
#define SEMU_SDL_PPM_MAX_DIM 512u

static void semu_sdl_ppm_dump(const char *dir, uint32_t *last_crc,
                               uint32_t crc, uint64_t now_ns,
                               const semu_frame *frame)
{
    static uint8_t rgb[SEMU_SDL_PPM_MAX_DIM * SEMU_SDL_PPM_MAX_DIM * 3u];
    static uint64_t seq;
    char path[512];
    char logpath[512];
    FILE *out;
    FILE *log;
    uint32_t y;
    if (dir == NULL || frame == NULL || frame->format != SEMU_PIXEL_RGB565_LE ||
        frame->width > SEMU_SDL_PPM_MAX_DIM ||
        frame->height > SEMU_SDL_PPM_MAX_DIM ||
        frame->stride < frame->width * 2u) {
        return;
    }
    if (*last_crc == crc) {
        return;
    }
    *last_crc = crc;
    ++seq;
    snprintf(logpath, sizeof(logpath), "%s/frames.log", dir);
    log = fopen(logpath, "a");
    if (log != NULL) {
        fprintf(log, "%llu %llu %08x\n", (unsigned long long)seq,
                (unsigned long long)now_ns, crc);
        fclose(log);
    }
    for (y = 0u; y < frame->height; ++y) {
        const uint8_t *row = frame->pixels + (size_t)y * (size_t)frame->stride;
        uint32_t x;
        for (x = 0u; x < frame->width; ++x) {
            uint32_t v = (uint32_t)row[2u * x] |
                         ((uint32_t)row[2u * x + 1u] << 8u);
            uint32_t o = 3u * ((size_t)y * (size_t)frame->width + x);
            rgb[o + 0u] = (uint8_t)(((v >> 11u) & 0x1fu) * 255u / 31u);
            rgb[o + 1u] = (uint8_t)(((v >> 5u) & 0x3fu) * 255u / 63u);
            rgb[o + 2u] = (uint8_t)((v & 0x1fu) * 255u / 31u);
        }
    }
    snprintf(path, sizeof(path), "%s/suunto-frame-%08x.ppm", dir, crc);
    out = fopen(path, "wb");
    if (out == NULL) {
        return;
    }
    fprintf(out, "P6\n%u %u\n255\n", frame->width, frame->height);
    fwrite(rgb, 1u, (size_t)frame->width * (size_t)frame->height * 3u, out);
    fclose(out);
}
