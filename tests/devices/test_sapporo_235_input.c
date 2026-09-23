/*
 * Sapporo-2.35.34 input-driven phase records (E-SAP-0037, ticket 710
 * instance-7).
 *
 * The boot parks in WFI at pc 0x000e1862 (E-SAP-0036).  This module
 * drives the machine through the public API with the documented
 * 4096-instruction input poll and a pinned eight-press button timeline
 * (lower, middle, upper at 20/21/22 s, 30/31/32 s, lower and middle at
 * 60/61 s, each held 300 ms), so the census can name which code paths a
 * semantic press serves after the park.  The run is taken in slices of
 * 200000 instructions and 100000000 ns per call, the configuration under
 * which every total below was observed, twice byte-identically each,
 * through the probe harness /tmp/sap235/pressus.c (the 100000-instruction
 * command-line chunking of --input-replay is a different quantization and
 * its own record is kept in the contract file).
 *
 * Observed laws: a press opens the scheduler drain at 0xa6bea (the
 * one-deep depth counter at struct+0x74 is incremented by the push helper
 * at 0xa6bb6 and decremented per pop at 0xa6c04, the loop runs while it is
 * non-zero), the event bitmap at +0x54 takes 1 << index by the dispatch
 * arm at 0xa6c2c-0xa6c30, the service callback table is indexed 0x14
 * bytes per entry at 0xa6c34, the queue-emptiness predicate at 0xa7042
 * answers through the count at +0x60 and the head/capacity pair
 * +0x38/+0x4c, the 64-bit now-stamp is committed to +0x6c/+0x70 at
 * 0xa754e with carry, and the wake path tests the SCR bit 4 and the
 * low-power timer bit 20 at 0xe1818-0xe182a before the park again.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "semu/machine.h"
#include "test.h"

enum {
    INPUT_TIMELINE_MAX = 32u,
    HOLD_NS = 300000000u,
    SLICE_INSTRUCTIONS = 200000u,
    SLICE_VIRTUAL_TIME_NS = 100000000u
};

typedef struct {
    uint64_t time_ns;
    unsigned long button;
    unsigned long down;
} timeline_entry;

static timeline_entry g_queue[INPUT_TIMELINE_MAX];
static size_t g_queue_count;
static size_t g_queue_cursor;

static int timeline_button_of(char letter)
{
    if (letter == 'l') {
        return (int)SEMU_BUTTON_LOWER;
    }
    if (letter == 'm') {
        return (int)SEMU_BUTTON_MIDDLE;
    }
    if (letter == 'u') {
        return (int)SEMU_BUTTON_UPPER;
    }
    return -1;
}

/* Grammar: "<ms>:<l|m|u>[,<ms>:<l|m|u>..."; each entry expands to the
 * press at the stated time and the release 300 ms later.  A malformed
 * entry is rejected as a (size_t)-1 sentinel; a trailing comma is
 * tolerated. */
static size_t parse_timeline(const char *text)
{
    const char *cursor = text;
    size_t count = 0u;
    g_queue_count = 0u;
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    while (*cursor != '\0') {
        char *end;
        unsigned long long ms;
        int button;
        if (count >= (size_t)INPUT_TIMELINE_MAX / 2u ||
            *cursor < '0' || *cursor > '9') {
            return (size_t)-1;
        }
        errno = 0;
        ms = strtoull(cursor, &end, 10);
        if (errno == ERANGE || end == cursor || *end != ':' ||
            ms > (UINT64_MAX - (uint64_t)HOLD_NS) / UINT64_C(1000000)) {
            return (size_t)-1;
        }
        button = timeline_button_of(end[1]);
        if (button < 0) {
            return (size_t)-1;
        }
        cursor = end + 2;
        if (*cursor == ',') {
            ++cursor;
        } else if (*cursor != '\0') {
            return (size_t)-1;
        }
        g_queue[g_queue_count].time_ns = (uint64_t)ms * 1000000u;
        g_queue[g_queue_count].button = (unsigned long)button;
        g_queue[g_queue_count].down = 1u;
        ++g_queue_count;
        g_queue[g_queue_count].time_ns =
            (uint64_t)ms * 1000000u + (uint64_t)HOLD_NS;
        g_queue[g_queue_count].button = (unsigned long)button;
        g_queue[g_queue_count].down = 0u;
        ++g_queue_count;
        ++count;
    }
    return count;
}

static semu_stop_reason poll_timeline(void *context, semu_machine *machine,
                                      semu_error *error)
{
    uint64_t now = semu_machine_virtual_time(machine);
    (void)context;
    while (g_queue_cursor < g_queue_count &&
           g_queue[g_queue_cursor].time_ns <= now) {
        semu_input_event input;
        memset(&input, 0, sizeof(input));
        input.kind = SEMU_INPUT_BUTTON;
        input.code = (uint32_t)g_queue[g_queue_cursor].button;
        input.value = g_queue[g_queue_cursor].down != 0u ? 1u : 0u;
        if (semu_machine_input(machine, &input, error) != SEMU_OK) {
            return SEMU_STOP_USER;
        }
        ++g_queue_cursor;
    }
    return SEMU_STOP_NONE;
}

static semu_machine *open_235(const char *timeline, semu_error *error,
                              size_t *presses)
{
    const char *manifest_path = getenv("SEMU_SAPPORO_235_FIRMWARE_MANIFEST");
    static semu_firmware_manifest firmware;
    static semu_profile profile;
    FILE *probe;
    semu_machine_options options;
    memset(&options, 0, sizeof(options));
    if (manifest_path == NULL || manifest_path[0] == '\0') {
        manifest_path = "tests/private/sapporo-2.35.34.18929/firmware.semu";
    }
    probe = fopen(manifest_path, "rb");
    if (probe == NULL) {
        const char *configured = getenv("SEMU_SAPPORO_235_FIRMWARE_MANIFEST");
        if (errno != ENOENT || (configured != NULL && configured[0] != '\0')) {
            semu_error_set(error, SEMU_ERR_IO,
                           "cannot open Sapporo 2.35 fixture: %s", manifest_path);
        }
        return NULL;
    }
    fclose(probe);
    if (semu_profile_load("profiles/sapporo/2.35.34/profile.semu",
                          &profile, error) != SEMU_OK) {
        return NULL;
    }
    if (semu_manifest_load(manifest_path, &firmware, error) != SEMU_OK) {
        return NULL;
    }
    *presses = parse_timeline(timeline);
    if (*presses == (size_t)-1) {
        semu_error_set(error, SEMU_ERR_FORMAT, "invalid Sapporo input timeline");
        return NULL;
    }
    g_queue_cursor = 0u;
    options.profile = &profile;
    options.firmware = &firmware;
    options.input_poll = poll_timeline;
    options.input_poll_context = NULL;
    return semu_machine_create(&options, error);
}

static void run_sliced(semu_machine *machine, uint64_t cap_instructions,
                        uint64_t cap_virtual_time_ns, semu_error *error)
{
    for (;;) {
        semu_run_limits limits;
        uint64_t done_i = semu_machine_instructions(machine);
        uint64_t done_v = semu_machine_virtual_time(machine);
        uint64_t left_i;
        uint64_t left_v;
        if (done_i >= cap_instructions || done_v >= cap_virtual_time_ns) {
            return;
        }
        left_i = cap_instructions - done_i;
        left_v = cap_virtual_time_ns - done_v;
        limits.max_instructions =
            (SLICE_INSTRUCTIONS < left_i) ? SLICE_INSTRUCTIONS : left_i;
        limits.max_virtual_time_ns =
            (SLICE_VIRTUAL_TIME_NS < left_v) ? SLICE_VIRTUAL_TIME_NS : left_v;
        if (semu_machine_run(machine, &limits, error) != SEMU_STOP_BUDGET) {
            return;
        }
        if (semu_machine_instructions(machine) == done_i &&
            semu_machine_virtual_time(machine) == done_v) {
            semu_error_set(error, SEMU_ERR_STATE, "Sapporo input run made no progress");
            return;
        }
    }
}

static void test_input_run_refusal_returns(semu_test_context *context)
{
    semu_error error;
    semu_error_clear(&error);
    run_sliced(NULL, 1u, 1u, &error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT, error.code);
}

static void test_input_timeline_refusals(semu_test_context *context)
{
    size_t presses;
    /* The pinned full timeline parses into sixteen queue entries. */
    presses = parse_timeline(
        "20000:l,21000:m,22000:u,30000:l,31000:m,32000:u,60000:l,61000:m");
    SEMU_TEST_EQ_U64(context, 8u, (uint64_t)presses);
    SEMU_TEST_EQ_U64(context, 16u, (uint64_t)g_queue_count);
    SEMU_TEST_EQ_U64(context, UINT64_C(20000000000), g_queue[0].time_ns);
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_BUTTON_LOWER,
                     g_queue[0].button);
    SEMU_TEST_EQ_U64(context, 1u, g_queue[0].down);
    SEMU_TEST_EQ_U64(context, UINT64_C(20300000000), g_queue[1].time_ns);
    SEMU_TEST_EQ_U64(context, 0u, g_queue[1].down);
    SEMU_TEST_EQ_U64(context, UINT64_C(61300000000), g_queue[15].time_ns);
    /* A trailing comma is tolerated. */
    SEMU_TEST_EQ_U64(context, 1u,
                      (uint64_t)parse_timeline("20000:l,"));
    /* Refusal cases: every malformed entry is rejected by the sentinel. */
    SEMU_TEST_EQ_U64(context, (uint64_t)(size_t)-1,
                      (uint64_t)parse_timeline("20000;l"));
    SEMU_TEST_EQ_U64(context, (uint64_t)(size_t)-1,
                      (uint64_t)parse_timeline("20000:x"));
    SEMU_TEST_EQ_U64(context, (uint64_t)(size_t)-1,
                      (uint64_t)parse_timeline("20000:"));
    SEMU_TEST_EQ_U64(context, (uint64_t)(size_t)-1,
                      (uint64_t)parse_timeline("abc:l"));
    SEMU_TEST_EQ_U64(context, (uint64_t)(size_t)-1,
                      (uint64_t)parse_timeline("20000:l;30000:m"));
    /* The empty timeline is accepted as zero presses. */
    SEMU_TEST_EQ_U64(context, 0u, (uint64_t)parse_timeline(""));
}

static void test_input_timeline_capacity(semu_test_context *context)
{
    char timeline[192] = "";
    size_t i;
    for (i = 0u; i < INPUT_TIMELINE_MAX / 2u; ++i) {
        (void)strcat(timeline, "1000:l,");
    }
    SEMU_TEST_EQ_U64(context, INPUT_TIMELINE_MAX / 2u,
                     parse_timeline(timeline));
    SEMU_TEST_EQ_U64(context, INPUT_TIMELINE_MAX, g_queue_count);
    (void)strcat(timeline, "2000:m");
    SEMU_TEST_EQ_U64(context, (size_t)-1, parse_timeline(timeline));
}

static void test_input_timeline_time_overflow(semu_test_context *context)
{
    SEMU_TEST_EQ_U64(context, (size_t)-1, parse_timeline("-1:m"));
    SEMU_TEST_EQ_U64(context, (size_t)-1,
                     parse_timeline("18446744073709551616:l"));
    SEMU_TEST_EQ_U64(context, (size_t)-1,
                     parse_timeline("18446744073710:l"));
    SEMU_TEST_EQ_U64(context, 1u, parse_timeline("18446744073409:l"));
    SEMU_TEST_EQ_U64(context, UINT64_C(18446744073709000000),
                     g_queue[1].time_ns);
    SEMU_TEST_EQ_U64(context, (size_t)-1,
                     parse_timeline("18446744073410:l"));
}

static void test_input_timeline_boot_record(semu_test_context *context)
{
    const char *timeline =
        "20000:l,21000:m,22000:u,30000:l,31000:m,32000:u,60000:l,61000:m";
    semu_error error;
    size_t presses = 0u;
    semu_machine *machine;
    semu_error_clear(&error);
    machine = open_235(timeline, &error, &presses);
    if (machine == NULL) {
        if (error.code != SEMU_OK) fprintf(stderr, "%s\n", error.text);
        SEMU_TEST_EQ_U64(context, SEMU_OK, error.code);
        printf("SKIP sapporo 2.35.34 private fixture unavailable\n");
        return;
    }
    SEMU_TEST_EQ_U64(context, 8u, (uint64_t)presses);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_reset(machine, &error));
    run_sliced(machine, UINT64_C(400000000), UINT64_C(300000000000),
               &error);
    /* At the 400000000-instruction budget the instruction grant is spent
     * first: the stop is the budget, eight of the sixteen queue entries
     * (through the release at 30300 ms) have been delivered, and the
     * last observed program counter is the depth-counter return of the
     * scheduler drain. */
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_STOP_BUDGET,
                     (uint64_t)semu_machine_stop_reason(machine));
    SEMU_TEST_EQ_U64(context, UINT64_C(400000000),
                     semu_machine_instructions(machine));
    SEMU_TEST_EQ_U64(context, UINT64_C(30891199362),
                     semu_machine_virtual_time(machine));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x000bdc10),
                     semu_machine_program_counter(machine));
    SEMU_TEST_EQ_U64(context, 8u, (uint64_t)g_queue_cursor);
    semu_machine_destroy(machine);
}

static void test_input_timeline_full_record(semu_test_context *context)
{
    const char *timeline =
        "20000:l,21000:m,22000:u,30000:l,31000:m,32000:u,60000:l,61000:m";
    semu_error error;
    size_t presses = 0u;
    semu_machine *machine;
    semu_error_clear(&error);
    machine = open_235(timeline, &error, &presses);
    if (machine == NULL) {
        if (error.code != SEMU_OK) fprintf(stderr, "%s\n", error.text);
        SEMU_TEST_EQ_U64(context, SEMU_OK, error.code);
        printf("SKIP sapporo 2.35.34 private fixture unavailable\n");
        return;
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_reset(machine, &error));
    run_sliced(machine, UINT64_C(1000000000), UINT64_C(300000000000),
               &error);
    /* The instruction budget arrives at 49.84 s, before the 300 s time
     * limit: twelve entries are in (through the release at 32300 ms). */
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_STOP_BUDGET,
                     (uint64_t)semu_machine_stop_reason(machine));
    SEMU_TEST_EQ_U64(context, UINT64_C(1000000000),
                     semu_machine_instructions(machine));
    SEMU_TEST_EQ_U64(context, UINT64_C(49836877598),
                     semu_machine_virtual_time(machine));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x000a72cc),
                     semu_machine_program_counter(machine));
    SEMU_TEST_EQ_U64(context, 12u, (uint64_t)g_queue_cursor);
    semu_machine_destroy(machine);
}

static void test_input_timeline_neutral_control(semu_test_context *context)
{
    semu_error error;
    size_t presses = 0u;
    semu_machine *machine;
    semu_error_clear(&error);
    machine = open_235("", &error, &presses);
    if (machine == NULL) {
        if (error.code != SEMU_OK) fprintf(stderr, "%s\n", error.text);
        SEMU_TEST_EQ_U64(context, SEMU_OK, error.code);
        printf("SKIP sapporo 2.35.34 private fixture unavailable\n");
        return;
    }
    SEMU_TEST_EQ_U64(context, 0u, (uint64_t)presses);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_reset(machine, &error));
    run_sliced(machine, UINT64_C(1000000000), UINT64_C(300000000000),
               &error);
    /* With no presses the slice-driven observer is neutral: the control
     * reproduces the E-SAP-0036 300 s boot record exactly. */
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_STOP_BUDGET,
                     (uint64_t)semu_machine_stop_reason(machine));
    SEMU_TEST_EQ_U64(context, UINT64_C(122878688),
                     semu_machine_instructions(machine));
    SEMU_TEST_EQ_U64(context, UINT64_C(300000000000),
                     semu_machine_virtual_time(machine));
    SEMU_TEST_EQ_U64(context, UINT64_C(0x000e1862),
                     semu_machine_program_counter(machine));
    SEMU_TEST_EQ_U64(context, 0u, (uint64_t)g_queue_cursor);
    semu_machine_destroy(machine);
}

int main(void)
{
    static const semu_test_case cases[] = {
        { "test_input_timeline_refusals", test_input_timeline_refusals },
        SEMU_TEST_CASE(test_input_run_refusal_returns),
        SEMU_TEST_CASE(test_input_timeline_capacity),
        SEMU_TEST_CASE(test_input_timeline_time_overflow),
        { "test_input_timeline_boot_record",
          test_input_timeline_boot_record },
        { "test_input_timeline_full_record",
          test_input_timeline_full_record },
        { "test_input_timeline_neutral_control",
          test_input_timeline_neutral_control }
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
