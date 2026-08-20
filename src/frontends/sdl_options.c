#include "sdl_options.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "sdl_present_core.h"

static int parse_scale_value(const char *text, uint32_t *scale)
{
    char *end;
    unsigned long long value;

    if (text == NULL || text[0] == '\0' || text[0] == '+' ||
        text[0] == '-') {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' ||
        value < SDL_PRESENT_MIN_SCALE || value > SDL_PRESENT_MAX_SCALE) {
        return 0;
    }
    *scale = (uint32_t)value;
    return 1;
}

semu_status semu_sdl_parse_scale(int argc, char *const *argv,
    uint32_t *scale, semu_error *error)
{
    int i;

    if (argc < 0 || argv == NULL || scale == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "SDL options: invalid scale arguments");
        return SEMU_ERR_ARGUMENT;
    }
    *scale = 2u;
    for (i = 1; i < argc; ++i) {
        if (argv[i] == NULL) {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                           "SDL options: null argument");
            return SEMU_ERR_ARGUMENT;
        }
        if (strcmp(argv[i], "--scale") != 0) {
            continue;
        }
        if (i + 1 >= argc || argv[i + 1] == NULL) {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                           "SDL options: --scale requires a value");
            return SEMU_ERR_ARGUMENT;
        }
        if (!parse_scale_value(argv[i + 1], scale)) {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                           "SDL options: invalid --scale value %s",
                           argv[i + 1]);
            return SEMU_ERR_ARGUMENT;
        }
        ++i;
    }
    return SEMU_OK;
}
