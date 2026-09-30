#include "test.h"
#include "../../src/soc/apollo4/apollo4_internal.h"
#include "../../src/soc/apollo4/iom_internal.h"
#include "../../src/core/bus_internal.h"
#include <string.h>

#define BASE UINT32_C(0x40052000)
#define RAM UINT32_C(0x10010000)

typedef struct fixture {
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4 *soc;
    semu_serial_endpoint endpoint;
    semu_error error;
    unsigned rises, calls;
} fixture;

static void irq(void *context, unsigned line, int level)
{
    fixture *f = context;
    if (line == 8u && level) ++f->rises;
}

static semu_transaction_result absent(void *context,
    semu_serial_transaction *transaction, semu_error *error)
{
    fixture *f = context;
    (void)transaction;
    ++f->calls;
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "test endpoint absent");
    return SEMU_TRANSACTION_REFUSE;
}

static int create(fixture *f, const char *profile)
{
    memset(f, 0, sizeof(*f));
    f->bus = semu_bus_create(&f->error);
    f->scheduler = semu_scheduler_create(&f->error);
    if (!f->bus || !f->scheduler) return 0;
    f->soc = semu_apollo4_create(f->bus, &f->error);
    f->endpoint.name = "absent-test";
    f->endpoint.context = f;
    f->endpoint.transfer = absent;
    return f->soc && semu_apollo4_init(f->soc, f->scheduler, irq, f, &f->error) == SEMU_OK &&
        semu_apollo4_select_profile(f->soc, profile, &f->error) == SEMU_OK &&
        semu_apollo4_iom_attach_endpoint(f->soc->iom2, &f->endpoint, &f->error) == SEMU_OK &&
        semu_bus_map_ram(f->bus, "target", RAM, 4u, &f->error) == SEMU_OK;
}

static void destroy(fixture *f)
{
    semu_apollo4_destroy(f->soc);
    semu_bus_destroy(f->bus);
    semu_scheduler_destroy(f->scheduler);
}

static void write_reg(semu_test_context *c, fixture *f, uint32_t offset, uint32_t value)
{
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_write(f->bus, BASE + offset, 4u, value, &f->error));
}

static void read_reg(semu_test_context *c, fixture *f, uint32_t offset, uint32_t expected)
{
    uint32_t value = 0u;
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_read(f->bus, BASE + offset, 4u, &value, &f->error));
    SEMU_TEST_EQ_U64(c, expected, value);
}

static void prepare(semu_test_context *c, fixture *f, uint32_t address)
{
    semu_apollo4_iom_reset(f->soc->iom2);
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_write(f->bus, RAM, 4u, 0xabcdefa5u, &f->error));
    write_reg(c, f, 0x11cu, 0x10u);
    write_reg(c, f, 0x200u, 0x442u);
    write_reg(c, f, 0x2c4u, address);
    write_reg(c, f, 0x21cu, 1u);
    write_reg(c, f, 0x220u, RAM);
    write_reg(c, f, 0x218u, 0x101u);
}

static void test_sapporo_235_pressure_negative_probe(semu_test_context *c)
{
    fixture f;
    uint32_t address, value;
    SEMU_TEST_ASSERT(c, create(&f, "sapporo-2.35.34"));
    for (address = 0x5cu; address <= 0x5du; ++address) {
        prepare(c, &f, address);
        write_reg(c, &f, 0x120u, 0x0f000112u);
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_read(f.bus, RAM, 4u, &value, &f.error));
        SEMU_TEST_EQ_U64(c, 0xabcdef00u, value);
        read_reg(c, &f, 0x204u, 0x442u);
        read_reg(c, &f, 0x224u, 2u);
        read_reg(c, &f, 0x218u, 0x100u);
        read_reg(c, &f, 0x214u, 4u);
        SEMU_TEST_EQ_U64(c, 0u, f.calls);
        SEMU_TEST_EQ_U64(c, address - 0x5bu, f.rises);
    }
    destroy(&f);
}

static void test_sapporo_235_pressure_refusal_atomic(semu_test_context *c)
{
    fixture f;
    const uint8_t rom[] = {0xa5u};
    unsigned variant;
    SEMU_TEST_ASSERT(c, create(&f, "sapporo-2.35.34"));
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_map_rom(f.bus, "rom", RAM + 0x1000u, rom, 1u, &f.error));
    for (variant = 0u; variant < 7u; ++variant) {
        semu_apollo4_iom before;
        uint32_t command = 0x0f000112u, value;
        unsigned rises = f.rises;
        prepare(c, &f, 0x5cu);
        if (variant == 0u) command = 0x10000112u;
        if (variant == 1u) write_reg(c, &f, 0x21cu, 2u);
        if (variant == 2u) write_reg(c, &f, 0x218u, 0x103u);
        if (variant == 3u) write_reg(c, &f, 0x218u, 0x100u);
        if (variant == 4u) write_reg(c, &f, 0x220u, RAM + 4u);
        if (variant == 5u) write_reg(c, &f, 0x220u, RAM + 0x1000u);
        if (variant == 6u) write_reg(c, &f, 0x11cu, 0u);
        before = *f.soc->iom2;
        SEMU_TEST_ASSERT(c, semu_bus_write(f.bus, BASE + 0x120u, 4u, command, &f.error) != SEMU_OK);
        SEMU_TEST_ASSERT(c, memcmp(&before, f.soc->iom2, sizeof(before)) == 0);
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_read(f.bus, RAM, 4u, &value, &f.error));
        SEMU_TEST_EQ_U64(c, 0xabcdefa5u, value);
        SEMU_TEST_EQ_U64(c, rises, f.rises);
        SEMU_TEST_EQ_U64(c, 0u, f.calls);
    }
    destroy(&f);
}

static void test_sapporo_235_pressure_profile_isolation(semu_test_context *c)
{
    static const char *profiles[] = {"sapporo-2.22.60", "sapporo-2.33.16", "sapporo-2.39.20"};
    fixture a, b;
    size_t i;
    SEMU_TEST_ASSERT(c, create(&a, "sapporo-2.35.34"));
    for (i = 0u; i < SEMU_ARRAY_LEN(profiles); ++i) {
        SEMU_TEST_ASSERT(c, create(&b, profiles[i]));
        prepare(c, &b, 0x5du);
        SEMU_TEST_ASSERT(c, semu_bus_write(b.bus, BASE + 0x120u, 4u, 0x0f000112u, &b.error) != SEMU_OK);
        destroy(&b);
        prepare(c, &a, 0x5du);
        write_reg(c, &a, 0x120u, 0x0f000112u);
    }
    prepare(c, &a, 0x5eu);
    SEMU_TEST_ASSERT(c, semu_bus_write(a.bus, BASE + 0x120u, 4u, 0x0f000112u, &a.error) != SEMU_OK);
    destroy(&a);
}

/* Ticket 792: the pressure IOM2 state is codec-covered now. The probe
 * state round-trips; only a wrong-profile consumer refuses the image. */
static void test_sapporo_235_pressure_snapshot_roundtrip(semu_test_context *c)
{
    fixture f;
    fixture g;
    fixture stub;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    SEMU_TEST_ASSERT(c, create(&f, "sapporo-2.35.34"));
    SEMU_TEST_ASSERT(c, create(&g, "sapporo-2.35.34"));
    SEMU_TEST_ASSERT(c, create(&stub, "sapporo-2.22.60"));
    prepare(c, &f, 0x5du);
    write_reg(c, &f, 0x120u, 0x0f000112u);
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_apollo4_iom_snapshot_write(f.soc->iom2, &writer, &f.error));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_apollo4_iom_snapshot_read(g.soc->iom2, &reader, &g.error));
    SEMU_TEST_ASSERT(c, semu_snapshot_reader_done(&reader));
    read_reg(c, &g, 0x204u, 0x442u);
    read_reg(c, &g, 0x214u, 4u);
    read_reg(c, &g, 0x218u, 0x100u);
    read_reg(c, &g, 0x224u, 2u);
    SEMU_TEST_EQ_U64(c, f.soc->iom2->device_config,
                     g.soc->iom2->device_config);
    SEMU_TEST_EQ_U64(c, f.soc->iom2->inten, g.soc->iom2->inten);
    SEMU_TEST_EQ_U64(c, f.soc->iom2->dma_count, g.soc->iom2->dma_count);
    SEMU_TEST_EQ_U64(c, f.soc->iom2->dma_target, g.soc->iom2->dma_target);
    SEMU_TEST_ASSERT(c, f.soc->iom2->endpoint_attached ==
                     g.soc->iom2->endpoint_attached);
    SEMU_TEST_ASSERT(c, f.soc->iom2->irq_level == g.soc->iom2->irq_level);
    SEMU_TEST_ASSERT(c, memcmp(f.soc->iom2->observed_registers,
                               g.soc->iom2->observed_registers,
                               sizeof(f.soc->iom2->observed_registers)) == 0);
    /* A stub-profile IOM2 refuses the pressure image: the probe's
     * DMA config lies outside the shared-law mask. */
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    semu_error_clear(&stub.error);
    SEMU_TEST_EQ_U64(c, SEMU_ERR_FORMAT,
        semu_apollo4_iom_snapshot_read(stub.soc->iom2, &reader, &stub.error));
    semu_snapshot_writer_destroy(&writer);
    destroy(&stub);
    destroy(&g);
    destroy(&f);
}

static semu_status overlay_write(void *context, uint32_t offset,
    unsigned width, uint32_t value, semu_error *error)
{
    fixture *f = context;
    (void)offset; (void)width; (void)value;
    ++f->calls;
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status overlay_read(void *context, uint32_t offset,
    unsigned width, uint32_t *value, semu_error *error)
{
    *value = 0u;
    return overlay_write(context, offset, width, 0u, error);
}

static void test_sapporo_235_pressure_mmio_refuses(semu_test_context *c)
{
    fixture f;
    semu_apollo4_iom before;
    const semu_bus_device_ops ops = {overlay_read, overlay_write, NULL};
    SEMU_TEST_ASSERT(c, create(&f, "sapporo-2.35.34"));
    prepare(c, &f, 0x5du);
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_map_overlay(f.bus,
        "overlay", RAM, 1u, &ops, &f, &f.error));
    before = *f.soc->iom2;
    SEMU_TEST_ASSERT(c, semu_bus_write(f.bus, BASE + 0x120u, 4u,
        0x0f000112u, &f.error) != SEMU_OK);
    SEMU_TEST_ASSERT(c, memcmp(&before, f.soc->iom2, sizeof(before)) == 0);
    SEMU_TEST_EQ_U64(c, 0u, f.calls);
    SEMU_TEST_EQ_U64(c, 0u, f.rises);
    destroy(&f);
}

int main(void)
{
    const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_sapporo_235_pressure_negative_probe),
        SEMU_TEST_CASE(test_sapporo_235_pressure_refusal_atomic),
        SEMU_TEST_CASE(test_sapporo_235_pressure_profile_isolation),
        SEMU_TEST_CASE(test_sapporo_235_pressure_snapshot_roundtrip),
        SEMU_TEST_CASE(test_sapporo_235_pressure_mmio_refuses)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
