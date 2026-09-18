/*
 * Sapporo-2.35.34 live IOM4 lane-mirror tests (E-SAP-0036 law module).
 *
 * Expected values are lane observations from the byte-identical probe
 * pairs iom4law{,2,3,4,5,6} (sha256 5a2f3974.., ea5feadd..), reconciled
 * against SapporoApollo4Iom4.cs and AmbiqApollo4_IOMaster.cs. Started-
 * lane probes ran behind the guest's own 0x104=0x1010/0x200=0x4e7d
 * driver setup, replayed here; fresh-module deviations are asserted
 * fresh and motivated per case. The irq sink records line edges.
 */

#include "test.h"

#include <stdint.h>

#include "semu/bus.h"
#include "../../src/devices/sapporo_iom4.h"

#define SRAM_BASE 0x10000000u
#define SRAM_SIZE 0x160000u
#define IOM4_IRQ  10u

typedef struct iom4_fixture {
    semu_error error;
    semu_bus *bus;
    semu_sapporo_iom4 *m;
    unsigned rises, falls, last_irq;
    int last_level;
} iom4_fixture;

static void irq_sink(void *context, unsigned irq, int level)
{
    iom4_fixture *f = (iom4_fixture *)context;
    if (level != 0) {
        f->rises++;
    } else {
        f->falls++;
    }
    f->last_irq = irq;
    f->last_level = level;
}

static int fixture_init(iom4_fixture *f)
{
    semu_error_clear(&f->error);
    f->rises = 0u;
    f->falls = 0u;
    f->last_irq = 0u;
    f->last_level = -1;
    f->bus = semu_bus_create(&f->error);
    if (f->bus == NULL) {
        return 0;
    }
    if (semu_bus_map_ram(f->bus, "sram", SRAM_BASE, SRAM_SIZE,
                         &f->error) != SEMU_OK) {
        return 0;
    }
    f->m = semu_sapporo_iom4_create(f->bus, irq_sink, f, &f->error);
    return f->m != NULL;
}

static void fixture_destroy(iom4_fixture *f)
{
    semu_sapporo_iom4_destroy(f->m);
    semu_bus_destroy(f->bus);
}

static int fixture_start(semu_test_context *c, iom4_fixture *f)
{
    if (fixture_init(f) == 0) {
        semu_test_fail(c, __FILE__, __LINE__, "fixture_init");
        return 0;
    }
    return 1;
}

static void wr(semu_test_context *c, iom4_fixture *f, uint32_t off,
               uint32_t value)
{
    SEMU_TEST_EQ_U64(c, SEMU_OK,
                     semu_sapporo_iom4_write(f->m, off, 4u, value,
                                             &f->error));
}

static void rd(semu_test_context *c, iom4_fixture *f, uint32_t off,
               uint32_t want)
{
    uint32_t value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(c, SEMU_OK,
                     semu_sapporo_iom4_read(f->m, off, 4u, &value,
                                            &f->error));
    SEMU_TEST_EQ_U64(c, want, value);
}

static void ram_w(semu_test_context *c, iom4_fixture *f, uint32_t addr,
                  uint32_t value)
{
    SEMU_TEST_EQ_U64(c, SEMU_OK,
                     semu_bus_write(f->bus, addr, 4u, value, &f->error));
}

static void ram_r(semu_test_context *c, iom4_fixture *f, uint32_t addr,
                  uint32_t want)
{
    uint32_t value = 0xdeadbeefu;
    SEMU_TEST_EQ_U64(c, SEMU_OK,
                     semu_bus_read(f->bus, addr, 4u, &value, &f->error));
    SEMU_TEST_EQ_U64(c, want, value);
}

/* Cold write of a command with both master interfaces disabled: the
 * delegated controller returns before anything and the wrapper records
 * only the command register itself (iom4law tail, after machine reset:
 * the reset cleared all DMA registers). */
static void test_cold_silent_command(semu_test_context *c)
{
    iom4_fixture f;
    if (fixture_start(c, &f) == 0) {
        return;
    }
    wr(c, &f, 0x120u, 0x38000212u);
    rd(c, &f, 0x120u, 0x38000212u);
    rd(c, &f, 0x204u, 0x00000000u);
    rd(c, &f, 0x12cu, 0x00000080u);
    rd(c, &f, 0x248u, 0x00000004u);
    rd(c, &f, 0x224u, 0x00000000u);
    rd(c, &f, 0x100u, 0x20002000u);
    SEMU_TEST_EQ_U64(c, 0u, (uint64_t)f.rises);
    SEMU_TEST_EQ_U64(c, 0u, (uint64_t)f.falls);
    fixture_destroy(&f);
}

/* The a5a5 write matrix of iom4law pair 1, the 0x104/0x118/0x11c
 * readbacks of pair 2 and the INTSET/INTCLR semantics. The lane INTSTAT
 * carried 0x6 only because the booted guest had popped an empty RX. */
static void test_lane_write_laws(semu_test_context *c)
{
    iom4_fixture f;
    if (fixture_start(c, &f) == 0) {
        return;
    }
    wr(c, &f, 0x210u, 0xa5a5a5a5u);
    rd(c, &f, 0x210u, 0x1u);
    wr(c, &f, 0x218u, 0xa5a5a5a5u);
    rd(c, &f, 0x218u, 0x101u);
    wr(c, &f, 0x21cu, 0xa5a5a5a5u);
    rd(c, &f, 0x21cu, 0x5a5u);
    wr(c, &f, 0x220u, 0xa5a5a5a5u);
    rd(c, &f, 0x220u, 0x25a5a5a5u);
    wr(c, &f, 0x2c4u, 0xa5a5a5a5u);
    rd(c, &f, 0x2c4u, 0x25u);
    wr(c, &f, 0x200u, 0xa5a5a5a5u);
    rd(c, &f, 0x200u, 0x25a5u);
    wr(c, &f, 0x2c0u, 0xa5a5a5a5u);
    rd(c, &f, 0x2c0u, 0xa521u);
    wr(c, &f, 0x2c0u, 0x103f270u);
    rd(c, &f, 0x2c0u, 0xf270u);
    wr(c, &f, 0x2c0u, 0xf270u);
    rd(c, &f, 0x2c0u, 0xf270u);
    wr(c, &f, 0x228u, 0xa5a5a5a5u);
    wr(c, &f, 0x22cu, 0xa5a5a5a5u);
    wr(c, &f, 0x234u, 0xa5a5a5a5u);
    wr(c, &f, 0x23cu, 0xa5a5a5a5u);
    wr(c, &f, 0x240u, 0xa5a5a5a5u);
    wr(c, &f, 0x244u, 0xa5a5a5a5u);
    rd(c, &f, 0x228u, 0u);
    rd(c, &f, 0x23cu, 0u);
    wr(c, &f, 0x280u, 0xa5a5a5a5u);
    rd(c, &f, 0x280u, 0x200000u);
    wr(c, &f, 0x104u, 0xa5a5a5a5u);
    rd(c, &f, 0x104u, 0x2525u);
    /* Empty TX below the recorded write threshold: threshold interrupt. */
    rd(c, &f, 0x204u, 0x2u);
    wr(c, &f, 0x118u, 0xa5a5a5a5u);
    rd(c, &f, 0x118u, 0xa5a50501u);
    wr(c, &f, 0x118u, 0x1d0e1301u);
    rd(c, &f, 0x118u, 0x1d0e1301u);
    wr(c, &f, 0x11cu, 0xe30u);
    rd(c, &f, 0x11cu, 0xe30u);
    wr(c, &f, 0x11cu, 0xe20u);
    rd(c, &f, 0x11cu, 0xe20u);
    wr(c, &f, 0x11cu, 0x10u);
    rd(c, &f, 0x11cu, 0xe30u);
    wr(c, &f, 0x11cu, 0xa5a5a5a5u);
    rd(c, &f, 0x11cu, 0xe21u);
    /* INTSTAT ignores writes; INTSET sets, INTCLR clears. */
    wr(c, &f, 0x204u, 0xa5a5a5a5u);
    rd(c, &f, 0x204u, 0x2u);
    wr(c, &f, 0x20cu, 0xa5a5a5a5u);
    rd(c, &f, 0x20cu, 0u);
    rd(c, &f, 0x204u, 0x25a7u);
    wr(c, &f, 0x208u, 0xffffffffu);
    rd(c, &f, 0x204u, 0u);
    rd(c, &f, 0x208u, 0xffff8000u);
    rd(c, &f, 0x110u, 0x2u);
    /* The INTSET words above intersect the enabled bits: one rising
     * edge, and one falling edge when the clear takes them away. */
    SEMU_TEST_EQ_U64(c, 1u, (uint64_t)f.rises);
    SEMU_TEST_EQ_U64(c, 1u, (uint64_t)f.falls);
    fixture_destroy(&f);
}

/* Gauge VCell read (iom4law4/5 started lanes): the guest's own
 * 0x104=0x1010 and 0x200=0x4e7d writes are replayed, then the recorded
 * transaction registers; every status word matches the probe. */
static void test_gauge_read_transaction(semu_test_context *c)
{
    iom4_fixture f;
    if (fixture_start(c, &f) == 0) {
        return;
    }
    wr(c, &f, 0x11cu, 0x10u);
    wr(c, &f, 0x104u, 0x1010u);
    wr(c, &f, 0x200u, 0x4e7du);
    wr(c, &f, 0x2c4u, 0x36u);
    wr(c, &f, 0x21cu, 0x2u);
    wr(c, &f, 0x220u, 0x10030000u);
    wr(c, &f, 0x218u, 0x101u);
    wr(c, &f, 0x210u, 0x2u);
    wr(c, &f, 0x120u, 0x9000212u);
    ram_r(c, &f, 0x10030000u, 0xc000u);
    rd(c, &f, 0x204u, 0x403u);
    rd(c, &f, 0x12cu, 0x80u);
    rd(c, &f, 0x218u, 0x100u);
    rd(c, &f, 0x224u, 0x2u);
    rd(c, &f, 0x214u, 0x4u);
    rd(c, &f, 0x248u, 0x4u);
    rd(c, &f, 0x210u, 0x2u);
    rd(c, &f, 0x100u, 0x20002000u);
    rd(c, &f, 0x120u, 0x9000212u);
    /* CommandComplete on the enabled line: exactly one rising edge. */
    SEMU_TEST_EQ_U64(c, 1u, (uint64_t)f.rises);
    SEMU_TEST_EQ_U64(c, 0u, (uint64_t)f.falls);
    SEMU_TEST_EQ_U64(c, IOM4_IRQ, (uint64_t)f.last_irq);
    /* Interrupt clear, then the same transaction again: the gauge byte
     * counters were reset by the finish, so VCell reads 0xC000 once
     * more. */
    wr(c, &f, 0x208u, 0xffffffffu);
    rd(c, &f, 0x204u, 0u);
    SEMU_TEST_EQ_U64(c, 1u, (uint64_t)f.falls);
    wr(c, &f, 0x21cu, 0x2u);
    wr(c, &f, 0x220u, 0x10030000u);
    wr(c, &f, 0x218u, 0x101u);
    wr(c, &f, 0x120u, 0x9000212u);
    ram_r(c, &f, 0x10030000u, 0xc000u);
    rd(c, &f, 0x204u, 0x403u);
    SEMU_TEST_EQ_U64(c, 2u, (uint64_t)f.rises);
    fixture_destroy(&f);
}

/* The not-yet-identified 0x28 endpoint reads deterministically zero
 * (iom4law4 section 2); with fresh zero thresholds INTSTAT shows 0x401,
 * not the lane's 0x403 behind the guest's 0x104=0x1010. */
static void test_observed_device_read(semu_test_context *c)
{
    iom4_fixture f;
    if (fixture_start(c, &f) == 0) {
        return;
    }
    wr(c, &f, 0x11cu, 0x10u);
    wr(c, &f, 0x200u, 0x4e7du);
    wr(c, &f, 0x2c4u, 0x28u);
    wr(c, &f, 0x21cu, 0x1u);
    wr(c, &f, 0x220u, 0x10030004u);
    wr(c, &f, 0x218u, 0x101u);
    wr(c, &f, 0x120u, 0x21000212u);
    ram_r(c, &f, 0x10030004u, 0u);
    rd(c, &f, 0x204u, 0x401u);
    wr(c, &f, 0x208u, 0xffffffffu);
    rd(c, &f, 0x204u, 0u);
    wr(c, &f, 0x21cu, 0x1u);
    wr(c, &f, 0x220u, 0x10030004u);
    wr(c, &f, 0x218u, 0x101u);
    wr(c, &f, 0x120u, 0x21000212u);
    rd(c, &f, 0x204u, 0x401u);
    /* Both transactions raised the line; the clear took it low again. */
    SEMU_TEST_EQ_U64(c, 2u, (uint64_t)f.rises);
    SEMU_TEST_EQ_U64(c, 1u, (uint64_t)f.falls);
    fixture_destroy(&f);
}

/* Memory-to-peripheral write of the recorded initialisation table
 * (iom4law4 section 3): the DMA streams count=min(3,2)=2 bytes from
 * the firmware window into the gauge pair at 0x09 and reads it back.
 * Fresh thresholds keep INTSTAT at 0x401 (lane 0x403 with 0x1010). */
static void test_m2p_write_readback(semu_test_context *c)
{
    iom4_fixture f;
    if (fixture_start(c, &f) == 0) {
        return;
    }
    ram_w(c, &f, 0x10030010u, 0x0000abcdu);
    wr(c, &f, 0x11cu, 0x10u);
    wr(c, &f, 0x200u, 0x4e7du);
    wr(c, &f, 0x2c4u, 0x36u);
    wr(c, &f, 0x21cu, 0x3u);
    wr(c, &f, 0x220u, 0x10030010u);
    wr(c, &f, 0x218u, 0x103u);
    wr(c, &f, 0x120u, 0x9000211u);
    rd(c, &f, 0x204u, 0x401u);
    rd(c, &f, 0x224u, 0x2u);
    rd(c, &f, 0x218u, 0x102u);
    wr(c, &f, 0x208u, 0xffffffffu);
    wr(c, &f, 0x21cu, 0x2u);
    wr(c, &f, 0x220u, 0x10030014u);
    wr(c, &f, 0x218u, 0x101u);
    wr(c, &f, 0x120u, 0x9000212u);
    ram_r(c, &f, 0x10030014u, 0xabcd);
    rd(c, &f, 0x204u, 0x401u);
    fixture_destroy(&f);
}

/* FIFO write then read: register 0x01 receives 0x0302 from the pushed
 * word 0x04030201 (iom4law5), the write command retires the
 * transaction, and the read-back command streams the value into SRAM.
 * The DMA engine stays disabled throughout (every completion clears its
 * enable bit), so INTSTAT records the command completion alone. */
static void test_fifo_write_readback(semu_test_context *c)
{
    iom4_fixture f;
    if (fixture_start(c, &f) == 0) {
        return;
    }
    wr(c, &f, 0x11cu, 0x10u);
    wr(c, &f, 0x200u, 0x4e7du);
    wr(c, &f, 0x2c4u, 0x36u);
    wr(c, &f, 0x10cu, 0x04030201u);
    wr(c, &f, 0x120u, 0x401u);
    rd(c, &f, 0x204u, 0x1u);
    rd(c, &f, 0x100u, 0x20002000u);
    wr(c, &f, 0x21cu, 0x2u);
    wr(c, &f, 0x220u, 0x10030018u);
    wr(c, &f, 0x218u, 0x101u);
    wr(c, &f, 0x120u, 0x1000212u);
    ram_r(c, &f, 0x10030018u, 0x302u);
    rd(c, &f, 0x204u, 0x401u);
    wr(c, &f, 0x208u, 0xffffffffu);
    SEMU_TEST_EQ_U64(c, 1u, (uint64_t)f.rises);
    SEMU_TEST_EQ_U64(c, 1u, (uint64_t)f.falls);
    fixture_destroy(&f);
}

/* Access ports index the eight physical words absolutely
 * (DirectGet/DirectSet): a push does not move the window and writes to
 * the ports bypass the ring logic entirely. */
static void test_direct_access_ports(semu_test_context *c)
{
    iom4_fixture f;
    if (fixture_start(c, &f) == 0) {
        return;
    }
    wr(c, &f, 0x10cu, 0xaabbccddu);
    /* The ring wrote its first word into physical slot 0; the direct
     * write to port 1 bypasses the counters. */
    wr(c, &f, 0x04u, 0x11223344u);
    rd(c, &f, 0x00u, 0xaabbccddu);
    rd(c, &f, 0x04u, 0x11223344u);
    rd(c, &f, 0x100u, 0x20001c04u); /* FIFO0SIZ=4, FIFO0REM=28 */
    wr(c, &f, 0x24u, 0x55aa55aau); /* RX slot 1 without the ring logic */
    rd(c, &f, 0x24u, 0x55aa55aau);
    fixture_destroy(&f);
}

/* Refusals and error paths, each a lane observation (iom4law6, probe
 * ea5feadd.., byte-identical x2): unknown offsets and non-4 widths fail
 * closed; an invalid DMA target records the error and never drains the
 * received word; an unregistered slave address and an empty-TX write
 * raise the illegal-command interrupt; the ninth push onto the TX ring
 * overflows; a busy controller drops a new command. */
static void test_refusals_and_errors(semu_test_context *c)
{
    iom4_fixture f;
    uint32_t value = 0x5a5a5a5au;
    unsigned i;
    if (fixture_start(c, &f) == 0) {
        return;
    }
    SEMU_TEST_EQ_U64(c, SEMU_ERR_UNSUPPORTED,
                     semu_sapporo_iom4_read(f.m, 0x204u, 2u, &value,
                                            &f.error));
    SEMU_TEST_EQ_U64(c, SEMU_ERR_UNSUPPORTED,
                     semu_sapporo_iom4_write(f.m, 0x200u, 2u, value,
                                             &f.error));
    SEMU_TEST_EQ_U64(c, SEMU_ERR_UNSUPPORTED,
                     semu_sapporo_iom4_read(f.m, 0x300u, 4u, &value,
                                            &f.error));
    /* The lane's opening writes: the I2C interface, its interrupts and
     * the FIFO thresholds, then clear the flags they just raised. */
    wr(c, &f, 0x11cu, 0x10u);
    wr(c, &f, 0x200u, 0x4e7du);
    wr(c, &f, 0x104u, 0xa5a5a5a5u);
    wr(c, &f, 0x208u, 0xffffffffu);
    /* Invalid target: the DMA records the error and stops before the
     * drain, so the received gauge word stays retained in the RX. */
    wr(c, &f, 0x2c4u, 0x36u);
    wr(c, &f, 0x21cu, 0x2u);
    wr(c, &f, 0x220u, 0x30000000u);
    wr(c, &f, 0x218u, 0x101u);
    wr(c, &f, 0x120u, 0x9000212u);
    rd(c, &f, 0x204u, 0x801u);
    rd(c, &f, 0x224u, 0x4u);
    rd(c, &f, 0x100u, 0x1c042000u);
    rd(c, &f, 0x12cu, 0x80u);
    wr(c, &f, 0x208u, 0xffffffffu);
    /* Unregistered 0x40: illegal command, error status, idle engine. */
    wr(c, &f, 0x218u, 0u);
    wr(c, &f, 0x2c4u, 0x40u);
    wr(c, &f, 0x120u, 0x9000212u);
    rd(c, &f, 0x204u, 0x40u);
    rd(c, &f, 0x12cu, 0x20u);
    rd(c, &f, 0x248u, 0x4u);
    wr(c, &f, 0x208u, 0xffffffffu);
    /* Write with an empty TX buffer is illegal. */
    wr(c, &f, 0x2c4u, 0x36u);
    wr(c, &f, 0x120u, 0x101u);
    rd(c, &f, 0x204u, 0x40u);
    wr(c, &f, 0x208u, 0xffffffffu);
    /* The ninth push onto the eight-word TX ring is discarded with the
     * write-overflow flag; the empty TX under the recorded threshold
     * keeps firing the threshold interrupt. */
    for (i = 0u; i < 9u; ++i) {
        wr(c, &f, 0x10cu, 0x100u + i);
    }
    rd(c, &f, 0x204u, 0xau);
    rd(c, &f, 0x100u, 0x1c040020u);
    rd(c, &f, 0x108u, 0xc000u);
    rd(c, &f, 0x108u, 0u);
    rd(c, &f, 0x204u, 0xeu);
    wr(c, &f, 0x208u, 0xffffffffu);
    /* Busy controller: the 36-byte read fills the eight-word RX ring,
     * waits with a 4-byte tail and raises nothing; the next command is
     * dropped while the raw command register keeps it. */
    wr(c, &f, 0x2c4u, 0x36u);
    wr(c, &f, 0x120u, 0x2402u);
    rd(c, &f, 0x12cu, 0x4c2u);
    rd(c, &f, 0x100u, 0x200020u);
    rd(c, &f, 0x204u, 0u);
    wr(c, &f, 0x120u, 0x101u);
    rd(c, &f, 0x12cu, 0x4c2u);
    rd(c, &f, 0x120u, 0x101u);
    fixture_destroy(&f);
}

/* Reset re-initialises the module: the DMA window and the FIFOs are
 * cleared, the interrupt line falls, and the gauge register file is
 * back at the deterministic fixture (a previously written 16-bit
 * register reads zero again). */
static void test_reset_restores(semu_test_context *c)
{
    iom4_fixture f;
    if (fixture_start(c, &f) == 0) {
        return;
    }
    wr(c, &f, 0x11cu, 0x10u);
    wr(c, &f, 0x200u, 0x4e7du);
    wr(c, &f, 0x2c4u, 0x36u);
    wr(c, &f, 0x10cu, 0x04030201u);
    wr(c, &f, 0x120u, 0x401u);
    wr(c, &f, 0x218u, 0x101u);
    wr(c, &f, 0x21cu, 0x2u);
    wr(c, &f, 0x220u, 0x1003001cu);
    wr(c, &f, 0x120u, 0x1000212u);
    ram_r(c, &f, 0x1003001cu, 0x302u);
    SEMU_TEST_ASSERT(c, f.rises >= 1u);
    semu_sapporo_iom4_reset(f.m);
    rd(c, &f, 0x204u, 0u);
    rd(c, &f, 0x224u, 0u);
    rd(c, &f, 0x218u, 0u);
    rd(c, &f, 0x100u, 0x20002000u);
    rd(c, &f, 0x11cu, 0xe20u);
    rd(c, &f, 0x12cu, 0x80u);
    rd(c, &f, 0x120u, 0u);
    SEMU_TEST_EQ_U64(c, 1u, (uint64_t)f.falls);
    /* The gauge fixture is restored: register 0x01 no longer holds the
     * 0x0302 written above, so the read-back streams zeroes. */
    wr(c, &f, 0x11cu, 0x10u);
    wr(c, &f, 0x218u, 0x101u);
    wr(c, &f, 0x21cu, 0x2u);
    wr(c, &f, 0x220u, 0x10030018u);
    wr(c, &f, 0x120u, 0x1000212u);
    ram_r(c, &f, 0x10030018u, 0u);
    fixture_destroy(&f);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_cold_silent_command),
        SEMU_TEST_CASE(test_lane_write_laws),
        SEMU_TEST_CASE(test_gauge_read_transaction),
        SEMU_TEST_CASE(test_observed_device_read),
        SEMU_TEST_CASE(test_m2p_write_readback),
        SEMU_TEST_CASE(test_fifo_write_readback),
        SEMU_TEST_CASE(test_direct_access_ports),
        SEMU_TEST_CASE(test_refusals_and_errors),
        SEMU_TEST_CASE(test_reset_restores)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
