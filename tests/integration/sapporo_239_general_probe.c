/* Private-only observer; compiled by the firmware gate, never normal tests. */
#include "semu/machine.h"
#include "semu/hash.h"
#include "../../src/display/nema_backend.h"
#include "../../src/frontends/cli_snapshot.c"
#include <inttypes.h>

#define END_TIME UINT64_C(35000000000)
#define MID UINT64_C(1376525552)

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
    /* Actual edges, including the evidenced WFI deadline overshoots. */
    static const uint64_t counts[] = {847389018u,850221529u,1132984059u,1136723800u};
    static const uint64_t times[] = {
        UINT64_C(12010884553), UINT64_C(12096961148),
        UINT64_C(14075897022), UINT64_C(14161873768)
    };
    semu_profile p; semu_firmware_manifest fw; semu_machine_options opts = {0};
    semu_error e; semu_logger logger; observer obs = {0};
    semu_machine *m = NULL; semu_nema_backend *backend = NULL;
    semu_snapshot *s = NULL;
    semu_stop_reason reason = SEMU_STOP_BUDGET;
    unsigned edge = 0u; int result = 2, mid_saved = 0, resumed = 0;
    semu_error_clear(&e);
    if (argc != 5) { fprintf(stderr, "usage: probe manifest flash cold|snapshot output-prefix\n"); return 2; }
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
    opts.display_snapshot = &semu_nema_backend_snapshot_ops;
    opts.frame_callback = frame; opts.frame_context = &obs;
    m = semu_machine_create(&opts, &e);
    if (!m) goto done;
    if (strcmp(argv[3], "cold") == 0) {
        semu_run_limits limits = {700000000u, END_TIME};
        reason = semu_machine_run(m, &limits, &e);
        if (reason != SEMU_STOP_BUDGET || semu_machine_instructions(m) != 700000000u ||
            !save(m, argv[4], "prefix", &e)) goto done;
        result = 0; goto done;
    }
    s = semu_snapshot_create(&e);
    if (!s || semu_cli_snapshot_load_file(argv[3], s, &e) != SEMU_OK ||
        semu_machine_snapshot_load(m, s, &e) != SEMU_OK) goto done;
    if (semu_machine_instructions(m) == MID) {
        edge = 4u; mid_saved = 1; resumed = 1;
    } else if (semu_machine_instructions(m) != 700000000u) {
        semu_error_set(&e, SEMU_ERR_STATE, "unexpected native start checkpoint"); goto done;
    }
    if (!file_hash(argv[3], resumed ?
        "76a7af5385eb2ddf5dfe94f6607db34e6e820edb06f5054a31bce7a5ef4ada66" :
        "7650d82fe72e58d544dc0043df99ab756ece41092d39e94d7c0b2b7460a904d2", &e)) goto done;
    while (semu_machine_instructions(m) < UINT64_C(5000000000) &&
        semu_machine_virtual_time(m) < END_TIME) {
        uint64_t count = semu_machine_instructions(m);
        uint64_t now = semu_machine_virtual_time(m);
        uint64_t target = edge < 4u ? counts[edge] : !mid_saved ? MID : UINT64_C(5000000000);
        if (edge < 4u && count == target) {
            semu_input_event event = {SEMU_INPUT_BUTTON, SEMU_BUTTON_MIDDLE,
                                      (int32_t)(edge % 2u), 0, 0};
            if (now != times[edge] || semu_machine_input(m, &event, &e) != SEMU_OK) goto done;
            printf("INPUT instructions=%" PRIu64 " time=%" PRIu64 " value=%u\n", count, now, edge % 2u);
            ++edge; continue;
        }
        if (!mid_saved && count == MID) {
            if (!save(m, argv[4], "mid", &e)) goto done;
            mid_saved = 1; continue;
        }
        if (target <= count) goto done;
        semu_run_limits limits = {target - count, END_TIME - now};
        reason = semu_machine_run(m, &limits, &e);
        if (reason != SEMU_STOP_BUDGET) break;
    }
    printf("END reason=%u instructions=%" PRIu64 " time=%" PRIu64
        " pc=%08x frames=%" PRIu64 " crc=%08x sha=%s\n",
        (unsigned)reason, semu_machine_instructions(m), semu_machine_virtual_time(m),
        semu_machine_program_counter(m), obs.frames, obs.crc, obs.hash);
    if (reason != SEMU_STOP_COMPAT_REFUSED ||
        semu_machine_instructions(m) != UINT64_C(2363623546) ||
        semu_machine_virtual_time(m) != UINT64_C(32619070564) ||
        semu_machine_program_counter(m) != 0x1291ccu || obs.crc != 0x405d1af6u ||
        strcmp(obs.hash, "6eb15b72ea2d250b1106d6a89c39ac87eb3827ebd1ce7c367bf1efcfeb2b4742") ||
        obs.frames != (resumed ? 528u : 676u) ||
        !save(m, argv[4], "final", &e)) goto done;
    result = 0;
done:
    if (result) fprintf(stderr, "private general-settings gate failed: %s\n", e.text);
    semu_snapshot_destroy(s); semu_machine_destroy(m); semu_nema_backend_destroy(backend);
    return result;
}
