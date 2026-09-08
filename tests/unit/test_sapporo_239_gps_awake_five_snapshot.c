/* Share only synthetic test constructors and byte-comparison helpers. */
#define main legacy_awake_snapshot_main
#include "test_sapporo_239_gps_awake_snapshot.c"
#undef main

static void test_five_snapshot_phases_and_identity(semu_test_context *context)
{
    for (unsigned phase = 0u; phase < 4u; ++phase) {
        char path[128], old_path[128]; semu_error e; semu_logger logger, old_logger;
        semu_machine *m = synthetic_machine_layer(path, &logger,
            &semu_sapporo_239_gps_awake_five_layer, &e);
        semu_machine *old = synthetic_machine(old_path, &old_logger, &e);
        semu_snapshot *s = semu_snapshot_create(&e), *a = semu_snapshot_create(&e);
        semu_snapshot *b = semu_snapshot_create(&e), *old_s = semu_snapshot_create(&e);
        SEMU_TEST_ASSERT(context, m && old && s && a && b && old_s);
        arm(context, m); finish(context, m); /* Four completed pulses. */
        if (phase > 0u) arm(context, m);
        if (phase > 1u) SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_scheduler_advance(m->scheduler, 100000000u, &e));
        if (phase > 2u) SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_scheduler_advance(m->scheduler, 1000000u, &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, s, &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(old, old_s, &e));
        SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT, semu_machine_snapshot_load(old, s, &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(old, b, &e));
        equal(context, old_s, b);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT, semu_machine_snapshot_load(m, old_s, &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, b, &e));
        equal(context, s, b);
        for (unsigned run = 0u; run < 2u; ++run) {
            if (run) {
                SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_reset(m, &e));
                SEMU_TEST_EQ_U64(context, 0u, m->layers[2].hits);
                SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_load(m, s, &e));
            }
            if (!phase) arm(context, m);
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_scheduler_advance(m->scheduler,
                UINT64_C(505000000) - semu_scheduler_now(m->scheduler), &e));
            SEMU_TEST_EQ_U64(context, 5u, m->layers[2].hits);
            SEMU_TEST_EQ_U64(context, 0u, semu_apollo4_gpio_get_input(m->soc->gpio, 24u));
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, run ? b : a, &e));
        }
        equal(context, a, b);
        /* Over-limit serialized counters refuse atomically under the new ID. */
        {
            const uint8_t *data; uint8_t *copy; size_t size, aggregate = 29u;
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_snapshot_read_section(a, SEMU_SNAPSHOT_SECTION_MACHINE, &data, &size));
            copy = malloc(size); SEMU_TEST_ASSERT(context, copy != NULL); memcpy(copy, data, size);
            for (unsigned i = 0u; i < 2u; ++i) aggregate += 33u + strlen(m->layers[i].descriptor->id);
            aggregate += strlen(m->layers[2].descriptor->id);
            put64(copy + aggregate, 6u); put64(copy + aggregate + 12u, 6u);
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_snapshot_write_section(a, SEMU_SNAPSHOT_SECTION_MACHINE, copy, size, &e));
            free(copy);
            SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT, semu_machine_snapshot_load(m, a, &e));
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_machine_snapshot_save(m, a, &e));
            equal(context, a, b);
        }
        semu_snapshot_destroy(s); semu_snapshot_destroy(a); semu_snapshot_destroy(b);
        semu_snapshot_destroy(old_s); semu_machine_destroy(m); semu_machine_destroy(old);
        (void)remove(path); (void)remove(old_path);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_five_snapshot_phases_and_identity)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
