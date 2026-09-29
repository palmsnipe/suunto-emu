/* Private firmware gate; no diagnostic adapter or guest-state mutation. */
#define main personal_gate_main
#include "sapporo_239_personal_probe.c"
#undef main
#include "../../src/boards/machine_internal.h"

#define BASE UINT64_C(3960123530)

/* A sleeping dispatch may advance time without retiring an instruction.
 * Reach the measured time too; never inject at an earlier same-count edge. */
static int boundary(semu_machine *m, uint64_t count, uint64_t time, semu_error *e)
{
    if (semu_machine_instructions(m) != count) return 0;
    while (semu_machine_virtual_time(m) < time) {
        semu_run_limits step = {1u, 1u};
        if (semu_machine_run(m, &step, e) != SEMU_STOP_BUDGET ||
            semu_machine_instructions(m) != count) return 0;
    }
    return semu_machine_virtual_time(m) == time;
}

static uint32_t le32(const uint8_t *p)
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
        size < 24u || le32(data + 20u) != 4u) goto done;
    for (unsigned i = 0u; i < 4u; ++i) {
        uint32_t length, counters;
        if (offset > size || size - offset < 4u) goto done;
        length = le32(data + offset);
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
        counters = le32(data + offset + 4u + length + 9u);
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
    const uint64_t counts[] = {
        847389018u,850221529u,1132984059u,1136723800u,
        2103476826u,2107221220u,2397641544u,2404140067u,2701906762u,2708404518u,
        3016651598u,3023149894u,3326045273u,3332543381u,3637404457u,3640223889u,
        UINT64_C(3991602893),UINT64_C(3995360381)
    };
    const uint64_t times[] = {
        UINT64_C(12010884553),UINT64_C(12096961148),UINT64_C(14075897022),UINT64_C(14161873768),
        UINT64_C(20020743409),UINT64_C(20106721328),UINT64_C(22084812137),UINT64_C(22170370053),
        UINT64_C(24048201779),UINT64_C(24133728411),UINT64_C(26010661264),UINT64_C(26096218953),
        UINT64_C(28022050108),UINT64_C(28107577092),UINT64_C(30036360075),UINT64_C(30116594734),
        UINT64_C(34088644931),UINT64_C(34174452535)
    };
    const uint64_t phases[] = {BASE + 1u, UINT64_C(3962901511), UINT64_C(3962901514), UINT64_C(3962902096)};
    const uint64_t phase_times[] = {UINT64_C(32455738920), UINT64_C(32555739360),
        UINT64_C(32555739363), UINT64_C(32556738919)};
    const char *suffix[] = {"pending", "irq", "high", "fallen"};
    semu_profile p; semu_firmware_manifest fw; semu_machine_options o = {0};
    semu_machine *m = NULL; semu_snapshot *s = NULL; semu_nema_backend *backend = NULL;
    semu_logger logger; semu_error e; observer obs = {0};
    semu_stop_reason reason = SEMU_STOP_BUDGET;
    unsigned edge, phase = 0u, edge_end; int cold, middle, result = 2;
    semu_error_clear(&e);
    if (argc != 7 || (strcmp(argv[6], "prefix") && strcmp(argv[6], "idle") && strcmp(argv[6], "middle"))) return 2;
    cold = strcmp(argv[3], "cold") == 0; middle = strcmp(argv[6], "middle") == 0;
    if (cold != (strcmp(argv[6], "prefix") == 0)) return 2;
    if (semu_profile_load("profiles/sapporo/2.39.20/profile.semu", &p, &e) != SEMU_OK ||
        semu_manifest_load(argv[1], &fw, &e) != SEMU_OK ||
        semu_manifest_validate(&p, &fw, &e) != SEMU_OK ||
        !file_hash(argv[2], "37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb", &e) ||
        (!cold && !file_hash(argv[3], argv[4], &e))) goto done;
    semu_log_init(&logger, stderr, SEMU_LOG_INFO);
    backend = semu_nema_backend_create(&e); if (!backend) goto done;
    o.profile = &p; o.firmware = &fw; o.logger = &logger;
    o.external_flash_path = argv[2]; o.layers = layers; o.layer_count = 4u;
    o.display_backend = &semu_nema_backend_ops; o.display_backend_context = backend;
    o.display_snapshot = &semu_nema_backend_snapshot_ops;
    o.frame_callback = frame; o.frame_context = &obs;
    m = semu_machine_create(&o, &e); if (!m) goto done;
    if (!cold) {
        s = semu_snapshot_create(&e);
        if (!s || semu_cli_snapshot_load_file(argv[3], s, &e) != SEMU_OK ||
            semu_machine_snapshot_load(m, s, &e) != SEMU_OK) goto done;
        uint64_t count = semu_machine_instructions(m);
        if (count != BASE && count != phases[0] && count != phases[1] &&
            count != phases[2] && count != phases[3]) goto done;
        while (phase < 4u && phases[phase] < count) ++phase;
    }
    edge = cold ? 0u : 16u; edge_end = middle ? 18u : 16u;
    while (semu_machine_instructions(m) < UINT64_C(6000000000) &&
        semu_machine_virtual_time(m) < UINT64_C(45000000000)) {
        uint64_t count = semu_machine_instructions(m), now = semu_machine_virtual_time(m);
        uint64_t target = cold ? BASE : UINT64_C(6000000000);
        if (cold && count == BASE) {
            if (now != UINT64_C(32455738919) || semu_machine_program_counter(m) != 0x1291ccu ||
                !save(m, argv[5], "prefix", &e)) goto done;
            result = 0; goto done;
        }
        if (!cold && phase < 4u && count == phases[phase]) {
            uint8_t flag;
            if (!boundary(m, count, phase_times[phase], &e) ||
                semu_bus_copy_out(m->bus, 0x100588a2u, &flag, 1u, &e) != SEMU_OK ||
                (phase == 1u && (semu_machine_program_counter(m) != 0x128926u || flag != 0u)) ||
                (phase == 2u && (semu_machine_program_counter(m) != 0x12892eu || flag != 1u)) ||
                !save(m, argv[5], suffix[phase], &e)) goto done;
            printf("PHASE %s count=%" PRIu64 " time=%" PRIu64 " flag=%u\n",
                suffix[phase], count, semu_machine_virtual_time(m), flag);
            ++phase; continue;
        }
        if (edge < edge_end && count == counts[edge]) {
            semu_input_event event = {SEMU_INPUT_BUTTON, SEMU_BUTTON_MIDDLE, (int32_t)(edge % 2u), 0, 0};
            if (!boundary(m, count, times[edge], &e) || semu_machine_input(m, &event, &e) != SEMU_OK) goto done;
            printf("INPUT count=%" PRIu64 " time=%" PRIu64 " value=%u\n", count, times[edge], edge % 2u);
            ++edge; continue;
        }
        if (!cold && phase < 4u && target > phases[phase]) target = phases[phase];
        if (edge < edge_end && target > counts[edge]) target = counts[edge];
        if (target <= count) goto done;
        semu_run_limits limits = {target - count, UINT64_C(45000000000) - now};
        reason = semu_machine_run(m, &limits, &e);
        if (reason != SEMU_STOP_BUDGET) break;
    }
    printf("END reason=%u pc=%08x count=%" PRIu64 " time=%" PRIu64 " frames=%" PRIu64
        " crc=%08x sha=%s error=%s\n", (unsigned)reason, semu_machine_program_counter(m),
        semu_machine_instructions(m), semu_machine_virtual_time(m), obs.frames, obs.crc, obs.hash, e.text);
    if (reason != SEMU_STOP_COMPAT_REFUSED || edge != edge_end ||
        semu_machine_program_counter(m) != 0x1291ccu ||
        semu_machine_instructions(m) != (middle ? UINT64_C(4345171340) : UINT64_C(4071207676)) ||
        semu_machine_virtual_time(m) != (middle ? UINT64_C(37899807613) : UINT64_C(37929735196)) ||
        obs.frames != (middle ? 75u : 0u) || (middle && (obs.crc != 0xcd4c0a99u ||
        strcmp(obs.hash, "33339448cbcfafa47bd9d0ed4e37b61abd43062acf95f2bf7470ebefae921072"))) ||
        !save(m, argv[5], "final", &e) ||
        !compare_diagnostic(m, argv[5], middle ?
            "65255eb1abe56f8f3ff82e1320dfce40c7671a52e446f15b3cdced37768a83d5" :
            "127214e55e966741d3cc3acb5fd5fad50988b3cb4bdadb78788e590b91f8df28", &e)) goto done;
    {
        semu_run_limits limits = {1u, 1u};
        if (semu_machine_run(m, &limits, &e) != SEMU_STOP_COMPAT_REFUSED ||
            !save(m, argv[5], "refused", &e)) goto done;
    }
    result = 0;
done:
    if (result) fprintf(stderr, "private five-pulse gate failed: %s\n", e.text);
    semu_snapshot_destroy(s); semu_machine_destroy(m); semu_nema_backend_destroy(backend);
    return result;
}
