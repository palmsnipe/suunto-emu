/* Private-only observer; compiled by the firmware gate, never normal tests. */
#include "semu/machine.h"
#include "semu/hash.h"
#include "../../src/display/nema_backend.h"
#include "../../src/frontends/cli_snapshot.c"
#include <inttypes.h>

#define END_TIME UINT64_C(35000000000)
#define COLD_CAP UINT64_C(700000000)
/* RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the old cold prefix cap 1376525552
 * (76a7af53…), the personal mid 2953666398 (68ab2fa1…), the time mid
 * 3885613020 (b85eed95…) and the 5000000000 instruction ceiling are
 * unreachable caps and are retired — with the storage wall gone the clean
 * boot saves settings/personal inside the cold prefix and the first GPS-awake
 * refusal stops the flow at 1296811148 / virtual time 32775096969, two
 * instructions after the save closes (the close completes at 1296811147 /
 * 32775096968). A budget stop at 1296811147 resumes into the refusal itself,
 * so the refusal state 1296811148 is the only durable mid-state above the
 * cold prefix. The full terminal 3960123530 / 32455738919 (frames 238-1010,
 * crc a8c9f3d3, sha 38207035…) and idle terminal 3152721353 / 32538694863
 * (frames 6-778, crc 568bdc7d, sha d2c4833a…) are unreachable: above the
 * refusal nothing runs that the flow does not reach on its own. */
#define SAVE_MID UINT64_C(1296811147)
#define SAVE_MID_TIME UINT64_C(32775096968)
#define REFUSAL UINT64_C(1296811148)
#define REFUSAL_TIME UINT64_C(32775096969)
#define REFUSAL_DETAIL "2.39 GPS awake lifecycle or hit budget refused"

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
        ++o->frames > 20000u) {
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
    semu_profile p; semu_firmware_manifest fw; semu_machine_options opts = {0};
    semu_error e; semu_logger logger; observer obs = {0};
    semu_machine *m = NULL; semu_nema_backend *backend = NULL;
    semu_snapshot *s = NULL;
    semu_stop_reason reason = SEMU_STOP_BUDGET;
    int result = 2;
    uint64_t start_count = 0;
    /* RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the sixteen middle-button edge
     * counts {847389018, …, 3640223889} and virtual-time stamps
     * {12010884553, …, 30116594734} (E-SAP-COMPAT-PERSONAL-239-001 and the
     * preceding general-save edges) are retired as anchors: the clean boot
     * performs the whole personal save inside the cold prefix, so the
     * resumed flow reaches no injected-input phase — its first observable
     * after the prefix is the awake refusal itself. mid_budget is the first
     * old-anchor stamp above the refusal; the refusal binds long before it.
     * The `full` and `idle` branches run identically now: both terminate at
     * the same refusal, which arrives before either branch's own
     * injected-input phase diverges. */
    const uint64_t mid_budget = UINT64_C(12010884553);
    int cold, idle;
    semu_error_clear(&e);
    if (argc != 6 || (strcmp(argv[5], "full") && strcmp(argv[5], "idle"))) {
        fprintf(stderr, "usage: probe manifest flash cold|snapshot output-prefix full|idle\n");
        return 2;
    }
    cold = strcmp(argv[3], "cold") == 0; idle = strcmp(argv[5], "idle") == 0;
    (void) idle;
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
    if (cold) {
        semu_run_limits limits = {COLD_CAP, END_TIME};
        reason = semu_machine_run(m, &limits, &e);
        if (reason != SEMU_STOP_BUDGET || semu_machine_instructions(m) != COLD_CAP ||
            !save(m, argv[4], "prefix", &e)) goto done;
        result = 0; goto done;
    }
    s = semu_snapshot_create(&e);
    if (!s || semu_cli_snapshot_load_file(argv[3], s, &e) != SEMU_OK ||
        semu_machine_snapshot_load(m, s, &e) != SEMU_OK) goto done;
    start_count = semu_machine_instructions(m);
    if (start_count != COLD_CAP && start_count != REFUSAL) {
        semu_error_set(&e, SEMU_ERR_STATE, "unexpected native start checkpoint"); goto done;
    }
    if (!file_hash(argv[3], start_count == COLD_CAP ?
        "c114c00f65c0eb061ae1af548699854e22199fff5a7b36b7a89c190daade16ff" :
        "630f6062838533b021389eec1005eed17a297a2529ca250104cdbef73db5a1d9", &e)) goto done;
    if (start_count == REFUSAL) {
        /* The refusal state carries the refusal itself at its checkpoint: a
         * single step re-fires it. The refusal is a fixed point — it refuses
         * again at the identical stop without advancing instructions or
         * virtual time. */
        semu_run_limits step = {1u, 1u};
        reason = semu_machine_run(m, &step, &e);
    } else {
        semu_run_limits limits = {mid_budget - start_count,
            END_TIME - semu_machine_virtual_time(m)};
        reason = semu_machine_run(m, &limits, &e);
    }
    printf("END reason=%u instructions=%" PRIu64 " time=%" PRIu64
        " pc=%08x frames=%" PRIu64 " crc=%08x sha=%s detail=%s\n",
        (unsigned)reason, semu_machine_instructions(m), semu_machine_virtual_time(m),
        semu_machine_program_counter(m), obs.frames, obs.crc, obs.hash, e.text);
    /* The refused terminal — identical from every legitimate start state and
     * identical for the full and idle branches, twice byte-identical:
     * instr 1296811148, vt 32775096969, pc 0x1291cc, refusal detail verbatim.
     * Frame law: the frame callback fires inside semu_machine_run, so the
     * 700M cold prefix legitimately accumulates its cold frames; the prefix-
     * resumed terminal of THIS flow shows frames=4 (the load publishes one,
     * the three wake-cycle frames the guest queues before the refusal bind,
     * twice-verified). Loading a snapshot publishes exactly one frame and a
     * refusal publishes none, so the refusal-start terminal shows frames=1.
     * (The 677-census terminal belongs to the edge-driven general-budget
     * flow, pinned there.) */
    if (reason != SEMU_STOP_COMPAT_REFUSED ||
        semu_machine_instructions(m) != REFUSAL ||
        semu_machine_virtual_time(m) != REFUSAL_TIME ||
        semu_machine_program_counter(m) != 0x1291ccu ||
        strcmp(e.text, REFUSAL_DETAIL) ||
        obs.crc != 0x3bd12ac8u ||
        strcmp(obs.hash, "07944160817f67bb02efd0fdcebe6e0e38353d1950adb112f34a912d1a40396f") ||
        obs.frames != (start_count == REFUSAL ? 1u : 4u) ||
        !save(m, argv[4], "final", &e) ||
        (start_count == COLD_CAP && !save(m, argv[4], "mid", &e))) goto done;
    {
        /* The refusal re-fires on one more step at the identical stop and
         * re-saves the identical image: stable refusal, not a one-shot. */
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
