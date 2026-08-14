#include "semu/compat.h"

#include <stdio.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

int main(void)
{
    const semu_layer_descriptor descriptor = {
        "test-layer", SEMU_LAYER_DEVICE_FIXTURE, "test-profile", "unit test", 1u
    };
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    semu_error_clear(&error);
    semu_log_init(&logger, NULL, SEMU_LOG_ERROR);
    CHECK(semu_layer_enable(&state, &descriptor, "wrong", &error) ==
          SEMU_ERR_CONFLICT);
    CHECK(semu_layer_enable(&state, &descriptor, "test-profile", &error) ==
          SEMU_OK);
    CHECK(semu_layer_hit(&state, &logger, "test", &error) == SEMU_OK);
    CHECK(semu_layer_hit(&state, &logger, "test", &error) == SEMU_ERR_STATE);
    return failures != 0 ? 1 : 0;
}
