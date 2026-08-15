#ifndef SEMU_CLI_DEBUG_H
#define SEMU_CLI_DEBUG_H

#include "semu/types.h"

#include <stddef.h>

/*
 * Debug CLI options (ticket 620).
 * Bounded trace, report, replay, and snapshot configuration through
 * headless CLI options.  All operations are bounded and validated
 * before guest execution.
 */

#define SEMU_CLI_DEFAULT_TRACE_CAPACITY 1024u

typedef struct semu_cli_debug_options {
    const char *report_path;
    const char *snapshot_load_path;
    const char *snapshot_save_path;
    uint32_t trace_capacity;
    int trace_overflow_truncate;
    int has_trace_capacity;
} semu_cli_debug_options;

void semu_cli_debug_init(semu_cli_debug_options *opts);

/*
 * Parse one option.  Returns:
 *   1 if the option was recognized (value consumed if needed)
 *   0 if the option is not a debug option
 *  -1 on error (error set)
 * If the option requires a value and *value is NULL, the caller should
 * advance and supply the next argv entry.
 */
int semu_cli_debug_parse_option(semu_cli_debug_options *opts,
    const char *option, const char *value, semu_error *error);

/* Validate option combinations.  Returns 1 on success, 0 on error. */
int semu_cli_debug_validate(const semu_cli_debug_options *opts,
    semu_error *error);

/* Whether any debug option is set. */
int semu_cli_debug_active(const semu_cli_debug_options *opts);

#endif
