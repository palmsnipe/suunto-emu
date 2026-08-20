/*
 * Debug CLI option parsing and validation (ticket 620).
 * Bounded trace, report, replay, and snapshot configuration.
 */

#include "cli_debug.h"
#include "semu/trace.h"

#include <string.h>

void semu_cli_debug_init(semu_cli_debug_options *opts)
{
    if (opts == NULL) {
        return;
    }
    memset(opts, 0, sizeof(*opts));
    opts->trace_capacity = SEMU_CLI_DEFAULT_TRACE_CAPACITY;
}

int semu_cli_debug_active(const semu_cli_debug_options *opts)
{
    if (opts == NULL) {
        return 0;
    }
    return opts->report_path != NULL ||
           opts->snapshot_load_path != NULL ||
           opts->snapshot_save_path != NULL ||
           opts->has_trace_capacity;
}

static int parse_u32(const char *text, uint32_t *out)
{
    uint32_t val = 0u;
    size_t i;
    size_t len;
    if (text == NULL) {
        return 0;
    }
    len = strlen(text);
    if (len == 0u || len > 10u) {
        return 0;
    }
    for (i = 0u; i < len; ++i) {
        uint32_t digit;
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        digit = (uint32_t)(text[i] - '0');
        if (val > (UINT32_MAX - digit) / 10u) {
            return 0;
        }
        val = val * 10u + digit;
    }
    *out = val;
    return 1;
}

int semu_cli_debug_parse_option(semu_cli_debug_options *opts,
    const char *option, const char *value, semu_error *error)
{
    if (opts == NULL || option == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "debug: null argument");
        return -1;
    }
    if (strcmp(option, "--report") == 0) {
        if (value == NULL) {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                "option %s requires a value", option);
            return -1;
        }
        opts->report_path = value;
        return 1;
    }
    if (strcmp(option, "--snapshot-load") == 0) {
        if (value == NULL) {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                "option %s requires a value", option);
            return -1;
        }
        opts->snapshot_load_path = value;
        return 1;
    }
    if (strcmp(option, "--snapshot-save") == 0) {
        if (value == NULL) {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                "option %s requires a value", option);
            return -1;
        }
        opts->snapshot_save_path = value;
        return 1;
    }
    if (strcmp(option, "--trace-capacity") == 0) {
        if (value == NULL) {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                "option %s requires a value", option);
            return -1;
        }
        if (!parse_u32(value, &opts->trace_capacity) ||
            opts->trace_capacity == 0u ||
            opts->trace_capacity > SEMU_TRACE_MAX_RECORDS) {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                "invalid --trace-capacity %s (range 1-%u)",
                value, SEMU_TRACE_MAX_RECORDS);
            return -1;
        }
        opts->has_trace_capacity = 1;
        return 1;
    }
    if (strcmp(option, "--trace-overflow") == 0) {
        if (value == NULL) {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                "option %s requires a value", option);
            return -1;
        }
        if (strcmp(value, "stop") == 0) {
            opts->trace_overflow_truncate = 0;
        } else if (strcmp(value, "truncate") == 0) {
            opts->trace_overflow_truncate = 1;
        } else {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                "invalid --trace-overflow %s (stop|truncate)", value);
            return -1;
        }
        opts->has_trace_capacity = 1;
        return 1;
    }
    return 0;
}

int semu_cli_debug_validate(const semu_cli_debug_options *opts,
    semu_error *error)
{
    if (opts == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "debug: null options");
        return 0;
    }
    if (opts->snapshot_load_path != NULL &&
        opts->snapshot_save_path != NULL &&
        strcmp(opts->snapshot_load_path, opts->snapshot_save_path) == 0) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
            "snapshot load and save paths must differ");
        return 0;
    }
    return 1;
}
