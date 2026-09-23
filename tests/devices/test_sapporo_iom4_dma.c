/* DMA memory admission under the execution-model whole-transfer contract.
 * These synthetic cases test emulator safety, not new peripheral behavior. */
#include "test.h"
#include "../../src/core/bus_internal.h"
#include "../../src/devices/sapporo_iom4_internal.h"

#include <string.h>

#define BASE 0x10000000u

typedef struct fixture {
    semu_bus *bus;
    semu_sapporo_iom4 *iom;
    semu_error error;
    unsigned callbacks;
} fixture;

static void irq(void *context, unsigned line, int level)
{
    fixture *f = context;
    (void)line;
    (void)level;
    ++f->callbacks;
}

static semu_status device_read(void *context, uint32_t offset,
                               unsigned width, uint32_t *value,
                               semu_error *error)
{
    fixture *f = context;
    (void)offset;
    (void)width;
    ++f->callbacks;
    *value = 0u;
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status device_write(void *context, uint32_t offset,
                                unsigned width, uint32_t value,
                                semu_error *error)
{
    uint32_t ignored;
    (void)value;
    return device_read(context, offset, width, &ignored, error);
}

static void wr(semu_test_context *c, fixture *f, uint32_t offset,
               uint32_t value)
{
    SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_sapporo_iom4_write(f->iom, offset, 4u, value, &f->error));
}

static int setup(semu_test_context *c, fixture *f, int m2p)
{
    memset(f, 0, sizeof(*f));
    f->bus = semu_bus_create(&f->error);
    if (f->bus == NULL) return 0;
    f->iom = semu_sapporo_iom4_create(f->bus, irq, f, &f->error);
    if (f->iom == NULL) {
        semu_bus_destroy(f->bus);
        return 0;
    }
    wr(c, f, R_SUBMOD, 0x10u);
    wr(c, f, R_INTEN, 0x7fffu);
    wr(c, f, R_DEVICE_CFG, ADDR_GAUGE);
    wr(c, f, R_DMA_CFG, m2p ? 0x103u : 0x101u);
    wr(c, f, R_DMA_TARGET, BASE);
    wr(c, f, R_DMA_TOTAL, 8u);
    return 1;
}

static void destroy(fixture *f)
{
    semu_sapporo_iom4_destroy(f->iom);
    semu_bus_destroy(f->bus);
}

static void refused(semu_test_context *c, fixture *f, int m2p)
{
    semu_sapporo_iom4 before = *f->iom;
    unsigned callbacks = f->callbacks;
    uint8_t before_bytes[4], after_bytes[4];
    semu_status status;
    SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_bus_copy_out(f->bus, BASE, before_bytes, 4u, &f->error));
    status = semu_sapporo_iom4_write(f->iom, R_COMMAND, 4u,
        m2p ? 0x09000811u : 0x09000812u, &f->error);
    SEMU_TEST_ASSERT(c, status != SEMU_OK);
    SEMU_TEST_EQ_U64(c, status, f->error.code);
    SEMU_TEST_ASSERT(c, f->error.text[0] != '\0');
    SEMU_TEST_ASSERT(c, memcmp(&before, f->iom, sizeof(before)) == 0);
    SEMU_TEST_EQ_U64(c, callbacks, f->callbacks);
    SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_bus_copy_out(f->bus, BASE, after_bytes, 4u, &f->error));
    SEMU_TEST_ASSERT(c, memcmp(before_bytes, after_bytes, 4u) == 0);
}

static void test_dma_unmapped_tail_atomic(semu_test_context *c)
{
    fixture f;
    int m2p;
    for (m2p = 0; m2p <= 1; ++m2p) {
        SEMU_TEST_ASSERT(c, setup(c, &f, m2p));
        SEMU_TEST_EQ_U64(c, SEMU_OK,
            semu_bus_map_ram(f.bus, "prefix", BASE, 4u, &f.error));
        SEMU_TEST_EQ_U64(c, SEMU_OK,
            semu_bus_write(f.bus, BASE, 4u, 0xaabbccddu, &f.error));
        refused(c, &f, m2p);
        /* Correcting only the memory mapping permits the same command. */
        SEMU_TEST_EQ_U64(c, SEMU_OK,
            semu_bus_map_ram(f.bus, "tail", BASE + 4u, 4u, &f.error));
        wr(c, &f, R_COMMAND, m2p ? 0x09000811u : 0x09000812u);
        SEMU_TEST_EQ_U64(c, 2u, f.iom->dma_status);
        SEMU_TEST_ASSERT(c, (f.iom->intstat & INT_DMA_CMP) != 0u);
        SEMU_TEST_ASSERT(c, (f.iom->intstat & INT_DMA_ERR) == 0u);
        destroy(&f);
    }
}

static void test_dma_rom_destination_atomic(semu_test_context *c)
{
    static const uint8_t bytes[4] = { 1u, 2u, 3u, 4u };
    fixture f;
    SEMU_TEST_ASSERT(c, setup(c, &f, 0));
    SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_bus_map_ram(f.bus, "prefix", BASE, 4u, &f.error));
    SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_bus_map_rom(f.bus, "tail", BASE + 4u, bytes, 4u, &f.error));
    refused(c, &f, 0);
    destroy(&f);
}

static void test_dma_device_overlay_atomic(semu_test_context *c)
{
    const semu_bus_device_ops ops = { device_read, device_write, NULL };
    fixture f;
    int m2p;
    for (m2p = 0; m2p <= 1; ++m2p) {
        SEMU_TEST_ASSERT(c, setup(c, &f, m2p));
        SEMU_TEST_EQ_U64(c, SEMU_OK,
            semu_bus_map_ram(f.bus, "ram", BASE, 8u, &f.error));
        /* A wide copy would bypass this one-byte device overlay. */
        SEMU_TEST_EQ_U64(c, SEMU_OK, semu_bus_map_overlay(f.bus,
            "canary", BASE + 6u, 1u, &ops, &f, &f.error));
        refused(c, &f, m2p);
        destroy(&f);
    }
}

static void test_dma_adjacent_rom_source(semu_test_context *c)
{
    static const uint8_t first[1] = { 0xcdu };
    static const uint8_t second[1] = { 0xabu };
    fixture f;
    SEMU_TEST_ASSERT(c, setup(c, &f, 1));
    SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_bus_map_rom(f.bus, "first", BASE, first, 1u, &f.error));
    SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_bus_map_rom(f.bus, "second", BASE + 1u, second, 1u, &f.error));
    wr(c, &f, R_DMA_TOTAL, 2u);
    wr(c, &f, R_COMMAND, 0x09000211u);
    SEMU_TEST_EQ_U64(c, 2u, f.iom->dma_status);
    wr(c, &f, R_COMMAND, 0x09000212u);
    {
        uint32_t value;
        SEMU_TEST_EQ_U64(c, SEMU_OK,
            semu_sapporo_iom4_read(f.iom, R_FIFO_POP, 4u, &value, &f.error));
        SEMU_TEST_EQ_U64(c, 0xabcdu, value);
    }
    destroy(&f);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_dma_unmapped_tail_atomic),
        SEMU_TEST_CASE(test_dma_rom_destination_atomic),
        SEMU_TEST_CASE(test_dma_device_overlay_atomic),
        SEMU_TEST_CASE(test_dma_adjacent_rom_source)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
