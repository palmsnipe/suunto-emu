#include "../../src/frontends/cli.c"
#include "test.h"

#include <stdio.h>
#include <string.h>

typedef struct frame_observer {
    unsigned calls;
    const semu_frame *last;
} frame_observer;

static void observe_frame(void *context, const semu_frame *frame)
{
    frame_observer *observer = (frame_observer *)context;
    ++observer->calls;
    observer->last = frame;
}

static void test_first_frame_gate(semu_test_context *context)
{
    uint8_t pixel[2u] = {0x1Fu, 0u};
    semu_frame frame = {SEMU_PIXEL_RGB565_LE, 240u, 240u, 480u, 1u,
                        pixel, sizeof(pixel)};
    frame_observer observer = {0};
    first_frame_gate gate = {observe_frame, &observer, 0, 0, 0, 0};

    first_frame_gate_publish(&gate, &frame);
    SEMU_TEST_EQ_U64(context, 1u, observer.calls);
    SEMU_TEST_ASSERT(context, observer.last == &frame);
    SEMU_TEST_EQ_U64(context, 1u, gate.reached);

    gate.reached = 0;
    pixel[0] = 0u;
    first_frame_gate_publish(&gate, &frame);
    SEMU_TEST_EQ_U64(context, 1u, observer.calls);
    SEMU_TEST_EQ_U64(context, 0u, gate.reached);

    first_frame_gate_publish(&gate, NULL);
    SEMU_TEST_EQ_U64(context, 1u, observer.calls);
    SEMU_TEST_EQ_U64(context, 0u, gate.reached);

    gate.wait_for_input = 1;
    gate.required_button = SEMU_BUTTON_MIDDLE;
    gate.input_seen = 0;
    pixel[0] = 0x1Fu;
    first_frame_gate_publish(&gate, &frame);
    SEMU_TEST_EQ_U64(context, 1u, observer.calls);
    gate.input_seen = 1;
    first_frame_gate_publish(&gate, &frame);
    SEMU_TEST_EQ_U64(context, 2u, observer.calls);
}

static void test_first_frame_gate_requires_press(semu_test_context *context)
{
    first_frame_gate gate = {0};
    semu_input_event event = {SEMU_INPUT_BUTTON, SEMU_BUTTON_MIDDLE, 1, 0, 0};

    gate.wait_for_input = 1;
    gate.required_button = SEMU_BUTTON_MIDDLE;
    first_frame_gate_note_input(&gate, &event);
    SEMU_TEST_EQ_U64(context, 0u, gate.input_seen);

    event.code = SEMU_BUTTON_UPPER;
    event.value = 0;
    first_frame_gate_note_input(&gate, &event);
    SEMU_TEST_EQ_U64(context, 0u, gate.input_seen);

    event.code = SEMU_BUTTON_MIDDLE;
    first_frame_gate_note_input(&gate, &event);
    SEMU_TEST_EQ_U64(context, 1u, gate.input_seen);
}

static void test_report_option(semu_test_context *context)
{
    semu_cli_debug_options opts;
    semu_error err;

    semu_error_clear(&err);
    semu_cli_debug_init(&opts);
    SEMU_TEST_EQ_U64(context, 1,
        (uint64_t)semu_cli_debug_parse_option(&opts, "--report",
            "/tmp/report.txt", &err));
    SEMU_TEST_ASSERT(context, opts.report_path != NULL);
    SEMU_TEST_ASSERT(context,
        strcmp(opts.report_path, "/tmp/report.txt") == 0);
    SEMU_TEST_ASSERT(context, semu_cli_debug_active(&opts));
}

static void test_snapshot_options(semu_test_context *context)
{
    semu_cli_debug_options opts;
    semu_error err;

    semu_error_clear(&err);
    semu_cli_debug_init(&opts);
    SEMU_TEST_EQ_U64(context, 1,
        (uint64_t)semu_cli_debug_parse_option(&opts, "--snapshot-load",
            "/tmp/load.bin", &err));
    SEMU_TEST_EQ_U64(context, 1,
        (uint64_t)semu_cli_debug_parse_option(&opts, "--snapshot-save",
            "/tmp/save.bin", &err));
    SEMU_TEST_ASSERT(context, opts.snapshot_load_path != NULL);
    SEMU_TEST_ASSERT(context, opts.snapshot_save_path != NULL);
    SEMU_TEST_ASSERT(context, semu_cli_debug_active(&opts));
    SEMU_TEST_EQ_U64(context, 1,
        (uint64_t)semu_cli_debug_validate(&opts, &err));
}

static void test_trace_capacity(semu_test_context *context)
{
    semu_cli_debug_options opts;
    semu_error err;

    semu_error_clear(&err);
    semu_cli_debug_init(&opts);
    SEMU_TEST_EQ_U64(context, 1,
        (uint64_t)semu_cli_debug_parse_option(&opts, "--trace-capacity",
            "512", &err));
    SEMU_TEST_EQ_U64(context, 512u, opts.trace_capacity);
    SEMU_TEST_ASSERT(context, opts.has_trace_capacity);
    SEMU_TEST_ASSERT(context, semu_cli_debug_active(&opts));
}

static void test_trace_overflow(semu_test_context *context)
{
    semu_cli_debug_options opts;
    semu_error err;

    semu_error_clear(&err);
    semu_cli_debug_init(&opts);
    SEMU_TEST_EQ_U64(context, 1,
        (uint64_t)semu_cli_debug_parse_option(&opts, "--trace-overflow",
            "truncate", &err));
    SEMU_TEST_ASSERT(context, opts.trace_overflow_truncate);
    SEMU_TEST_ASSERT(context, opts.has_trace_capacity);

    semu_cli_debug_init(&opts);
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, 1,
        (uint64_t)semu_cli_debug_parse_option(&opts, "--trace-overflow",
            "stop", &err));
    SEMU_TEST_ASSERT(context, !opts.trace_overflow_truncate);
}

static void test_invalid_trace_capacity(semu_test_context *context)
{
    semu_cli_debug_options opts;
    semu_error err;

    semu_error_clear(&err);
    semu_cli_debug_init(&opts);
    SEMU_TEST_EQ_U64(context, (uint64_t)-1,
        (uint64_t)semu_cli_debug_parse_option(&opts, "--trace-capacity",
            "0", &err));

    semu_cli_debug_init(&opts);
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, (uint64_t)-1,
        (uint64_t)semu_cli_debug_parse_option(&opts, "--trace-capacity",
            "abc", &err));

    semu_cli_debug_init(&opts);
    semu_error_clear(&err);
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u", SEMU_TRACE_MAX_RECORDS + 1u);
        SEMU_TEST_EQ_U64(context, (uint64_t)-1,
            (uint64_t)semu_cli_debug_parse_option(&opts, "--trace-capacity",
                buf, &err));
    }
}

static void test_invalid_trace_overflow(semu_test_context *context)
{
    semu_cli_debug_options opts;
    semu_error err;

    semu_error_clear(&err);
    semu_cli_debug_init(&opts);
    SEMU_TEST_EQ_U64(context, (uint64_t)-1,
        (uint64_t)semu_cli_debug_parse_option(&opts, "--trace-overflow",
            "drop", &err));
}

static void test_unknown_option(semu_test_context *context)
{
    semu_cli_debug_options opts;
    semu_error err;

    semu_error_clear(&err);
    semu_cli_debug_init(&opts);
    SEMU_TEST_EQ_U64(context, 0,
        (uint64_t)semu_cli_debug_parse_option(&opts, "--bogus",
            "value", &err));
    SEMU_TEST_ASSERT(context, !semu_cli_debug_active(&opts));
}

static void test_missing_value(semu_test_context *context)
{
    semu_cli_debug_options opts;
    semu_error err;

    semu_error_clear(&err);
    semu_cli_debug_init(&opts);
    SEMU_TEST_EQ_U64(context, (uint64_t)-1,
        (uint64_t)semu_cli_debug_parse_option(&opts, "--report",
            NULL, &err));
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, (uint64_t)-1,
        (uint64_t)semu_cli_debug_parse_option(&opts, "--snapshot-load",
            NULL, &err));
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, (uint64_t)-1,
        (uint64_t)semu_cli_debug_parse_option(&opts, "--trace-capacity",
            NULL, &err));
}

static void test_same_load_save_path(semu_test_context *context)
{
    semu_cli_debug_options opts;
    semu_error err;

    semu_error_clear(&err);
    semu_cli_debug_init(&opts);
    opts.snapshot_load_path = "/tmp/snap.bin";
    opts.snapshot_save_path = "/tmp/snap.bin";
    SEMU_TEST_EQ_U64(context, 0,
        (uint64_t)semu_cli_debug_validate(&opts, &err));
}

static void test_different_load_save_path(semu_test_context *context)
{
    semu_cli_debug_options opts;
    semu_error err;

    semu_error_clear(&err);
    semu_cli_debug_init(&opts);
    opts.snapshot_load_path = "/tmp/load.bin";
    opts.snapshot_save_path = "/tmp/save.bin";
    SEMU_TEST_EQ_U64(context, 1,
        (uint64_t)semu_cli_debug_validate(&opts, &err));
}

static void test_no_debug_options_inactive(semu_test_context *context)
{
    semu_cli_debug_options opts;
    semu_cli_debug_init(&opts);
    SEMU_TEST_ASSERT(context, !semu_cli_debug_active(&opts));
    SEMU_TEST_EQ_U64(context, SEMU_CLI_DEFAULT_TRACE_CAPACITY,
        opts.trace_capacity);
}

static void test_null_safety(semu_test_context *context)
{
    semu_error err;
    semu_error_clear(&err);
    semu_cli_debug_init(NULL);
    SEMU_TEST_ASSERT(context, !semu_cli_debug_active(NULL));
    SEMU_TEST_EQ_U64(context, (uint64_t)-1,
        (uint64_t)semu_cli_debug_parse_option(NULL, "--report", "x", &err));
    SEMU_TEST_EQ_U64(context, 0,
        (uint64_t)semu_cli_debug_validate(NULL, &err));
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_report_option),
        SEMU_TEST_CASE(test_snapshot_options),
        SEMU_TEST_CASE(test_trace_capacity),
        SEMU_TEST_CASE(test_trace_overflow),
        SEMU_TEST_CASE(test_invalid_trace_capacity),
        SEMU_TEST_CASE(test_invalid_trace_overflow),
        SEMU_TEST_CASE(test_unknown_option),
        SEMU_TEST_CASE(test_missing_value),
        SEMU_TEST_CASE(test_same_load_save_path),
        SEMU_TEST_CASE(test_different_load_save_path),
        SEMU_TEST_CASE(test_no_debug_options_inactive),
        SEMU_TEST_CASE(test_null_safety),
        SEMU_TEST_CASE(test_first_frame_gate),
        SEMU_TEST_CASE(test_first_frame_gate_requires_press)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
