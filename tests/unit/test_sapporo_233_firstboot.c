/* Ticket 710 instance-36 / E-SAP-0066/0068: the 2.33 first-boot result
 * translations at the shared startup-client checker 0x119734. Name-based
 * on [client+12] with the command byte at +16; only the observed seven
 * (name, command) pairs are admitted, each once. */
#include "sapporo_233_firstboot.h"
#include "test.h"
#include <string.h>

typedef struct {
    semu_sapporo_233_firstboot_context ctx;
    semu_layer_state state;
    semu_logger logger;
    FILE *log;
    semu_bus *bus;
} fixture;

static int init(fixture *f, semu_error *e)
{
    const char *hashes[3];
    unsigned i;
    memset(f, 0, sizeof(*f));
    f->log = tmpfile();
    f->bus = semu_bus_create(e);
    if (!f->log || !f->bus) return 0;
    semu_log_init(&f->logger, f->log, SEMU_LOG_WARNING);
    if (semu_bus_map_ram(f->bus, "sram", UINT32_C(0x10000000),
            UINT32_C(0x180000), e) != SEMU_OK ||
        semu_bus_map_ram(f->bus, "mram", 0u, UINT32_C(0x00200000), e) !=
            SEMU_OK) {
        return 0;
    }
    for (i = 0u; i < 3u; ++i) hashes[i] =
        semu_sapporo_233_firstboot_layer.component_hashes[i];
    if (semu_layer_enable_checked(&f->state,
            &semu_sapporo_233_firstboot_layer, "sapporo-2.33.16", hashes,
            3u, e) != SEMU_OK) {
        return 0;
    }
    f->ctx.state = &f->state;
    f->ctx.logger = &f->logger;
    return 1;
}

static int put_client(semu_bus *bus, uint32_t client, const char *name,
                      uint8_t command, semu_error *e)
{
    uint32_t name_ptr = UINT32_C(0x001b0000);
    size_t len = strlen(name);
    uint32_t i;
    if (semu_bus_write(bus, client + 12u, 4u, name_ptr, e) != SEMU_OK)
        return 0;
    for (i = 0u; i <= len; ++i) {
        if (semu_bus_write(bus, name_ptr + i, 1u,
                           (uint8_t)name[i], e) != SEMU_OK) return 0;
    }
    return semu_bus_write(bus, client + 16u, 1u, command, e) == SEMU_OK;
}

static void test_translation_and_budgets(semu_test_context *context)
{
    fixture f;
    semu_error e;
    semu_cpu_state cpu;
    static const struct { const char *name; uint8_t command; } admitted[] = {
        { "WbStoPreload", 0u }, { "sleepln", 0u }, { "TrainingTss", 0u },
        { "PoiP", 1u }, { "sleepln", 1u }, { "actitmln", 1u },
        { "TrainingTss", 1u }
    };
    unsigned i;
    SEMU_TEST_ASSERT(context, init(&f, &e));
    memset(&cpu, 0, sizeof(cpu));
    cpu.r[15] = SEMU_SAPPORO_233_FIRSTBOOT_PC;
    /* Every admitted pair translates exactly once; the aggregate budget is
     * the rule count. */
    for (i = 0u; i < sizeof(admitted) / sizeof(admitted[0]); ++i) {
        cpu.r[0] = UINT32_C(0x10028f04);
        cpu.r[3] = 500u;
        SEMU_TEST_ASSERT(context, put_client(f.bus, cpu.r[0],
            admitted[i].name, admitted[i].command, &e));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_233_firstboot_apply(&f.ctx, f.bus, &cpu, &e));
        SEMU_TEST_EQ_U64(context, 200u, cpu.r[3]);
        SEMU_TEST_EQ_U64(context, 1u, f.state.hits);
        /* A repeated pair consumes another aggregate hit (the layer budget
         * is the staged window's observed total); re-enable before the
         * next pair so every admitted pair is exercised. */
        cpu.r[3] = 500u;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_sapporo_233_firstboot_apply(&f.ctx, f.bus, &cpu, &e));
        SEMU_TEST_EQ_U64(context, 200u, cpu.r[3]);
        SEMU_TEST_EQ_U64(context, 2u, f.state.hits);
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_layer_enable(&f.state,
            &semu_sapporo_233_firstboot_layer, "sapporo-2.33.16", &e));
    }
    /* Non-500 statuses and unknown names stay native. */
    cpu.r[3] = 200u;
    SEMU_TEST_ASSERT(context, put_client(f.bus, cpu.r[0], "WbStoPreload",
        0u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_firstboot_apply(
        &f.ctx, f.bus, &cpu, &e));
    SEMU_TEST_EQ_U64(context, 200u, cpu.r[3]);
    cpu.r[3] = 500u;
    SEMU_TEST_ASSERT(context, put_client(f.bus, cpu.r[0], "Unlisted", 0u,
        &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_firstboot_apply(
        &f.ctx, f.bus, &cpu, &e));
    SEMU_TEST_EQ_U64(context, 500u, cpu.r[3]);
    /* A client outside SRAM stays native; a wrong profile refuses. */
    cpu.r[0] = UINT32_C(0x20000000);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_233_firstboot_apply(
        &f.ctx, f.bus, &cpu, &e));
    cpu.r[0] = UINT32_C(0x10028f04);
    SEMU_TEST_ASSERT(context, put_client(f.bus, cpu.r[0], "sleepln", 0u,
        &e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_layer_enable_checked(&f.state,
            &semu_sapporo_233_firstboot_layer, "sapporo-2.35.34",
            semu_sapporo_233_firstboot_layer.component_hashes, 3u, &e));
    (void)fclose(f.log);
    semu_bus_destroy(f.bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_translation_and_budgets)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
