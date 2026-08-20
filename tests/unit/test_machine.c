#include "semu/hash.h"
#include "semu/machine.h"
#include "sapporo_flash.h"
#include "test.h"
#include "../../src/boards/machine_internal.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int write_bytes(const char *path, const uint8_t *program, size_t size)
{
    FILE *stream = fopen(path, "wb");
    if (stream == NULL) {
        return 0;
    }
    if (fwrite(program, 1u, size, stream) != size) {
        (void)fclose(stream);
        return 0;
    }
    return fclose(stream) == 0;
}

static int write_program(const char *path, uint8_t program[34])
{
    return write_bytes(path, program, 34u);
}

static unsigned count_reset_requests(FILE *stream)
{
    char line[512];
    unsigned count = 0u;

    if (stream == NULL || fseek(stream, 0L, SEEK_SET) != 0) {
        return 0u;
    }
    while (fgets(line, sizeof(line), stream) != NULL) {
        if (strstr(line, "event=machine-reset-request") != NULL) {
            ++count;
        }
    }
    return count;
}

static int reset_log_contains(FILE *stream, const char *text)
{
    char line[1024];

    if (stream == NULL || text == NULL || fseek(stream, 0L, SEEK_SET) != 0) {
        return 0;
    }
    while (fgets(line, sizeof(line), stream) != NULL) {
        if (strstr(line, "event=machine-reset-request") != NULL &&
            strstr(line, text) != NULL) {
            return 1;
        }
    }
    return 0;
}

static int write_full_flash(const char *path)
{
    FILE *stream = fopen(path, "wb");
    if (stream == NULL) return 0;
    if (fseek(stream, 0x02000000L - 1L, SEEK_SET) != 0 ||
        fputc(0xff, stream) == EOF || fseek(stream, 0x00fc0000L, SEEK_SET) != 0 ||
        fputs("1VSF", stream) < 0) {
        (void)fclose(stream);
        return 0;
    }
    return fclose(stream) == 0;
}

typedef struct input_poll_fixture {
    unsigned calls;
    semu_status input_status;
} input_poll_fixture;

static void no_op_event(void *context, uint64_t now_ns)
{
    (void)context;
    (void)now_ns;
}

static semu_stop_reason stop_from_input_poll(void *context,
                                              semu_machine *machine,
                                              semu_error *error)
{
    input_poll_fixture *fixture = (input_poll_fixture *)context;
    semu_input_event event = {
        SEMU_INPUT_BUTTON, SEMU_BUTTON_UPPER, 0, 0, 0
    };
    ++fixture->calls;
    fixture->input_status = semu_machine_input(machine, &event, error);
    return SEMU_STOP_USER;
}

static void copy_text(char *destination, size_t capacity, const char *source)
{
    (void)snprintf(destination, capacity, "%s", source);
}

static int make_contract(const char *path, semu_profile *profile,
                         semu_firmware_manifest *firmware, semu_error *error)
{
    semu_component *required;
    semu_component *component;
    uint64_t size;
    memset(profile, 0, sizeof(*profile));
    memset(firmware, 0, sizeof(*firmware));
    profile->format = 1u;
    copy_text(profile->id, sizeof(profile->id), "sapporo-2.22.60");
    copy_text(profile->board, sizeof(profile->board), "sapporo");
    copy_text(profile->product, sizeof(profile->product), "Synthetic Sapporo");
    copy_text(profile->version, sizeof(profile->version), "machine-test");
    profile->display_width = 240u;
    profile->display_height = 240u;
    profile->required_count = 1u;

    firmware->format = 1u;
    copy_text(firmware->product, sizeof(firmware->product), profile->product);
    copy_text(firmware->version, sizeof(firmware->version), profile->version);
    firmware->component_count = 1u;
    component = &firmware->components[0];
    copy_text(component->id, sizeof(component->id), "synthetic-reset");
    copy_text(component->role, sizeof(component->role), "application");
    copy_text(component->path, sizeof(component->path), path);
    component->load_address = 0u;
    if (semu_sha256_file(path, component->sha256, &size, error) != SEMU_OK) {
        return 0;
    }
    component->size = size;
    required = &profile->required[0];
    *required = *component;
    required->path[0] = '\0';
    return 1;
}

static void test_repeated_reset_and_source_guard(semu_test_context *context)
{
    uint8_t program[34] = {
        0x00u, 0x01u, 0x00u, 0x10u, /* MSP = 0x10000100 */
        0x21u, 0x00u, 0x00u, 0x00u, /* reset = 0x00000021 */
        [32] = 0x00u, [33] = 0xbeu  /* BKPT */
    };
    uint8_t source_before[SEMU_SHA256_SIZE];
    uint8_t source_after[SEMU_SHA256_SIZE];
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_run_limits limits = {8u, 8u};
    semu_profile profile;
    semu_machine *machine;
    semu_error error;
    char path[128];
    char flash_path[128];
    uint64_t size;
    uint64_t first_instructions;
    uint64_t first_time;
    uint32_t first_pc;

    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(path, sizeof(path), "machine.bin"));
    SEMU_TEST_ASSERT(context, write_program(path, program));
    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(flash_path, sizeof(flash_path), "machine-flash.bin"));
    SEMU_TEST_ASSERT(context, write_full_flash(flash_path));
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context,
        make_contract(path, &profile, &firmware, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sha256_file(path, source_before, &size, &error));
    memset(&options, 0, sizeof(options));
    options.profile = &profile;
    options.firmware = &firmware;
    options.external_flash_path = flash_path;
    machine = semu_machine_create(&options, &error);
    SEMU_TEST_ASSERT(context, machine != NULL);

    SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
        semu_machine_run(machine, &limits, &error));
    first_instructions = semu_machine_instructions(machine);
    first_time = semu_machine_virtual_time(machine);
    first_pc = semu_machine_program_counter(machine);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_reset(machine, &error));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
        semu_machine_run(machine, &limits, &error));
    SEMU_TEST_EQ_U64(context, first_instructions,
                     semu_machine_instructions(machine));
    SEMU_TEST_EQ_U64(context, first_time, semu_machine_virtual_time(machine));
    SEMU_TEST_EQ_U64(context, first_pc, semu_machine_program_counter(machine));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sha256_file(path, source_after, &size, &error));
    SEMU_TEST_ASSERT(context,
        memcmp(source_before, source_after, sizeof(source_before)) == 0);

    program[33] = 0xbfu;
    SEMU_TEST_ASSERT(context, write_program(path, program));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                     semu_machine_reset(machine, &error));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_DEVICE_REFUSED,
                     semu_machine_stop_reason(machine));
    semu_machine_destroy(machine);
    (void)remove(path);
    (void)remove(flash_path);
}

static void test_requested_reset_retains_ram_explicit_clears(
    semu_test_context *context)
{
    static const uint8_t program[72] = {
        0x00u, 0x01u, 0x00u, 0x10u, /* MSP = 0x10000100 */
        0x21u, 0x00u, 0x00u, 0x00u, /* reset = 0x00000021 */
        [0x20] = 0x06u, 0x48u,       /* ldr r0, [pc, #24] */
        [0x22] = 0x01u, 0x78u,       /* ldrb r1, [r0] */
        [0x24] = 0x00u, 0x29u,       /* cmp r1, #0 */
        [0x26] = 0x05u, 0xd1u,       /* bne retained */
        [0x28] = 0x5au, 0x21u,       /* movs r1, #0x5a */
        [0x2a] = 0x01u, 0x70u,       /* strb r1, [r0] */
        [0x2c] = 0x04u, 0x48u,       /* ldr r0, [pc, #16] */
        [0x2e] = 0x05u, 0x49u,       /* ldr r1, [pc, #20] */
        [0x30] = 0x01u, 0x60u,       /* str r1, [r0] -> SYSRESETREQ */
        [0x32] = 0x00u, 0xbeu,       /* unexpected fall-through */
        [0x34] = 0x00u, 0xbeu,       /* retained: halt */
        [0x3c] = 0x00u, 0x00u, 0x00u, 0x10u, /* SRAM marker */
        [0x40] = 0x0cu, 0xedu, 0x00u, 0xe0u, /* SCB AIRCR */
        [0x44] = 0x04u, 0x00u, 0xfau, 0x05u  /* VECTKEY | SYSRESETREQ */
    };
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_profile profile;
    semu_machine *machine;
    semu_logger logger;
    semu_run_limits limits = {32u, UINT64_C(1000000)};
    semu_error error;
    FILE *log_stream;
    char path[128];

    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(path, sizeof(path), "machine-reset-retention.bin"));
    SEMU_TEST_ASSERT(context, write_bytes(path, program, sizeof(program)));
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, make_contract(path, &profile, &firmware, &error));
    log_stream = tmpfile();
    SEMU_TEST_ASSERT(context, log_stream != NULL);
    semu_log_init(&logger, log_stream, SEMU_LOG_WARNING);
    memset(&options, 0, sizeof(options));
    options.profile = &profile;
    options.firmware = &firmware;
    options.logger = &logger;
    machine = semu_machine_create(&options, &error);
    SEMU_TEST_ASSERT(context, machine != NULL);

    SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
        semu_machine_run(machine, &limits, &error));
    SEMU_TEST_EQ_U64(context, 1u, count_reset_requests(log_stream));
    SEMU_TEST_ASSERT(context,
        reset_log_contains(log_stream,
            "pc=0x00000032 lr=0x00000000 sp=0x10000100 "
            "r0=0xe000ed0c r1=0x05fa0004 r2=0x00000000 "
            "r3=0x00000000 xpsr=0x21000000 reset_count=1 "
            "compat_hits=0 instructions=9 virtual_time_ns=9"));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_reset(machine, &error));
    SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
        semu_machine_run(machine, &limits, &error));
    SEMU_TEST_EQ_U64(context, 2u, count_reset_requests(log_stream));
    SEMU_TEST_ASSERT(context,
        reset_log_contains(log_stream, "reset_count=2"));

    semu_machine_destroy(machine);
    (void)fclose(log_stream);
    (void)remove(path);
}

static void test_input_poll_can_stop_and_inject(semu_test_context *context)
{
    uint8_t program[34] = {
        0x00u, 0x01u, 0x00u, 0x10u,
        0x21u, 0x00u, 0x00u, 0x00u,
        [32] = 0x00u, [33] = 0xbeu
    };
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_profile profile;
    semu_machine *machine;
    semu_run_limits limits = { 8u, 8u };
    input_poll_fixture poll = { 0u, SEMU_ERR_STATE };
    semu_error error;
    char path[128];

    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(path, sizeof(path), "machine-input.bin"));
    SEMU_TEST_ASSERT(context, write_program(path, program));
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, make_contract(path, &profile, &firmware, &error));
    memset(&options, 0, sizeof(options));
    options.profile = &profile;
    options.firmware = &firmware;
    options.input_poll = stop_from_input_poll;
    options.input_poll_context = &poll;
    machine = semu_machine_create(&options, &error);
    SEMU_TEST_ASSERT(context, machine != NULL);
    if (machine != NULL) {
        SEMU_TEST_EQ_U64(context, SEMU_STOP_USER,
                         semu_machine_run(machine, &limits, &error));
        SEMU_TEST_EQ_U64(context, 1u, poll.calls);
        SEMU_TEST_EQ_U64(context, SEMU_OK, poll.input_status);
        semu_machine_destroy(machine);
    }
    (void)remove(path);
}

static void test_button_input_polarity_and_refusal(
    semu_test_context *context)
{
    static const uint8_t program[64] = {
        0x00u, 0x01u, 0x00u, 0x10u, /* MSP = 0x10000100 */
        0x21u, 0x00u, 0x00u, 0x00u, /* reset = 0x00000021 */
        [0x20] = 0x06u, 0x48u,       /* ldr r0, [pc, #24] */
        [0x22] = 0x01u, 0x68u,       /* ldr r1, [r0] */
        [0x24] = 0x89u, 0x0eu,       /* lsrs r1, r1, #26 */
        [0x26] = 0x01u, 0x22u,       /* movs r2, #1 */
        [0x28] = 0x11u, 0x40u,       /* ands r1, r2 */
        [0x2a] = 0x00u, 0x29u,       /* cmp r1, #0 */
        [0x2c] = 0x00u, 0xd0u,       /* beq pressed */
        [0x2e] = 0x00u, 0xbeu,       /* released: halt */
        [0x30] = 0x30u, 0xbfu,       /* pressed: wfi */
        [0x32] = 0x00u, 0xbeu,       /* unexpected wake: halt */
        [0x3c] = 0x08u, 0x02u, 0x01u, 0x40u /* GPIO input bank 1 */
    };
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_profile profile;
    semu_machine *machine;
    semu_run_limits limits = { 32u, UINT64_C(1000000) };
    semu_input_event event;
    semu_error error;
    char path[128];

    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(path, sizeof(path), "machine-button.bin"));
    SEMU_TEST_ASSERT(context, write_bytes(path, program, sizeof(program)));
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, make_contract(path, &profile, &firmware, &error));
    memset(&options, 0, sizeof(options));
    options.profile = &profile;
    options.firmware = &firmware;
    machine = semu_machine_create(&options, &error);
    SEMU_TEST_ASSERT(context, machine != NULL);
    if (machine != NULL) {
        event = (semu_input_event){
            SEMU_INPUT_BUTTON, 3u, 0, 0, 0
        };
        SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                         semu_machine_input(machine, &event, &error));
        SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
                         semu_machine_run(machine, &limits, &error));

        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_machine_reset(machine, &error));
        event.code = SEMU_BUTTON_MIDDLE;
        event.value = 0;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_machine_input(machine, &event, &error));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_scheduler_schedule(machine->scheduler, 20u, no_op_event,
                                    NULL, NULL, &error));
        SEMU_TEST_EQ_U64(context, SEMU_STOP_WFI_DEADLOCK,
                         semu_machine_run(machine, &limits, &error));
        SEMU_TEST_EQ_U64(context, 20u, semu_machine_virtual_time(machine));

        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_machine_reset(machine, &error));
        event.value = 1;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_machine_input(machine, &event, &error));
        SEMU_TEST_EQ_U64(context, SEMU_STOP_HALT,
                         semu_machine_run(machine, &limits, &error));
        semu_machine_destroy(machine);
    }
    (void)remove(path);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_repeated_reset_and_source_guard),
        SEMU_TEST_CASE(test_requested_reset_retains_ram_explicit_clears),
        SEMU_TEST_CASE(test_input_poll_can_stop_and_inject),
        SEMU_TEST_CASE(test_button_input_polarity_and_refusal)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
