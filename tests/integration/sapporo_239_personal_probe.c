/* Private-only observer; compiled by the firmware gate, never normal tests. */
#include "semu/machine.h"
#include "semu/hash.h"
#include "../../src/display/nema_backend.h"
#include "../../src/frontends/cli_snapshot.c"
#include <inttypes.h>

#define END_TIME UINT64_C(35000000000)
#define PREFIX UINT64_C(1376525552)
#define MID UINT64_C(2953666398)
#define END_COUNT UINT64_C(5000000000)

typedef struct observer {
    uint64_t frames;
    uint32_t crc;
    char hash[65];
} observer;

static void frame(void *context, const semu_frame *f)
{
    observer *o = context;
    uint8_t digest[32];
    if (f->format != SEMU_PIXEL_RGB565_LE || f->width != 240u ||
        f->height != 240u || f->stride != 480u || f->size < 115200u ||
        ++o->frames > 5000u) {
        fprintf(stderr, "invalid private renderer frame\n"); exit(2);
    }
    o->crc = semu_crc32(0u, f->pixels, 115200u);
    semu_sha256(f->pixels, 115200u, digest);
    semu_sha256_format(digest, o->hash);
}

static int save(semu_machine *m, const char *prefix, const char *suffix,
                 semu_error *e)
{
    char path[1024];
    int ok = 0, n = snprintf(path, sizeof(path), "%s.%s.sems", prefix, suffix);
    semu_snapshot *s = semu_snapshot_create(e);
    if (n >= 0 && (size_t)n < sizeof(path) && s &&
        semu_machine_snapshot_save(m, s, e) == SEMU_OK &&
        semu_cli_snapshot_save_file(path, s, e) == SEMU_OK) ok = 1;
    semu_snapshot_destroy(s);
    return ok;
}

static int file_hash(const char *path, const char *wanted, semu_error *e)
{
    uint8_t digest[32]; char hash[65]; uint64_t size;
    if (semu_sha256_file(path, digest, &size, e) != SEMU_OK) return 0;
    semu_sha256_format(digest, hash);
    if (strcmp(hash, wanted) == 0) return 1;
    semu_error_set(e, SEMU_ERR_CONFLICT, "private input hash mismatch");
    return 0;
}


int main(int argc, char **argv)
{
    static const char *const layers[] = {
        "sapporo-2.39-synthetic-wbsto", "sapporo-2.39-gps-startup",
        "sapporo-2.39-gps-reopen", "sapporo-2.39-gps-awake"
    };
    /* E-SAP-COMPAT-PERSONAL-239-001 and the preceding general-save edges. */
    static const uint64_t counts[] = {
        847389018u,850221529u,1132984059u,1136723800u,
        2103476826u,2107221220u,2397641544u,2404140067u,2701906762u,2708404518u,
        3016651598u,3023149894u,3326045273u,3332543381u,3637404457u,3640223889u
    };
    static const uint64_t times[] = {
        UINT64_C(12010884553),UINT64_C(12096961148),
        UINT64_C(14075897022),UINT64_C(14161873768),
        UINT64_C(20020743409),UINT64_C(20106721328),
        UINT64_C(22084812137),UINT64_C(22170370053),
        UINT64_C(24048201779),UINT64_C(24133728411),
        UINT64_C(26010661264),UINT64_C(26096218953),
        UINT64_C(28022050108),UINT64_C(28107577092),
        UINT64_C(30036360075),UINT64_C(30116594734)
    };
    semu_profile p; semu_firmware_manifest fw; semu_machine_options opts = {0};
    semu_error e; semu_logger logger; observer obs = {0};
    semu_machine *m = NULL; semu_nema_backend *backend = NULL;
    semu_snapshot *s = NULL;
    semu_stop_reason reason = SEMU_STOP_BUDGET;
    unsigned edge = 0u, edge_end;
    int result = 2, mid_saved = 0, resumed = 0, cold, idle;
    semu_error_clear(&e);
    if (argc != 6 || (strcmp(argv[5], "full") && strcmp(argv[5], "idle"))) {
        fprintf(stderr, "usage: probe manifest flash cold|snapshot output-prefix full|idle\n");
        return 2;
    }
    cold = strcmp(argv[3], "cold") == 0; idle = strcmp(argv[5], "idle") == 0;
    edge_end = idle ? 10u : 16u;
    if (semu_profile_load("profiles/sapporo/2.39.20/profile.semu", &p, &e) != SEMU_OK ||
        semu_manifest_load(argv[1], &fw, &e) != SEMU_OK ||
        semu_manifest_validate(&p, &fw, &e) != SEMU_OK ||
        !file_hash(argv[2],
          "37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb", &e)) goto done;
    semu_log_init(&logger, stderr, SEMU_LOG_INFO);
    backend = semu_nema_backend_create(&e);
    if (!backend) goto done;
    opts.profile = &p; opts.firmware = &fw; opts.logger = &logger;
    opts.layers = layers; opts.layer_count = SEMU_ARRAY_LEN(layers);
    opts.external_flash_path = argv[2];
    opts.display_backend = &semu_nema_backend_ops; opts.display_backend_context = backend;
    opts.frame_callback = frame; opts.frame_context = &obs;
    m = semu_machine_create(&opts, &e);
    if (!m) goto done;
    if (!cold) {
        s = semu_snapshot_create(&e);
        if (!s || semu_cli_snapshot_load_file(argv[3], s, &e) != SEMU_OK ||
            semu_machine_snapshot_load(m, s, &e) != SEMU_OK) goto done;
        if (semu_machine_instructions(m) == MID) {
            edge = 10u; mid_saved = 1; resumed = 1;
        } else if (semu_machine_instructions(m) == PREFIX) edge = 4u;
        else {
            semu_error_set(&e, SEMU_ERR_STATE, "unexpected native start checkpoint"); goto done;
        }
        if (!file_hash(argv[3], resumed ?
            "68ab2fa1fda8596e1b51a6b3d83950363effb0598f1835506492d20636fad7d8" :
            "76a7af5385eb2ddf5dfe94f6607db34e6e820edb06f5054a31bce7a5ef4ada66", &e)) goto done;
    }
    while (semu_machine_instructions(m) < END_COUNT &&
        semu_machine_virtual_time(m) < END_TIME) {
        uint64_t count = semu_machine_instructions(m);
        uint64_t now = semu_machine_virtual_time(m);
        uint64_t target = edge < edge_end ? counts[edge] : END_COUNT;
        if (cold && target > PREFIX) target = PREFIX;
        if (!cold && !mid_saved && target > MID) target = MID;
        if (cold && count == PREFIX) {
            if (!save(m, argv[4], "prefix", &e)) goto done;
            result = 0; goto done;
        }
        if (!cold && !mid_saved && count == MID) {
            if (!save(m, argv[4], "mid", &e)) goto done;
            mid_saved = 1; continue;
        }
        if (edge < edge_end && count == counts[edge]) {
            semu_input_event event = {SEMU_INPUT_BUTTON, SEMU_BUTTON_MIDDLE,
                                      (int32_t)(edge % 2u), 0, 0};
            if (now != times[edge] || semu_machine_input(m, &event, &e) != SEMU_OK) goto done;
            printf("INPUT instructions=%" PRIu64 " time=%" PRIu64 " value=%u\n", count, now, edge % 2u);
            ++edge; continue;
        }
        if (target <= count) goto done;
        semu_run_limits limits = {target - count, END_TIME - now};
        reason = semu_machine_run(m, &limits, &e);
        if (reason != SEMU_STOP_BUDGET) break;
    }
    printf("END reason=%u instructions=%" PRIu64 " time=%" PRIu64
        " pc=%08x frames=%" PRIu64 " crc=%08x sha=%s error=%s\n",
        (unsigned)reason, semu_machine_instructions(m), semu_machine_virtual_time(m),
        semu_machine_program_counter(m), obs.frames, obs.crc, obs.hash, e.text);
    if (reason != SEMU_STOP_COMPAT_REFUSED || edge != edge_end ||
        semu_machine_instructions(m) != (idle ? UINT64_C(3152721353) : UINT64_C(3885178598)) ||
        semu_machine_virtual_time(m) != (idle ? UINT64_C(32538694863) : UINT64_C(30368914377)) ||
        semu_machine_program_counter(m) != (idle ? 0x1291ccu : 0x920b4u) ||
        obs.crc != (idle ? 0x568bdc7du : 0xa8c9f3d3u) ||
        strcmp(obs.hash, idle ?
            "d2c4833a433610b5087f6e04fe16c7c4bd9d3baf6573df21cc72e0abde77b09b" :
            "3820703556359211f629aea5ef013dda45fba092229b8d61e5a10d684c42d585") ||
        obs.frames != (idle ? (resumed ? 6u : 778u) : (resumed ? 232u : 1004u)) ||
        strcmp(e.text, idle ? "2.39 GPS awake lifecycle or hit budget refused" :
            "unknown Sapporo 2.39 writable file path") ||
        !save(m, argv[4], "final", &e)) goto done;
    {
        semu_run_limits retry = {1u, 1u};
        uint64_t count = semu_machine_instructions(m), now = semu_machine_virtual_time(m);
        if (semu_machine_run(m, &retry, &e) != SEMU_STOP_COMPAT_REFUSED ||
            semu_machine_instructions(m) != count || semu_machine_virtual_time(m) != now ||
            !save(m, argv[4], "refused", &e)) goto done;
    }
    result = 0;
done:
    if (result) fprintf(stderr, "private personal-settings gate failed: %s\n", e.text);
    semu_snapshot_destroy(s); semu_machine_destroy(m); semu_nema_backend_destroy(backend);
    return result;
}
