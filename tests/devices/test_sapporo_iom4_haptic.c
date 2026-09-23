#include "test.h"
#include "../../src/devices/sapporo_iom4_internal.h"
#include <string.h>

#define RAM 0x10010000u

typedef struct {
    semu_bus *bus;
    semu_sapporo_iom4 *iom;
    semu_error error;
} fixture;

static void wr(semu_test_context *c, fixture *f, uint32_t offset, uint32_t value)
{
    SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_sapporo_iom4_write(f->iom, offset, 4u, value, &f->error));
}

static int setup(fixture *f)
{
    memset(f, 0, sizeof(*f));
    f->bus = semu_bus_create(&f->error);
    if (!f->bus) return 0;
    if (semu_bus_map_ram(f->bus, "dma", RAM, 8u, &f->error) != SEMU_OK)
        return 0;
    f->iom = semu_sapporo_iom4_create(f->bus, NULL, NULL, &f->error);
    return f->iom != NULL;
}

static void destroy(fixture *f)
{
    semu_sapporo_iom4_destroy(f->iom);
    semu_bus_destroy(f->bus);
}

static void prepare(semu_test_context *c, fixture *f, uint32_t command,
                    const uint8_t *bytes)
{
    uint32_t n = (command >> 8) & 0xfffu;
    if (bytes) SEMU_TEST_EQ_U64(c, SEMU_OK,
        semu_bus_load(f->bus, RAM, bytes, n, &f->error));
    wr(c, f, R_SUBMOD, 0x10u);
    wr(c, f, R_DEVICE_CFG, 0x50u);
    wr(c, f, R_INTCLR, 0x7fffu);
    wr(c, f, R_DMA_TARGET, RAM);
    wr(c, f, R_DMA_TOTAL, n);
    wr(c, f, R_DMA_CFG, (command & 15u) == 1u ? 0x103u : 0x101u);
}

static void transfer(semu_test_context *c, fixture *f, uint32_t command,
                     const uint8_t *bytes)
{
    prepare(c, f, command, bytes);
    wr(c, f, R_COMMAND, command);
    SEMU_TEST_ASSERT(c, (f->iom->intstat & (INT_CMD | INT_DMA_CMP)) ==
        (INT_CMD | INT_DMA_CMP));
    SEMU_TEST_ASSERT(c, (f->iom->intstat & INT_ILLCMD) == 0u);
    SEMU_TEST_EQ_U64(c, 2u, f->iom->dma_status);
    SEMU_TEST_EQ_U64(c, 4u, f->iom->trig_stat);
}

static uint32_t read_ram(semu_test_context *c, fixture *f)
{
    uint32_t word = 0u;
    if (semu_bus_read(f->bus, RAM, 4u, &word, &f->error) != SEMU_OK)
        semu_test_fail(c, __FILE__, __LINE__, "RAM read succeeds");
    return word;
}

static void test_haptic_autotune_reset(semu_test_context *c)
{
    const uint8_t tune[] = { 0x22u, 1u };
    fixture f;
    SEMU_TEST_ASSERT(c, setup(&f));
    transfer(c, &f, 0x22000112u, NULL);
    SEMU_TEST_EQ_U64(c, 0u, read_ram(c, &f) & 255u);
    transfer(c, &f, 0x201u, tune);
    transfer(c, &f, 0x22000112u, NULL);
    SEMU_TEST_EQ_U64(c, 3u, read_ram(c, &f) & 255u);
    transfer(c, &f, 0x23000112u, NULL);
    SEMU_TEST_EQ_U64(c, 0u, read_ram(c, &f) & 255u);
    transfer(c, &f, 0x24000112u, NULL);
    SEMU_TEST_EQ_U64(c, 0u, read_ram(c, &f) & 255u);
    semu_sapporo_iom4_reset(f.iom);
    transfer(c, &f, 0x22000112u, NULL);
    SEMU_TEST_EQ_U64(c, 0u, read_ram(c, &f) & 255u);
    destroy(&f);
}

static void test_haptic_wave_chunking(semu_test_context *c)
{
    const uint8_t wave[] = { 0x40u, 0x11u, 0x22u, 0x33u, 0xddu };
    fixture f;
    SEMU_TEST_ASSERT(c, setup(&f));
    transfer(c, &f, 0x501u, wave);
    transfer(c, &f, 0x40000412u, NULL);
    SEMU_TEST_EQ_U64(c, 0x00332211u, read_ram(c, &f));
    destroy(&f);
}

static void test_haptic_atomic_refusals(semu_test_context *c)
{
    uint8_t bytes[] = { 0x22u, 1u, 0u, 0u, 0u };
    unsigned i;
    fixture f;
    SEMU_TEST_ASSERT(c, setup(&f));
    for (i = 0; i < 12u; ++i) {
        uint32_t command = 0x201u;
        semu_sapporo_iom4 before;
        uint32_t memory;
        semu_sapporo_iom4_reset(f.iom);
        prepare(c, &f, command, bytes);
        switch (i) {
        case 0: command = 0x25000112u; wr(c, &f, R_DMA_CFG, 0x101u);
                wr(c, &f, R_DMA_TOTAL, 1u); break;
        case 1: command = 0x301u; wr(c, &f, R_DMA_TOTAL, 3u); break;
        case 2: wr(c, &f, R_DMA_TOTAL, 1u); break;
        case 3: wr(c, &f, R_DMA_CFG, 0x101u); break;
        case 4: wr(c, &f, R_DMA_CFG, 0x102u); break;
        case 5: wr(c, &f, R_SUBMOD, 1u); break;
        case 6: wr(c, &f, R_DMA_TARGET, RAM + 7u); break;
        case 7: wr(c, &f, R_DMA_TARGET, 0u); break;
        case 8: SEMU_TEST_EQ_U64(c, SEMU_OK,
                    semu_bus_write(f.bus, RAM, 1u, 0xffu, &f.error)); break;
        case 9: command |= 0x80u; break;
        case 10: wr(c, &f, R_FIFO_PUSH, 0u); break;
        case 11: command = 0x501u; wr(c, &f, R_DMA_TOTAL, 5u); break;
        }
        before = *f.iom;
        memory = read_ram(c, &f);
        SEMU_TEST_ASSERT(c, semu_sapporo_iom4_write(f.iom, R_COMMAND, 4u,
            command, &f.error) != SEMU_OK);
        SEMU_TEST_ASSERT(c, f.error.text[0] != '\0');
        SEMU_TEST_ASSERT(c, memcmp(&before, f.iom, sizeof(before)) == 0);
        SEMU_TEST_EQ_U64(c, memory, read_ram(c, &f));
    }
    destroy(&f);
}

static void test_haptic_endpoint_switch_refuses(semu_test_context *c)
{
    fixture f;
    semu_sapporo_iom4 before;
    SEMU_TEST_ASSERT(c, setup(&f));
    wr(c, &f, R_SUBMOD, 0x10u);
    wr(c, &f, R_DEVICE_CFG, ADDR_GAUGE);
    wr(c, &f, R_FIFO_PUSH, 0u);
    wr(c, &f, R_COMMAND, 0x801u);
    SEMU_TEST_EQ_U64(c, 1u, f.iom->active_cmd);
    before = *f.iom;
    SEMU_TEST_ASSERT(c, semu_sapporo_iom4_write(f.iom, R_DEVICE_CFG,
        4u, 0x50u, &f.error) != SEMU_OK);
    SEMU_TEST_ASSERT(c, memcmp(&before, f.iom, sizeof(before)) == 0);
    destroy(&f);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_haptic_autotune_reset),
        SEMU_TEST_CASE(test_haptic_wave_chunking),
        SEMU_TEST_CASE(test_haptic_atomic_refusals),
        SEMU_TEST_CASE(test_haptic_endpoint_switch_refuses)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
