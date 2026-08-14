#include "cli.h"

#include "semu/hash.h"
#include "semu/machine.h"
#include "semu/manifest.h"
#include "semu/types.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_INSTRUCTION_BUDGET 10000000u

typedef struct run_arguments {
    const char *profile;
    const char *firmware;
    const char *trace;
    const char *until;
    const char *layers[SEMU_MAX_LAYERS];
    size_t layer_count;
    uint64_t max_time;
} run_arguments;

static void usage(FILE *stream)
{
    fprintf(stream,
            "usage:\n"
            "  suunto-emu list\n"
            "  suunto-emu show-profile PROFILE\n"
            "  suunto-emu validate --profile PROFILE --firmware MANIFEST\n"
            "  suunto-emu list-layers --profile PROFILE\n"
            "  suunto-emu run --profile PROFILE --firmware MANIFEST "
            "[--layer ID] [--until wfi] [--max-time NS] "
            "[--trace PATH] [--headless]\n");
}

static const char *profile_path(const char *argument)
{
    if (argument != NULL && strcmp(argument, "sapporo-2.22.60") == 0) {
        return "profiles/sapporo/2.22.60/profile.semu";
    }
    return argument;
}

static int load_profile(const char *argument, semu_profile *profile,
                        semu_error *error)
{
    if (argument == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "--profile is required");
        return 0;
    }
    return semu_profile_load(profile_path(argument), profile, error) == SEMU_OK;
}

static int parse_u64(const char *text, uint64_t *value)
{
    char *end;
    unsigned long long result;
    errno = 0;
    result = strtoull(text, &end, 0);
    if (errno != 0 || text[0] == '\0' || *end != '\0') {
        return 0;
    }
    *value = (uint64_t)result;
    return 1;
}

static int parse_options(int argc, char **argv, int start,
                         run_arguments *arguments, semu_error *error)
{
    int index;
    memset(arguments, 0, sizeof(*arguments));
    for (index = start; index < argc; ++index) {
        const char *option = argv[index];
        const char *value;
        if (strcmp(option, "--headless") == 0) {
            continue;
        }
        if (index + 1 >= argc) {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                           "option %s requires a value", option);
            return 0;
        }
        value = argv[++index];
        if (strcmp(option, "--profile") == 0) {
            arguments->profile = value;
        } else if (strcmp(option, "--firmware") == 0) {
            arguments->firmware = value;
        } else if (strcmp(option, "--trace") == 0) {
            arguments->trace = value;
        } else if (strcmp(option, "--until") == 0) {
            arguments->until = value;
        } else if (strcmp(option, "--max-time") == 0) {
            if (!parse_u64(value, &arguments->max_time)) {
                semu_error_set(error, SEMU_ERR_ARGUMENT,
                               "invalid --max-time value %s", value);
                return 0;
            }
        } else if (strcmp(option, "--layer") == 0) {
            if (arguments->layer_count >= SEMU_MAX_LAYERS) {
                semu_error_set(error, SEMU_ERR_RANGE, "too many --layer options");
                return 0;
            }
            arguments->layers[arguments->layer_count++] = value;
        } else {
            semu_error_set(error, SEMU_ERR_ARGUMENT, "unknown option %s", option);
            return 0;
        }
    }
    return 1;
}

static int command_list(void)
{
    puts("sapporo-2.22.60  Sapporo  2.22.60.3383-P  interpreter-bring-up");
    return 0;
}

static int command_show(const char *path)
{
    semu_profile profile;
    semu_error error;
    size_t i;
    semu_error_clear(&error);
    if (!load_profile(path, &profile, &error)) {
        fprintf(stderr, "show-profile: %s\n", error.text);
        return 2;
    }
    printf("id=%s\nboard=%s\nproduct=%s\nversion=%s\n",
           profile.id, profile.board, profile.product, profile.version);
    printf("vector_table=0x%08x\ndisplay=%ux%u\n",
           profile.vector_table, profile.display_width, profile.display_height);
    for (i = 0u; i < profile.required_count; ++i) {
        char digest[65];
        semu_sha256_format(profile.required[i].sha256, digest);
        printf("component=%s role=%s load=0x%08x size=%llu sha256=%s\n",
               profile.required[i].id, profile.required[i].role,
               profile.required[i].load_address,
               (unsigned long long)profile.required[i].size, digest);
    }
    return 0;
}

static int command_layers(const char *path)
{
    semu_profile profile;
    semu_error error;
    size_t i;
    semu_error_clear(&error);
    if (!load_profile(path, &profile, &error)) {
        fprintf(stderr, "list-layers: %s\n", error.text);
        return 2;
    }
    for (i = 0u; i < profile.layer_count; ++i) {
        printf("%s (disabled by default)\n", profile.layers[i]);
    }
    return 0;
}

static int load_and_validate(const run_arguments *arguments,
                             semu_profile *profile,
                             semu_firmware_manifest *firmware,
                             semu_error *error)
{
    if (!load_profile(arguments->profile, profile, error)) {
        return 0;
    }
    if (arguments->firmware == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "--firmware is required");
        return 0;
    }
    if (semu_manifest_load(arguments->firmware, firmware, error) != SEMU_OK) {
        return 0;
    }
    return semu_manifest_validate(profile, firmware, error) == SEMU_OK;
}

static int command_validate(const run_arguments *arguments)
{
    semu_profile profile;
    semu_firmware_manifest firmware;
    semu_error error;
    semu_error_clear(&error);
    if (!load_and_validate(arguments, &profile, &firmware, &error)) {
        fprintf(stderr, "validate: %s\n", error.text);
        return 2;
    }
    printf("valid profile=%s product=%s version=%s components=%lu\n",
           profile.id, firmware.product, firmware.version,
           (unsigned long)firmware.component_count);
    return 0;
}

static int command_run(const run_arguments *arguments,
                       semu_frame_callback frame_callback, void *frame_context)
{
    semu_profile profile;
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_machine *machine;
    semu_run_limits limits;
    semu_logger logger;
    semu_error error;
    FILE *trace = stderr;
    semu_stop_reason reason;
    semu_error_clear(&error);
    if (arguments->until != NULL && strcmp(arguments->until, "wfi") != 0) {
        fprintf(stderr, "run: --until currently accepts only wfi\n");
        return 2;
    }
    if (!load_and_validate(arguments, &profile, &firmware, &error)) {
        fprintf(stderr, "run: %s\n", error.text);
        return 2;
    }
    if (arguments->trace != NULL) {
        trace = fopen(arguments->trace, "w");
        if (trace == NULL) {
            fprintf(stderr, "run: cannot open trace %s\n", arguments->trace);
            return 2;
        }
    }
    semu_log_init(&logger, trace, SEMU_LOG_INFO);
    memset(&options, 0, sizeof(options));
    options.profile = &profile;
    options.firmware = &firmware;
    options.layers = arguments->layers;
    options.layer_count = arguments->layer_count;
    options.logger = &logger;
    options.frame_callback = frame_callback;
    options.frame_context = frame_context;
    machine = semu_machine_create(&options, &error);
    if (machine == NULL) {
        fprintf(stderr, "run: %s\n", error.text);
        if (trace != stderr) {
            fclose(trace);
        }
        return 2;
    }
    limits.max_instructions = DEFAULT_INSTRUCTION_BUDGET;
    limits.max_virtual_time_ns = arguments->max_time;
    reason = semu_machine_run(machine, &limits, &error);
    printf("stop=%s pc=0x%08x instructions=%llu virtual_time_ns=%llu",
           semu_stop_reason_name(reason),
           semu_machine_program_counter(machine),
           (unsigned long long)semu_machine_instructions(machine),
           (unsigned long long)semu_machine_virtual_time(machine));
    if (error.code != SEMU_OK) {
        printf(" detail=%s", error.text);
    }
    putchar('\n');
    semu_machine_destroy(machine);
    if (trace != stderr) {
        fclose(trace);
    }
    return reason == SEMU_STOP_WFI_DEADLOCK || reason == SEMU_STOP_HALT ? 0 : 3;
}

int semu_cli_main(int argc, char **argv, semu_frame_callback frame_callback,
                  void *frame_context)
{
    run_arguments arguments;
    semu_error error;
    const char *command;
    semu_error_clear(&error);
    if (argc < 2) {
        usage(stderr);
        return 2;
    }
    command = argv[1];
    if (strcmp(command, "list") == 0 && argc == 2) {
        return command_list();
    }
    if (strcmp(command, "show-profile") == 0 && argc == 3) {
        return command_show(argv[2]);
    }
    if (!parse_options(argc, argv, 2, &arguments, &error)) {
        fprintf(stderr, "%s: %s\n", command, error.text);
        usage(stderr);
        return 2;
    }
    if (strcmp(command, "list-layers") == 0) {
        return command_layers(arguments.profile);
    }
    if (strcmp(command, "validate") == 0) {
        return command_validate(&arguments);
    }
    if (strcmp(command, "run") == 0) {
        return command_run(&arguments, frame_callback, frame_context);
    }
    fprintf(stderr, "unknown command %s\n", command);
    usage(stderr);
    return 2;
}
