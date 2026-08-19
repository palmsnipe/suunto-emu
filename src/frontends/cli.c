#include "cli.h"
#include "cli_debug.h"
#include "cli_debug.c"

#include "semu/hash.h"
#include "semu/machine.h"
#include "semu/manifest.h"
#include "semu/trace.h"
#include "semu/types.h"

#include "../display/nema_backend.h"
#include "../devices/sapporo_flash.h"
#include "input_replay.c"
#include "cli_checkpoint.c"
#include "cli_snapshot.c"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_INSTRUCTION_BUDGET 10000000u
#define CHECKPOINT_INSTRUCTION_LIMIT 10000000u
#define CHECKPOINT_TIME_LIMIT 2000000000u
#define RUN_CHUNK_INSTRUCTIONS 100000u

typedef struct run_arguments {
    const char *profile;
    const char *firmware;
    const char *trace;
    const char *until;
    const char *input_replay;
    const char *full_flash;
    const char *layers[SEMU_MAX_LAYERS];
    size_t layer_count;
    uint64_t max_instructions;
    uint64_t max_time;
    semu_cli_debug_options debug;
} run_arguments;

static void cleanup_run(semu_machine *machine, semu_snapshot *snapshot,
                        semu_input_replay *replay, semu_nema_backend *backend,
                        FILE *trace)
{
    semu_snapshot_destroy(snapshot);
    semu_machine_destroy(machine);
    semu_input_replay_destroy(replay);
    semu_nema_backend_destroy(backend);
    if (trace != stderr) fclose(trace);
}

static void usage(FILE *stream)
{
    fprintf(stream,
            "usage:\n"
            "  suunto-emu list\n"
            "  suunto-emu show-profile PROFILE\n"
            "  suunto-emu validate --profile PROFILE --firmware MANIFEST\n"
            "  suunto-emu list-layers --profile PROFILE\n"
            "  suunto-emu run --profile PROFILE --firmware MANIFEST "
            "[--layer ID] [--full-flash PATH] [--until CHECKPOINT] "
            "[--max-instructions N] [--max-time NS] "
            "[--trace PATH] [--input-replay PATH] [--headless] "
            "[--report PATH] [--snapshot-load PATH] [--snapshot-save PATH] "
            "[--trace-capacity N] [--trace-overflow stop|truncate]\n");
}

static const char *profile_path(const char *argument)
{
    if (argument != NULL && strcmp(argument, "sapporo-2.22.60") == 0) {
        return "profiles/sapporo/2.22.60/profile.semu";
    }
    if (argument != NULL && strcmp(argument, "sapporo-2.33.16") == 0) {
        return "profiles/sapporo/2.33.16/profile.semu";
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
    semu_cli_debug_init(&arguments->debug);
    for (index = start; index < argc; ++index) {
        const char *option = argv[index];
        const char *value;
        if (strcmp(option, "--headless") == 0) {
            continue;
        }
        if (strcmp(option, "--scale") == 0) {
            if (index + 1 >= argc) {
                semu_error_set(error, SEMU_ERR_ARGUMENT,
                               "option %s requires a value", option);
                return 0;
            }
            ++index;
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
        } else if (strcmp(option, "--input-replay") == 0) {
            arguments->input_replay = value;
        } else if (strcmp(option, "--full-flash") == 0) {
            arguments->full_flash = value;
        } else if (strcmp(option, "--until") == 0) {
            arguments->until = value;
        } else if (strcmp(option, "--max-time") == 0) {
            if (!parse_u64(value, &arguments->max_time)) {
                semu_error_set(error, SEMU_ERR_ARGUMENT,
                               "invalid --max-time value %s", value);
                return 0;
            }
        } else if (strcmp(option, "--max-instructions") == 0) {
            if (!parse_u64(value, &arguments->max_instructions) ||
                arguments->max_instructions == 0u) {
                semu_error_set(error, SEMU_ERR_ARGUMENT,
                               "invalid --max-instructions value %s", value);
                return 0;
            }
        } else if (strcmp(option, "--layer") == 0) {
            if (arguments->layer_count >= SEMU_MAX_LAYERS) {
                semu_error_set(error, SEMU_ERR_RANGE, "too many --layer options");
                return 0;
            }
            arguments->layers[arguments->layer_count++] = value;
        } else {
            int rc = semu_cli_debug_parse_option(&arguments->debug,
                option, value, error);
            if (rc < 0) {
                return 0;
            }
            if (rc == 0) {
                semu_error_set(error, SEMU_ERR_ARGUMENT,
                    "unknown option %s", option);
                return 0;
            }
        }
    }
    if (!semu_cli_debug_validate(&arguments->debug, error)) {
        return 0;
    }
    return 1;
}

static int command_list(void)
{
    puts("sapporo-2.22.60  Sapporo  2.22.60.3383-P  interpreter-bring-up");
    puts("sapporo-2.33.16  Sapporo  2.33.16.17428-P  evidence-contract");
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
    if (arguments->full_flash != NULL &&
        semu_sapporo_flash_validate_image(arguments->full_flash, &error) !=
            SEMU_OK) {
        fprintf(stderr, "validate: %s\n", error.text);
        return 2;
    }
    printf("valid profile=%s product=%s version=%s components=%lu\n",
           profile.id, firmware.product, firmware.version,
           (unsigned long)firmware.component_count);
    return 0;
}

static int is_named_checkpoint(const char *until)
{
    if (until == NULL) {
        return 0;
    }
    return strcmp(until, "wfi") == 0 ||
           strcmp(until, "startup-complete") == 0 ||
           strcmp(until, "normal-frame") == 0 ||
           strcmp(until, "middle-language") == 0 ||
           strcmp(until, "lower-transition") == 0;
}

static int command_run(const run_arguments *arguments,
                       semu_frame_callback frame_callback, void *frame_context,
                       semu_machine_input_poll_fn input_poll,
                       void *input_poll_context)
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
    semu_nema_backend *backend = NULL;
    semu_input_replay *replay = NULL;
    semu_snapshot *snapshot = NULL;
    first_frame_gate frame_gate;
    replay_input_context replay_context;
    uint64_t instr_limit = arguments->max_instructions != 0u
                               ? arguments->max_instructions
                               : CHECKPOINT_INSTRUCTION_LIMIT;
    uint64_t time_limit = CHECKPOINT_TIME_LIMIT;
    semu_error_clear(&error);
    memset(&frame_gate, 0, sizeof(frame_gate));
    memset(&replay_context, 0, sizeof(replay_context));
    frame_gate.callback = frame_callback;
    frame_gate.callback_context = frame_context;
    if (arguments->until != NULL && !is_named_checkpoint(arguments->until)) {
        fprintf(stderr, "run: --until accepts only wfi, startup-complete, "
                        "normal-frame, middle-language, lower-transition\n");
        return 2;
    }
    if (arguments->max_time > 0u) {
        time_limit = arguments->max_time;
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
    backend = semu_nema_backend_create(&error);
    if (backend == NULL) {
        fprintf(stderr, "run: %s\n", error.text);
        cleanup_run(NULL, NULL, NULL, NULL, trace);
        return 2;
    }
    if (arguments->input_replay != NULL) {
        FILE *rf = fopen(arguments->input_replay, "r");
        if (rf == NULL) {
            fprintf(stderr, "run: cannot open replay %s\n",
                    arguments->input_replay);
            cleanup_run(NULL, NULL, NULL, backend, trace);
            return 2;
        }
        {
            char buf[8192];
            size_t n = fread(buf, 1u, sizeof(buf), rf);
            fclose(rf);
            replay = semu_input_replay_create(&error);
            if (replay == NULL ||
                semu_input_replay_parse(replay, buf, n, &error) != SEMU_OK) {
                fprintf(stderr, "run: replay: %s\n", error.text);
                cleanup_run(NULL, NULL, replay, backend, trace);
                return 2;
            }
        }
    }
    memset(&options, 0, sizeof(options));
    options.profile = &profile;
    options.firmware = &firmware;
    options.layers = arguments->layers;
    options.layer_count = arguments->layer_count;
    options.logger = &logger;
    options.frame_callback = frame_callback;
    options.frame_context = frame_context;
    if (arguments->until != NULL &&
        strcmp(arguments->until, "normal-frame") == 0) {
        options.frame_callback = first_frame_gate_publish;
        options.frame_context = &frame_gate;
    } else if (arguments->until != NULL &&
               (strcmp(arguments->until, "middle-language") == 0 ||
                strcmp(arguments->until, "lower-transition") == 0)) {
        /*
         * A live frontend supplies input_poll and can keep the guest alive
         * while it presents the requested UI. Headless runs still require
         * replay so this checkpoint remains deterministic there.
         */
        if (replay == NULL && input_poll == NULL) {
            fprintf(stderr, "run: --until %s requires --input-replay\n",
                    arguments->until);
            cleanup_run(NULL, NULL, replay, backend, trace);
            return 2;
        }
        if (replay != NULL) {
            frame_gate.wait_for_input = 1;
            frame_gate.required_button =
                strcmp(arguments->until, "middle-language") == 0
                    ? SEMU_BUTTON_MIDDLE : SEMU_BUTTON_LOWER;
            options.frame_callback = first_frame_gate_publish;
            options.frame_context = &frame_gate;
        }
    }
    options.display_backend_submit = semu_nema_backend_submit;
    options.display_backend_context = backend;
    options.external_flash_path = arguments->full_flash;
    options.input_poll = input_poll;
    options.input_poll_context = input_poll_context;
    machine = semu_machine_create(&options, &error);
    if (machine == NULL) {
        fprintf(stderr, "run: %s\n", error.text);
        cleanup_run(NULL, NULL, replay, backend, trace);
        return 2;
    }
    if (arguments->debug.snapshot_load_path != NULL) {
        snapshot = semu_snapshot_create(&error);
        if (snapshot == NULL ||
            semu_cli_snapshot_load_file(arguments->debug.snapshot_load_path,
                                        snapshot, &error) != SEMU_OK ||
            semu_machine_snapshot_load(machine, snapshot, &error) != SEMU_OK) {
            fprintf(stderr, "run: snapshot load: %s\n", error.text);
            cleanup_run(machine, snapshot, replay, backend, trace);
            return 2;
        }
        if (arguments->max_instructions == 0u) {
            uint64_t base = semu_machine_instructions(machine);
            if (UINT64_MAX - base < CHECKPOINT_INSTRUCTION_LIMIT) {
                fprintf(stderr, "run: snapshot instruction budget overflows\n");
                cleanup_run(machine, snapshot, replay, backend, trace);
                return 2;
            }
            instr_limit = base + CHECKPOINT_INSTRUCTION_LIMIT;
        }
        if (arguments->max_time == 0u) {
            uint64_t base = semu_machine_virtual_time(machine);
            if (UINT64_MAX - base < CHECKPOINT_TIME_LIMIT) {
                fprintf(stderr, "run: snapshot time budget overflows\n");
                cleanup_run(machine, snapshot, replay, backend, trace);
                return 2;
            }
            time_limit = base + CHECKPOINT_TIME_LIMIT;
        }
    }
    {
        uint64_t executed;
        replay_context.machine = machine;
        replay_context.frame_gate = &frame_gate;
        for (;;) {
            uint64_t now = semu_machine_virtual_time(machine);
            executed = semu_machine_instructions(machine);
            if (executed >= instr_limit || now >= time_limit) {
                reason = SEMU_STOP_BUDGET;
                break;
            }
            if (replay != NULL) {
                int refused = 0;
                semu_input_replay_pump(replay, now, replay_sink,
                                       &replay_context, &refused);
                if (refused) {
                    reason = SEMU_STOP_DEVICE_REFUSED;
                    break;
                }
            }
            limits.max_instructions = instr_limit - executed;
            if (limits.max_instructions > RUN_CHUNK_INSTRUCTIONS) {
                limits.max_instructions = RUN_CHUNK_INSTRUCTIONS;
            }
            limits.max_virtual_time_ns = time_limit - now;
            reason = semu_machine_run(machine, &limits, &error);
            if (frame_gate.reached &&
                (reason == SEMU_STOP_BUDGET || reason == SEMU_STOP_HALT ||
                 reason == SEMU_STOP_WFI_DEADLOCK ||
                 reason == SEMU_STOP_USER)) {
                reason = SEMU_STOP_USER;
                break;
            }
            if (reason != SEMU_STOP_BUDGET) break;
        }
    }
    if (arguments->debug.snapshot_save_path != NULL) {
        semu_status snapshot_status;
        semu_error_clear(&error);
        if (snapshot == NULL) {
            snapshot = semu_snapshot_create(&error);
        }
        snapshot_status = snapshot == NULL ? error.code :
            semu_machine_snapshot_save(machine, snapshot, &error);
        if (snapshot_status == SEMU_OK) {
            snapshot_status = semu_cli_snapshot_save_file(
                arguments->debug.snapshot_save_path, snapshot, &error);
        }
        if (snapshot_status != SEMU_OK) {
            fprintf(stderr, "run: snapshot save: %s\n", error.text);
            cleanup_run(machine, snapshot, replay, backend, trace);
            return 2;
        }
    }
    printf("stop=%s pc=0x%08x instructions=%llu virtual_time_ns=%llu",
           semu_stop_reason_name(reason),
           semu_machine_program_counter(machine),
           (unsigned long long)semu_machine_instructions(machine),
           (unsigned long long)semu_machine_virtual_time(machine));
    if (error.code != SEMU_OK) {
        printf(" detail=%s", error.text);
    }
    putchar('\n');
    cleanup_run(machine, snapshot, replay, backend, trace);
    return reason == SEMU_STOP_WFI_DEADLOCK || reason == SEMU_STOP_HALT ||
                   reason == SEMU_STOP_USER ? 0 : 3;
}

#include "cli_main.c"
