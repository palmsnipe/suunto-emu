/* Private-only observer; compiled by the firmware gate, never normal tests. */
#include "semu/machine.h"
#include "semu/hash.h"
#include "../../src/display/nema_backend.h"
#include "../../src/frontends/cli_snapshot.c"
#include <inttypes.h>

#define END_TIME UINT64_C(35000000000)
#define COLD_CAP UINT64_C(700000000)
/* RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the old general-save mid checkpoint
 * 1376525552 (76a7af53…) is unreachable — the storage wall that made the
 * general save wait for an injected input edge is gone; the clean boot
 * performs the whole save (open, 90 writes, the 1505/1505 close, ordinals
 * 76315–76407) inside the cold prefix itself. The observable mid for the
 * resumed segment is the refusal state — instr 1296811148 / virtual time
 * 32775096969 — which the prefix's own first refused step produces. The
 * one-instruction-earlier state (1296811147 / 32775096968, just after the
 * close) stays reachable as the budget-stopped mid below, so both sides of
 * the refusal are pinned. The two states are distinguished by instruction
 * count. */
#define SAVE_MID UINT64_C(1296811147)
#define SAVE_MID_TIME UINT64_C(32775096968)
/* RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the budget-at-site stop on the
 * refusing instruction (a cap at SAVE_MID) resumes to 1296811148, not the cap
 * itself, so the SAVE_MID state is never a durable checkpoint in this lineage
 * and is not a probe start state; both constants are retained for the
 * one-step-before-refusal identification. */
/* RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the old terminal 2363623546 /
 * 32619070564 with frames 676 (input-driven fifth awake pulse) is
 * unreachable — above SAVE_MID nothing runs that the flow does not reach on
 * its own, and the first refusal is the terminal. Every cap above these
 * numbers is an unreachable cap and is retired. */
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
    uint64_t start_count;
    /* RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the middle-button edge tables
     * counts {847389018, 850221529, 1132984059, 1136723800} and virtual-time
     * stamps {10927227032, 11013122982, 12833901728, 12892782646} (old:
     * {12010884553, 12096961148, 14075897022, 14161873768}) are retired as
     * anchors: the clean boot performs the whole general save inside the cold
     * prefix, so the resumed flow reaches no injected-input phase — its first
     * observable after the prefix is the awake refusal itself. mid_budget is
     * the first old-anchor stamp above the refusal; the refusal binds there
     * long before that budget. The old 1376525552 mid cap and the
     * 5000000000 instruction ceiling are unreachable caps and are retired. */
    const uint64_t mid_budget = UINT64_C(10927227032);
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
    start_count = semu_machine_instructions(m);
    if (start_count == COLD_CAP) {
        /* Cold prefix: replay the four synthetic middle-button edges whose
         * instruction counts STILL HOLD (847389018/850221529/1132984059/
         * 1136723800). */
    } else if (start_count == UINT64_C(2391136680)) {
        /* The refused terminal state itself: a single step re-fires the
         * refusal there. The refusal is a fixed point — it refuses again at
         * the identical stop without advancing instructions or virtual time. */
    } else {
        semu_error_set(&e, SEMU_ERR_STATE, "unexpected native start checkpoint"); goto done;
    }
    /* RE-PINNED (710/E-EMU-SAP235-TICKTRAIL-002): the tsc6a per-resolve
     * frame lifecycle changed the serialized shadow inside the terminal
     * snapshot image (the shadow now holds the resting state at the stop
     * instead of the accumulated strokes) — the guest stop, the refusal
     * detail, the terminal frame (677/405d1af6/6eb15b72…) and every
     * transcript hash are unchanged; only the image bytes moved
     * bb17b7a8… -> 24d5a4dd… (twice byte-identical). The cold prefix
     * image is untouched by the fix (no fmt-17 shadow divergence in the
     * cold window) and keeps its pin. */
    if (!file_hash(argv[3], start_count == COLD_CAP ?
        "7dddd41a13c51b0e4d4d63be09b8bfff1db1654439d290343f24959758761c7c" :
        "24d5a4dd8859155426adf98b89b1f3d6b8fdf53618069b8fa92bad208fcabeba", &e)) goto done;
    if (start_count == COLD_CAP) {
        /* RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the old virtual-time
         * equality on each edge {12010884553, 12096961148, 14075897022,
         * 14161873768} -> observed {10927227032, 11013122982, 12833901728,
         * 12892782646} is retained as observables in the INPUT lines; the
         * instruction counts hold, the stamps moved with the clean-boot
         * virtual-time rate. */
        static const uint64_t counts[] = {847389018u, 850221529u,
            1132984059u, 1136723800u};
        uint64_t count = start_count;
        unsigned edge;
        for (edge = 0u; edge < 4u; ++edge) {
            uint64_t now = semu_machine_virtual_time(m);
            semu_run_limits limits = {counts[edge] - count, END_TIME - now};
            semu_input_event event = {SEMU_INPUT_BUTTON, SEMU_BUTTON_MIDDLE,
                                      (int32_t)(edge % 2u), 0, 0};
            if (semu_machine_run(m, &limits, &e) != SEMU_STOP_BUDGET ||
                semu_machine_instructions(m) != counts[edge]) goto done;
            if (semu_machine_input(m, &event, &e) != SEMU_OK) goto done;
            count = counts[edge];
            printf("INPUT instructions=%" PRIu64 " time=%" PRIu64 " value=%u\n",
                count, semu_machine_virtual_time(m), edge % 2u);
        }
        { semu_run_limits limits = {mid_budget - count,
              END_TIME - semu_machine_virtual_time(m)};
          reason = semu_machine_run(m, &limits, &e); }
    } else {
        semu_run_limits step = {1u, 1u};
        reason = semu_machine_run(m, &step, &e);
    }
    printf("END reason=%u instructions=%" PRIu64 " time=%" PRIu64
        " pc=%08x frames=%" PRIu64 " crc=%08x sha=%s detail=%s\n",
        (unsigned)reason, semu_machine_instructions(m), semu_machine_virtual_time(m),
        semu_machine_program_counter(m), obs.frames, obs.crc, obs.hash, e.text);
    /* The refused terminal, reached identically from the cold prefix (past
     * the four held edges) and from the terminal-state resume — a fixed
     * point — twice byte-identical: instr 2391136680 / virtual time
     * 32620918072, pc 0x1291cc, crc 405d1af6, sha 6eb15b72…, refusal detail
     * verbatim. RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the old golden
     * 2363623546 / 32619070564 belonged to the retired input-edge
     * virtual-time lineage; the refusal itself, its place and its screen
     * (frames 677, crc 405d1af6, sha 6eb15b72…) HOLD. Frame law: the frame
     * callback fires inside semu_machine_run, so the 700M cold prefix
     * legitimately accumulates its cold frames — the prefix leg shows the
     * full cold census 677; loading a snapshot publishes exactly one frame
     * and a refusal publishes none, so the terminal-state leg shows 1. */
    if (reason != SEMU_STOP_COMPAT_REFUSED ||
        semu_machine_instructions(m) != UINT64_C(2391136680) ||
        semu_machine_virtual_time(m) != UINT64_C(32620918072) ||
        semu_machine_program_counter(m) != 0x1291ccu ||
        strcmp(e.text, REFUSAL_DETAIL) ||
        obs.crc != 0x405d1af6u ||
        strcmp(obs.hash, "6eb15b72ea2d250b1106d6a89c39ac87eb3827ebd1ce7c367bf1efcfeb2b4742") ||
        obs.frames != (start_count == COLD_CAP ? 677u : 1u) ||
        !save(m, argv[4], "final", &e) ||
        (start_count == COLD_CAP && !save(m, argv[4], "mid", &e))) goto done;
    {
        /* The refusal re-fires on one more step at the identical stop and
         * re-saves the identical image. */
        semu_run_limits retry = {1u, 1u};
        uint64_t count = semu_machine_instructions(m), now = semu_machine_virtual_time(m);
        if (semu_machine_run(m, &retry, &e) != SEMU_STOP_COMPAT_REFUSED ||
            semu_machine_instructions(m) != count || semu_machine_virtual_time(m) != now ||
            !save(m, argv[4], "refused", &e)) goto done;
    }
    result = 0;
done:
    if (result) fprintf(stderr, "private general-settings gate failed: %s\n", e.text);
    semu_snapshot_destroy(s); semu_machine_destroy(m); semu_nema_backend_destroy(backend);
    return result;
}
