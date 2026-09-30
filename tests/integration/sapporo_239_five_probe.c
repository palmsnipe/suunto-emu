/* Private firmware gate; no diagnostic adapter or guest-state mutation. */
#define main personal_gate_main
#include "sapporo_239_personal_probe.c"
#undef main
#include "../../src/boards/machine_internal.h"
#include "../../src/compat/sapporo_239_gps_awake.h"

/* RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the old cold prefix cap
 * BASE=3960123530 (prefix 6e670940…, pc 0x1291cc / time 32455738919) is an
 * unreachable cap — the clean awake-five flow refuses at instruction
 * 1409719577, long before 3.96G, so no cap at or above the old BASE runs.
 * The re-scoped cold prefix save point is the 700000000-instruction budget
 * stop of the same clean-boot lineage the general/personal gates re-pinned
 * (pc 0x000a81b0, time 4823758494, 3 frames, observer state pinned below;
 * probe-internal snapshot 27625f12…, measured twice). Mid-boot caps in this
 * lineage are frame-sensitive (the display publishes a frame inside the
 * final stepped instruction, so a cap's serialized frame content depends on
 * how the run chunks to the cap); the prefix save below is taken at an
 * EXACT count+time arrival inside one probe run, which is the reproducible
 * anchor — verified twice in the era script. */
#define BASE UINT64_C(700000000)
#define BASE_TIME UINT64_C(4823758494)
#define BASE_PC UINT32_C(0x000a81b0)
#define BASE_FRAMES UINT64_C(3)
#define BASE_CRC UINT32_C(0x4979f432)
#define BASE_SHA "3eff811736aa1890e78095f31d88ad95a8a457d41caa0ccb3e527555c8ecf373"

/* RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the refusal triple. The five
 * layer (maximum_hits 5, trigger gps-awake-pulse) accepts five pulses and
 * the sixth arrival at the awake hook refuses with
 * '2.39 GPS awake lifecycle or hit budget refused'. The hook runs BEFORE
 * the cpu step retires, so the refused stop keeps pc 0x001291cc at the
 * identical instruction count and virtual time (the "+1 law": a CLI chunked
 * run reaching the cap first shows the refusal line at cap+1). A resume
 * from the refusal state re-fires the refusal at the same triple with no
 * advance; the mid mode below starts exactly there.
 * Refusal-observer state (frame observer pinned per run — FRAME LAW: frame
 * callbacks fire legitimately inside semu_machine_run and accumulate per
 * run): the full cold CLI lineage reaches the refusal with 6 frames
 * (crc 3bd12ac8, sha 07944160…, pinned in the era script's log census);
 * a resumed refusal state re-fires with ZERO frames, which is what both
 * resume modes below pin. */
#define FIVE_REFUSAL UINT64_C(1409719577)
#define FIVE_REFUSAL_TIME UINT64_C(38250007180)
#define FIVE_REFUSAL_PC UINT32_C(0x001291cc)
#define FIVE_REFUSAL_DETAIL "2.39 GPS awake lifecycle or hit budget refused"

/* RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the old four-phase pending/irq/
 * high/fallen window at ~3.96G (pcs 0x128926/0x12892e, flag 0/1) is
 * unreachable — it sat behind the retired storage wall. The five layer
 * shares the awake lane's CXD5610 rise-latch law (the fall is allocated AT
 * the rising deadline, the pin-high window is 0 ns, the stage never reaches
 * 3), so no snapshot in the flow shows the pin high or fallen: every
 * twice-verified step trace shows driver mirror 0x100588a2 = 1 and GPIO24
 * = 0. The observable phases of the five-pulse lifecycle are the five
 * trigger sites themselves. A trigger site is the hook-pc instruction
 * whose retirement produces the intervention-hit log line (the hook runs
 * before the step, so the logged time is the hook-entry time):
 *   ordinal 1  hook instr 846889602  entry t=10876190851
 *   ordinal 2  hook instr 960806493  entry t=16350459381
 *   ordinal 3  hook instr 1073726377 entry t=21825199678
 *   ordinal 4  hook instr 1184805197 entry t=27300294166
 *   ordinal 5  hook instr 1296811148 entry t=32775096969
 *              (the fifth hit queues the last pulse; the sixth arrival is
 *              the lifecycle refusal)
 * Hook pc 0x001291cc at every site, twice-verified by four independent
 * paths: cold single-step to refusal, time-cap and instruction-cap ±1 step
 * traces, and the CLI intervention-hit log census. After each accepted
 * hook a stop lands at pc 0x001291ce with mirror byte 1; one instruction
 * later the pc is 0x001296f8 (the awake lane's analogous return sites).
 * The five sites are stepped through (never saved) in both modes. */
static const uint64_t five_pulses[] = {
    UINT64_C(846889602), UINT64_C(960806493), UINT64_C(1073726377),
    UINT64_C(1184805197), UINT64_C(1296811148)
};
static const uint64_t five_pulse_times[] = {
    UINT64_C(10876190851), UINT64_C(16350459381), UINT64_C(21825199678),
    UINT64_C(27300294166), UINT64_C(32775096969)
};

/* A sleeping dispatch may advance time without retiring an instruction.
 * Reach the measured time too; never act at an earlier same-count edge. */
static int five_boundary(semu_machine *m, uint64_t count, uint64_t time,
    semu_error *e)
{
    if (semu_machine_instructions(m) != count) return 0;
    while (semu_machine_virtual_time(m) < time) {
        semu_run_limits step = {1u, 1u};
        if (semu_machine_run(m, &step, e) != SEMU_STOP_BUDGET ||
            semu_machine_instructions(m) != count) return 0;
    }
    return semu_machine_virtual_time(m) == time;
}

/* Stop exactly at a pinned pulse hook entry: the pc is the awake hook and
 * the driver mirror byte at 0x100588a2 is rise-latched 1. Read-only. */
static int five_observe_hook(semu_machine *m, unsigned ordinal, semu_error *e)
{
    uint8_t flag = 0u;
    if (!five_boundary(m, five_pulses[ordinal], five_pulse_times[ordinal], e) ||
        semu_machine_program_counter(m) != SEMU_SAPPORO_239_GPS_AWAKE_PC ||
        semu_bus_copy_out(m->bus, 0x100588a2u, &flag, 1u, e) != SEMU_OK ||
        flag != 1u) return 0;
    printf("HOOK ordinal=%u count=%" PRIu64 " time=%" PRIu64 " pc=%08x flag=%u\n",
        ordinal + 1u, semu_machine_instructions(m), semu_machine_virtual_time(m),
        semu_machine_program_counter(m), flag);
    return 1;
}

static uint32_t five_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8u |
        (uint32_t)p[2] << 16u | (uint32_t)p[3] << 24u;
}

/* Comparison copy only: remove the sole authorized difference from ticket 769.
 * This copy is hashed, never loaded into a machine or used for continuation. */
static int compare_diagnostic(semu_machine *m, const char *prefix,
    const char *expected, semu_error *e)
{
    const char *name = "sapporo-2.39-gps-awake-five";
    semu_snapshot *s = semu_snapshot_create(e);
    const uint8_t *data; uint8_t *copy = NULL; size_t size, offset = 24u;
    char path[1024]; int ok = 0, n;
    if (!s || semu_machine_snapshot_save(m, s, e) != SEMU_OK ||
        semu_snapshot_read_section(s, SEMU_SNAPSHOT_SECTION_MACHINE, &data, &size) != SEMU_OK ||
        size < 24u || five_le32(data + 20u) != 4u) goto done;
    for (unsigned i = 0u; i < 4u; ++i) {
        uint32_t length, counters;
        if (offset > size || size - offset < 4u) goto done;
        length = five_le32(data + offset);
        if (length > size - offset - 4u || size - offset - 4u - length < 13u) goto done;
        if (i == 3u) {
            if (length != strlen(name) || memcmp(data + offset + 4u, name, length)) goto done;
            copy = malloc(size - 5u); if (!copy) goto done;
            memcpy(copy, data, offset);
            uint32_t shortened = length - 5u;
            for (unsigned b = 0u; b < 4u; ++b) copy[offset + b] = (uint8_t)(shortened >> (8u * b));
            memcpy(copy + offset + 4u, data + offset + 4u, shortened);
            memcpy(copy + offset + 4u + shortened, data + offset + 4u + length,
                size - offset - 4u - length);
            break;
        }
        counters = five_le32(data + offset + 4u + length + 9u);
        if (counters > (size - offset - 17u - length) / 8u) goto done;
        offset += 17u + length + (size_t)counters * 8u;
    }
    n = snprintf(path, sizeof(path), "%s.normalized.sems", prefix);
    if (n < 0 || (size_t)n >= sizeof(path) || !copy ||
        semu_snapshot_write_section(s, SEMU_SNAPSHOT_SECTION_MACHINE, copy, size - 5u, e) != SEMU_OK ||
        semu_cli_snapshot_save_file(path, s, e) != SEMU_OK || !file_hash(path, expected, e)) goto done;
    ok = 1;
done:
    free(copy); semu_snapshot_destroy(s); return ok;
}

int main(int argc, char **argv)
{
    const char *layers[] = {"sapporo-2.39-synthetic-wbsto", "sapporo-2.39-gps-startup",
        "sapporo-2.39-gps-reopen", "sapporo-2.39-gps-awake-five"};
    /* RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the eighteen middle-button
     * edge counts {847389018, …, 3995360381} and virtual times
     * {12010884553, …, 34174452535} belonged to the old engine's input
     * choreography behind the retired storage wall. The refusal is the
     * lifecycle/hit-budget refusal reached with ZERO input: nothing at or
     * above instruction 1409719577 is reachable at any cap, so the edge
     * sites are unreachable and retired. Both modes are input-free. */
    semu_profile p; semu_firmware_manifest fw; semu_machine_options o = {0};
    semu_machine *m = NULL; semu_snapshot *s = NULL; semu_nema_backend *backend = NULL;
    semu_logger logger; semu_error e; observer obs = {0};
    semu_stop_reason reason = SEMU_STOP_BUDGET;
    unsigned pulse = 0u; int cold, result = 2;
    uint64_t start_count;
    semu_error_clear(&e);
    if (argc != 7 || (strcmp(argv[6], "prefix") && strcmp(argv[6], "idle") &&
        strcmp(argv[6], "refusal"))) return 2;
    cold = strcmp(argv[3], "cold") == 0;
    /* argv[3] is "cold" exactly in prefix mode, or the refusal-state
     * snapshot path in idle/refusal mode. */
    if (cold != (strcmp(argv[6], "prefix") == 0)) return 2;
    if (semu_profile_load("profiles/sapporo/2.39.20/profile.semu", &p, &e) != SEMU_OK ||
        semu_manifest_load(argv[1], &fw, &e) != SEMU_OK ||
        semu_manifest_validate(&p, &fw, &e) != SEMU_OK ||
        !file_hash(argv[2], "37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb", &e) ||
        (!cold && !file_hash(argv[3], argv[4], &e))) goto done;
    /* RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): all modes log at INFO. The
     * idle and refusal modes resume the refusal state, where the layer
     * counter is already at the budget and the refusal re-fires BEFORE any
     * further event is scheduled or logged, so their logs carry no event
     * lines (old idle-branch law) while the cold prefix log carries the
     * full clean-boot census the era script pins. */
    semu_log_init(&logger, stderr, SEMU_LOG_INFO);
    backend = semu_nema_backend_create(&e); if (!backend) goto done;
    o.profile = &p; o.firmware = &fw; o.logger = &logger;
    o.external_flash_path = argv[2]; o.layers = layers; o.layer_count = 4u;
    o.display_backend = &semu_nema_backend_ops; o.display_backend_context = backend;
    o.display_snapshot = &semu_nema_backend_snapshot_ops;
    o.frame_callback = frame; o.frame_context = &obs;
    m = semu_machine_create(&o, &e); if (!m) goto done;
    start_count = 0u;
    if (!cold) {
        /* The only resume entry is the durable refusal state (the budget
         * stop ON the refusing instruction: pc 0x001291cc, 1409719577
         * retired). Its snapshot already IS the refusal image; the run
         * below re-fires the refusal at the same triple with no advance.
         * The idle mode exists to prove that identity: it saves the same
         * image under the prefix name and lets the terminal path re-fire. */
        s = semu_snapshot_create(&e);
        if (!s || semu_cli_snapshot_load_file(argv[3], s, &e) != SEMU_OK ||
            semu_machine_snapshot_load(m, s, &e) != SEMU_OK) goto done;
        start_count = semu_machine_instructions(m);
        if (start_count != FIVE_REFUSAL ||
            semu_machine_virtual_time(m) != FIVE_REFUSAL_TIME ||
            semu_machine_program_counter(m) != FIVE_REFUSAL_PC) goto done;
        if (!save(m, argv[5], "prefix", &e)) goto done;
        printf("PREFIX count=%" PRIu64 " time=%" PRIu64 " pc=%08x frames=%" PRIu64
            " resume=refusal-state\n", start_count, semu_machine_virtual_time(m),
            semu_machine_program_counter(m), obs.frames);
        pulse = 5u;
    }
    while (semu_machine_instructions(m) < UINT64_C(2000000000) &&
        semu_machine_virtual_time(m) < UINT64_C(45000000000)) {
        uint64_t count = semu_machine_instructions(m), now = semu_machine_virtual_time(m);
        uint64_t target = FIVE_REFUSAL;
        if (cold && count == BASE && now == BASE_TIME) {
            if (semu_machine_program_counter(m) != BASE_PC ||
                obs.frames != BASE_FRAMES || obs.crc != BASE_CRC ||
                strcmp(obs.hash, BASE_SHA) ||
                !save(m, argv[5], "prefix", &e)) goto done;
            printf("PREFIX count=%" PRIu64 " time=%" PRIu64 " pc=%08x frames=%" PRIu64
                " crc=%08x sha=%s\n", count, now, semu_machine_program_counter(m),
                obs.frames, obs.crc, obs.hash);
            result = 0; goto done;
        }
        if (pulse < 5u && count == five_pulses[pulse]) {
            if (!five_observe_hook(m, pulse, &e)) goto done;
            ++pulse; continue;
        }
        if (pulse < 5u && target > five_pulses[pulse]) target = five_pulses[pulse];
        if (cold && target > BASE) target = BASE;
        /* The refusal fires INSIDE the final chunk (the hook refuses before
         * the step retires), so the refusal triple is the loop exit. */
        if (target <= count && !(cold && count == BASE)) {
            semu_run_limits step = {UINT64_C(2000000000) - count,
                UINT64_C(45000000000) - now};
            reason = semu_machine_run(m, &step, &e);
            break;
        }
        semu_run_limits limits = {target - count, UINT64_C(45000000000) - now};
        reason = semu_machine_run(m, &limits, &e);
        if (reason != SEMU_STOP_BUDGET) break;
    }
    printf("END reason=%u pc=%08x count=%" PRIu64 " time=%" PRIu64 " frames=%" PRIu64
        " crc=%08x sha=%s error=%s\n", (unsigned)reason, semu_machine_program_counter(m),
        semu_machine_instructions(m), semu_machine_virtual_time(m), obs.frames, obs.crc, obs.hash, e.text);
    if (reason != SEMU_STOP_COMPAT_REFUSED || pulse != 5u ||
        semu_machine_program_counter(m) != FIVE_REFUSAL_PC ||
        semu_machine_instructions(m) != FIVE_REFUSAL ||
        semu_machine_virtual_time(m) != FIVE_REFUSAL_TIME ||
        strcmp(e.text, FIVE_REFUSAL_DETAIL) ||
        /* Both resume modes re-fire the loaded refusal state with zero
         * frames: the refusal happens BEFORE the step retires, so no
         * display publishes during the resumed run. The refusal image is
         * the same on every terminal path; the normalized-machine-section
         * comparison (the sole authorized ticket-769 difference removed)
         * pins it twice: 33e1dbdf… raw = 3b59452a… normalized. */
        obs.frames != 0u || obs.crc != 0u || obs.hash[0] != '\0' ||
        !save(m, argv[5], "final", &e) ||
        !compare_diagnostic(m, argv[5],
            "3b59452a1a025bcc59496608d4a9a0b3787f33e839b76c7337deff30cc6d3009", &e)) goto done;
    {
        /* The refusal re-fires on one more step at the identical stop and
         * re-saves the identical image: a stable refusal, not a one-shot. */
        uint64_t count = semu_machine_instructions(m), now = semu_machine_virtual_time(m);
        semu_run_limits retry = {1u, UINT64_C(45000000000)};
        if (semu_machine_run(m, &retry, &e) != SEMU_STOP_COMPAT_REFUSED ||
            semu_machine_instructions(m) != count || semu_machine_virtual_time(m) != now ||
            semu_machine_program_counter(m) != FIVE_REFUSAL_PC ||
            !save(m, argv[5], "refused", &e)) goto done;
    }
    result = 0;
done:
    if (result) fprintf(stderr, "private five-pulse gate failed: %s\n", e.text);
    semu_snapshot_destroy(s); semu_machine_destroy(m); semu_nema_backend_destroy(backend);
    return result;
}
