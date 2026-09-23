#include "test.h"
#include "sapporo_235_production.h"
#include <stdio.h>
#include <string.h>

#define TARGET UINT32_C(0x14fff000)
#define TABLE UINT32_C(0x001b1644)
#define POLYNOMIAL UINT32_C(0x82f63b78)

typedef struct fixture {
    semu_bus *bus;
    semu_layer_state state;
    semu_logger logger;
    semu_error error;
    FILE *log;
} fixture;

static int open_fixture(fixture *f, uint32_t size, int table)
{
    unsigned i, j;
    uint8_t bytes[64];
    memset(f, 0, sizeof(*f));
    f->log = tmpfile();
    semu_log_init(&f->logger, f->log, SEMU_LOG_WARNING);
    f->bus = semu_bus_create(&f->error);
    if (!f->bus || !f->log || semu_bus_map_ram(f->bus, "sector", TARGET,
            size, &f->error) != SEMU_OK) return 0;
    for (i = 0u; i < 16u; ++i) {
        uint32_t n = i;
        for (j = 0u; j < 4u; ++j) n = (n >> 1u) ^ ((n & 1u) ? POLYNOMIAL : 0u);
        for (j = 0u; j < 4u; ++j) bytes[i * 4u + j] = (uint8_t)(n >> (j * 8u));
    }
    if (table && semu_bus_map_rom(f->bus, "synthetic-table", TABLE,
            bytes, sizeof(bytes), &f->error) != SEMU_OK) return 0;
    return semu_layer_enable_checked(&f->state, &semu_sapporo_235_production_layer,
        "sapporo-2.35.34", semu_sapporo_235_production_layer.component_hashes,
        3u, &f->error) == SEMU_OK;
}

static void close_fixture(fixture *f)
{
    semu_bus_destroy(f->bus);
    if (f->log) fclose(f->log);
}

static uint32_t crc_bits(const uint8_t *p)
{
    uint32_t crc = 0u;
    unsigned i, j;
    for (i = 0u; i < 252u; ++i) {
        crc ^= p[i];
        for (j = 0u; j < 8u; ++j)
            crc = (crc >> 1u) ^ ((crc & 1u) ? POLYNOMIAL : 0u);
    }
    return crc;
}

static void test_sapporo_235_production_records(semu_test_context *context)
{
    fixture f;
    uint8_t data[4096];
    const unsigned pages[] = {0u, 1u, 2u, 3u, 6u};
    char log[512];
    size_t i, n;
    SEMU_TEST_ASSERT(context, open_fixture(&f, sizeof(data), 1));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_235_install_production(f.bus, &f.state, &f.logger, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_copy_out(f.bus, TARGET, data, sizeof(data), &f.error));
    SEMU_TEST_ASSERT(context, memcmp(data, "ProductionData", 14u) == 0);
    SEMU_TEST_ASSERT(context, data[14] == 0xe8u && data[15] == 3u && data[61] == 1u);
    SEMU_TEST_ASSERT(context, memcmp(data + 256u, "ACCR", 4u) == 0);
    SEMU_TEST_ASSERT(context, memcmp(data + 512u, "ACCC", 4u) == 0);
    SEMU_TEST_ASSERT(context, memcmp(data + 768u, "MAGN", 4u) == 0);
    SEMU_TEST_ASSERT(context, memcmp(data + 1536u, "HLAT", 4u) == 0);
    for (i = 0u; i < sizeof(pages) / sizeof(pages[0]); ++i) {
        const uint8_t *p = data + pages[i] * 256u;
        uint32_t actual = (uint32_t)p[252] | (uint32_t)p[253] << 8u |
            (uint32_t)p[254] << 16u | (uint32_t)p[255] << 24u;
        SEMU_TEST_EQ_U64(context, crc_bits(p), actual);
    }
    for (i = 1792u; i < sizeof(data); ++i) SEMU_TEST_EQ_U64(context, 0xffu, data[i]);
    SEMU_TEST_EQ_U64(context, 1u, f.state.hits);
    rewind(f.log); n = fread(log, 1u, sizeof(log) - 1u, f.log); log[n] = '\0';
    SEMU_TEST_ASSERT(context, strstr(log, "trigger=production-data ordinal=1") != NULL);
    SEMU_TEST_ASSERT(context, strstr(log, "evidence=E-SAP-0038") != NULL);
    close_fixture(&f);
}

static void test_sapporo_235_production_refusal_atomic(semu_test_context *context)
{
    fixture f;
    uint8_t before[4096], after[4096];
    unsigned variant;
    for (variant = 0u; variant < 5u; ++variant) {
        SEMU_TEST_ASSERT(context, open_fixture(&f, sizeof(before), variant != 3u));
        memset(before, 0, sizeof(before));
        if (variant == 0u) f.state.enabled = 0;
        if (variant == 1u) f.state.hits = 1u;
        if (variant == 2u) before[4095] = 1u;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_load(f.bus, TARGET, before, sizeof(before), &f.error));
        SEMU_TEST_ASSERT(context, semu_sapporo_235_install_production(f.bus,
            &f.state, variant == 4u ? NULL : &f.logger, &f.error) != SEMU_OK);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_copy_out(f.bus, TARGET, after, sizeof(after), &f.error));
        SEMU_TEST_ASSERT(context, memcmp(before, after, sizeof(before)) == 0);
        SEMU_TEST_EQ_U64(context, variant == 1u ? 1u : 0u, f.state.hits);
        SEMU_TEST_EQ_U64(context, 0u, ftell(f.log));
        close_fixture(&f);
    }
}

static void test_sapporo_235_production_short_mapping(semu_test_context *context)
{
    fixture f;
    uint32_t value = 9u;
    SEMU_TEST_ASSERT(context, open_fixture(&f, 4095u, 1));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        semu_sapporo_235_install_production(f.bus, &f.state, &f.logger, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(f.bus, TARGET, 4u, &value, &f.error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, 0u, f.state.hits);
    close_fixture(&f);
}

static void test_sapporo_235_production_identity(semu_test_context *context)
{
    fixture f;
    const char *hashes[3];
    size_t i;
    SEMU_TEST_ASSERT(context, open_fixture(&f, 4096u, 1));
    for (i = 0u; i < 3u; ++i) {
        memcpy(hashes, semu_sapporo_235_production_layer.component_hashes, sizeof(hashes));
        hashes[i] = "wrong";
        SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT, semu_layer_enable_checked(
            &f.state, &semu_sapporo_235_production_layer, "sapporo-2.35.34",
            hashes, 3u, &f.error));
        SEMU_TEST_ASSERT(context, !f.state.enabled);
    }
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT, semu_layer_enable_checked(
        &f.state, &semu_sapporo_235_production_layer, "sapporo-2.22.60",
        semu_sapporo_235_production_layer.component_hashes, 3u, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, semu_layer_enable_checked(
        &f.state, &semu_sapporo_235_production_layer, "sapporo-2.35.34",
        semu_sapporo_235_production_layer.component_hashes, 2u, &f.error));
    close_fixture(&f);
}

static void test_sapporo_235_production_reset_and_ownership(semu_test_context *context)
{
    fixture a, b;
    uint8_t data[4096], again[4096];
    SEMU_TEST_ASSERT(context, open_fixture(&a, sizeof(data), 1));
    memset(data, 0xff, sizeof(data));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_load(a.bus, TARGET, data, sizeof(data), &a.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_235_install_production(a.bus, &a.state, &a.logger, &a.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_copy_out(a.bus, TARGET, data, sizeof(data), &a.error));
    SEMU_TEST_ASSERT(context, open_fixture(&b, sizeof(data), 1));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_235_install_production(a.bus, &a.state, &a.logger, &a.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_235_install_production(b.bus, &b.state, &b.logger, &b.error));
    SEMU_TEST_EQ_U64(context, 1u, a.state.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_layer_enable(&a.state,
        &semu_sapporo_235_production_layer, "sapporo-2.35.34", &a.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_235_install_production(a.bus, &a.state, &a.logger, &a.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_copy_out(a.bus, TARGET, again, sizeof(again), &a.error));
    SEMU_TEST_ASSERT(context, memcmp(data, again, sizeof(data)) == 0);
    close_fixture(&b); close_fixture(&a);
}

int main(void)
{
    const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_sapporo_235_production_records),
        SEMU_TEST_CASE(test_sapporo_235_production_refusal_atomic),
        SEMU_TEST_CASE(test_sapporo_235_production_short_mapping),
        SEMU_TEST_CASE(test_sapporo_235_production_identity),
        SEMU_TEST_CASE(test_sapporo_235_production_reset_and_ownership)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
