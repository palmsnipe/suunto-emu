#include "../../src/display/nema_framing.h"
#include "../../src/display/nema_backend.h"
#include "test.h"

#include "semu/bus.h"
#include "semu/types.h"

#include <string.h>

#define SRAM_BASE 0x10000000u
#define SRAM_SIZE 0x00100000u
#define RING_BASE (SRAM_BASE + 0x10000u)
#define RING_WORDS 64u
#define LIST_BASE (SRAM_BASE + 0x20000u)

typedef struct {
    nema_record records[NEMA_MAX_RECORDS];
    size_t count;
    size_t children;
    uint32_t child_addrs[32u];
    uint32_t child_entries[32u];
} capture_ctx;

static void on_record(void *context, const nema_record *record)
{
    capture_ctx *ctx = (capture_ctx *)context;
    if (ctx->count < NEMA_MAX_RECORDS) {
        ctx->records[ctx->count] = *record;
    }
    ++ctx->count;
}

static void on_child(void *context, uint32_t addr, uint32_t entries)
{
    capture_ctx *ctx = (capture_ctx *)context;
    if (ctx->children < 32u) {
        ctx->child_addrs[ctx->children] = addr;
        ctx->child_entries[ctx->children] = entries;
    }
    ++ctx->children;
}

static void load_words(semu_bus *bus, uint32_t addr, const uint32_t *words,
                        size_t count, semu_error *error)
{
    semu_bus_load(bus, addr, (const uint8_t *)words, count * 4u, error);
}

static semu_bus *make_bus(semu_error *error)
{
    semu_bus *bus = semu_bus_create(error);
    if (bus != NULL && semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, error) != SEMU_OK) {
        semu_bus_destroy(bus); return NULL;
    }
    return bus;
}

static void load_child_ring(semu_bus *bus, uint32_t address, uint32_t entries,
    semu_error *error)
{
    const uint32_t ring[] = {NEMA_REG_CMDADDR, address,
        NEMA_CL_PUSH | NEMA_REG_CMDSIZE, entries};
    load_words(bus, RING_BASE, ring, 4u, error);
}

static void test_bootstrap(semu_test_context *context)
{
    semu_error error; semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t ring[RING_WORDS] = {0};

    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    ring[0u] = NEMA_REG_INTERRUPT;
    ring[1u] = 0u;
    ring[2u] = NEMA_CL_NOP;
    ring[3u] = 0u;
    load_words(bus, RING_BASE, ring, 4u, &error);
    /* The old synthetic zero padding is an unmatched TEX0_BASE write. */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, nema_framing_parse(bus,
        RING_BASE, RING_WORDS, 0u, 4u, on_child, &cap, on_record, &cap, &error));
    SEMU_TEST_EQ_U64(context, 0u, cap.count);
    ring[3u] = NEMA_CL_NOP;
    load_words(bus, RING_BASE, ring, 4u, &error);
    st = nema_framing_parse(bus, RING_BASE, RING_WORDS, 0u, 4u,
                             on_child, &cap, on_record, &cap, &error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, st);
    SEMU_TEST_EQ_U64(context, 0u, cap.count);
    SEMU_TEST_EQ_U64(context, 0u, cap.children);
    semu_bus_destroy(bus);
}

static void test_complete_child(semu_test_context *context)
{
    semu_error error; semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t list[8u] = {0};

    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);

    load_child_ring(bus, LIST_BASE, 8u, &error);

    list[0u] = 0x00000110u; list[1u] = 0x00000000u;
    list[2u] = 0x00000114u; list[3u] = 0x007800F0u;
    list[4u] = 0x00000100u; list[5u] = 5u;
    list[6u] = 0xFF0000F0u; list[7u] = LIST_BASE;
    load_words(bus, LIST_BASE, list, 8u, &error);

    st = nema_framing_parse(bus, RING_BASE, RING_WORDS, 0u, 4u,
                             on_child, &cap, on_record, &cap, &error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, st);
    SEMU_TEST_EQ_U64(context, 1u, cap.children);
    SEMU_TEST_EQ_U64(context, 8u, cap.child_entries[0u]);
    SEMU_TEST_EQ_U64(context, 4u, cap.count);
    SEMU_TEST_EQ_U64(context, 0x110u, cap.records[0u].reg_offset);
    SEMU_TEST_EQ_U64(context, 0x114u, cap.records[1u].reg_offset);
    SEMU_TEST_EQ_U64(context, 0x100u, cap.records[2u].reg_offset);
    SEMU_TEST_EQ_U64(context, 0xFFu, cap.records[3u].prefix);
    semu_bus_destroy(bus);
}

static void test_multiple_children(semu_test_context *context)
{
    semu_error error; semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t ring[RING_WORDS] = {0};
    uint32_t list1[4u] = {0};
    uint32_t list2[4u] = {0};

    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);

    ring[0u] = NEMA_REG_CMDADDR;
    ring[1u] = LIST_BASE;
    ring[2u] = NEMA_CL_PUSH | NEMA_REG_CMDSIZE;
    ring[3u] = 4u;
    ring[4u] = NEMA_REG_CMDADDR;
    ring[5u] = LIST_BASE + 0x100u;
    ring[6u] = NEMA_CL_PUSH | NEMA_REG_CMDSIZE;
    ring[7u] = 4u;
    load_words(bus, RING_BASE, ring, 8u, &error);

    list1[0u] = 0x00000110u; list1[1u] = 1u;
    list1[2u] = 0x00000114u; list1[3u] = 2u;
    load_words(bus, LIST_BASE, list1, 4u, &error);

    list2[0u] = 0x00000100u; list2[1u] = 5u;
    list2[2u] = 0x00000118u; list2[3u] = 0x90000000u;
    load_words(bus, LIST_BASE + 0x100u, list2, 4u, &error);

    st = nema_framing_parse(bus, RING_BASE, RING_WORDS, 0u, 8u,
                             on_child, &cap, on_record, &cap, &error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, st);
    SEMU_TEST_EQ_U64(context, 2u, cap.children);
    SEMU_TEST_EQ_U64(context, 4u, cap.count);
    semu_bus_destroy(bus);
}

static void test_nop_and_holdcmd(semu_test_context *context)
{
    semu_error error; semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t ring[RING_WORDS] = {0};

    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);

    ring[0u] = NEMA_CL_NOP;
    ring[1u] = NEMA_HOLDCMD | 0x10u;
    ring[2u] = NEMA_HOLDCMD | NEMA_REG_CMDADDR;
    ring[3u] = RING_BASE;
    ring[4u] = NEMA_HOLDCMD | NEMA_REG_CMDSIZE;
    ring[5u] = RING_WORDS * 4u;
    load_words(bus, RING_BASE, ring, 6u, &error);
    /* Held graphics writes need a value; arbitrary held words are not NOPs. */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, nema_framing_parse(bus,
        RING_BASE, RING_WORDS, 0u, 6u, on_child, &cap, on_record, &cap, &error));
    SEMU_TEST_EQ_U64(context, 0u, cap.children);
    ring[1u] = NEMA_CL_NOP;
    load_words(bus, RING_BASE, ring, 6u, &error);
    st = nema_framing_parse(bus, RING_BASE, RING_WORDS, 0u, 6u,
                             on_child, &cap, on_record, &cap, &error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, st);
    SEMU_TEST_EQ_U64(context, 0u, cap.count);
    SEMU_TEST_EQ_U64(context, 0u, cap.children);
    semu_bus_destroy(bus);
}

static void test_completion_marker_payload_is_not_a_command(
    semu_test_context *context)
{
    semu_error error; semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t ring[RING_WORDS] = {0};

    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);

    /* 0xf0 is both the observed list ID and CMDADDR. */
    ring[0u] = NEMA_REG_CLID;
    ring[1u] = NEMA_REG_CMDADDR;
    ring[2u] = NEMA_REG_INTERRUPT;
    ring[3u] = 1u;
    load_words(bus, RING_BASE, ring, 4u, &error);
    st = nema_framing_parse(bus, RING_BASE, RING_WORDS, 0u, 4u,
                             on_child, &cap, on_record, &cap, &error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, st);
    SEMU_TEST_EQ_U64(context, 0u, cap.children);
    SEMU_TEST_EQ_U64(context, 0u, cap.count);
    semu_bus_destroy(bus);
}

static void test_truncated_child(semu_test_context *context)
{
    semu_error error; semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t ring[RING_WORDS] = {0};

    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);

    ring[0u] = NEMA_REG_CMDADDR;
    ring[1u] = LIST_BASE;
    ring[2u] = NEMA_CL_PUSH | NEMA_REG_CMDSIZE;
    load_words(bus, RING_BASE, ring, 3u, &error);

    st = nema_framing_parse(bus, RING_BASE, RING_WORDS, 0u, 3u,
                             on_child, &cap, on_record, &cap, &error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, st);
    SEMU_TEST_EQ_U64(context, 0u, cap.count);
    semu_bus_destroy(bus);
}

static void test_bad_prefix(semu_test_context *context)
{
    semu_error error; semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t list[4u] = {0};

    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);

    load_child_ring(bus, LIST_BASE, 4u, &error);

    list[0u] = 0x80000110u; list[1u] = 0u;
    list[2u] = 0x00000114u; list[3u] = 0u;
    load_words(bus, LIST_BASE, list, 4u, &error);

    st = nema_framing_parse(bus, RING_BASE, RING_WORDS, 0u, 4u,
                             on_child, &cap, on_record, &cap, &error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, st);
    SEMU_TEST_EQ_U64(context, 0u, cap.count);
    semu_bus_destroy(bus);
}

static void test_bad_alignment(semu_test_context *context)
{
    semu_error error; semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;

    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);

    load_child_ring(bus, LIST_BASE + 1u, 4u, &error);

    st = nema_framing_parse(bus, RING_BASE, RING_WORDS, 0u, 4u,
                             on_child, &cap, on_record, &cap, &error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, st);
    semu_bus_destroy(bus);
}

static void test_bad_size(semu_test_context *context)
{
    semu_error error; semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;

    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);

    load_child_ring(bus, LIST_BASE, 0u, &error);

    st = nema_framing_parse(bus, RING_BASE, RING_WORDS, 0u, 4u,
                             on_child, &cap, on_record, &cap, &error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, st);
    semu_bus_destroy(bus);
}

static void test_repeat_output(semu_test_context *context)
{
    semu_error error; semu_bus *bus;
    capture_ctx cap1 = {0};
    capture_ctx cap2 = {0};
    semu_status st;
    uint32_t list[8u] = {0};
    size_t i;

    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);

    load_child_ring(bus, LIST_BASE, 8u, &error);

    list[0u] = 0x00000110u; list[1u] = 0u;
    list[2u] = 0x00000114u; list[3u] = 0u;
    list[4u] = 0x00000100u; list[5u] = 5u;
    list[6u] = 0x00000118u; list[7u] = 0u;
    load_words(bus, LIST_BASE, list, 8u, &error);

    st = nema_framing_parse(bus, RING_BASE, RING_WORDS, 0u, 4u,
                             on_child, &cap1, on_record, &cap1, &error);
    SEMU_TEST_ASSERT(context, st == SEMU_OK);
    st = nema_framing_parse(bus, RING_BASE, RING_WORDS, 0u, 4u,
                             on_child, &cap2, on_record, &cap2, &error);
    SEMU_TEST_ASSERT(context, st == SEMU_OK);
    SEMU_TEST_EQ_U64(context, cap1.count, cap2.count);
    for (i = 0u; i < cap1.count; ++i) {
        SEMU_TEST_EQ_U64(context, cap1.records[i].reg_offset,
                         cap2.records[i].reg_offset);
        SEMU_TEST_EQ_U64(context, cap1.records[i].value,
                         cap2.records[i].value);
        SEMU_TEST_EQ_U64(context, cap1.records[i].prefix,
                         cap2.records[i].prefix);
    }
    semu_bus_destroy(bus);
}

static semu_status counted_read(void *context, uint32_t offset,
    unsigned width, uint32_t *value, semu_error *error)
{
    unsigned *reads = context;
    (void)width; ++*reads;
    *value = (offset & 4u) != 0u ? 0u : NEMA_REG_CLIPMIN;
    semu_error_clear(error); return SEMU_OK;
}
static void command_memory_case(semu_test_context *context, unsigned mode)
{
    const uint32_t device = 0x40000000u;
    const uint8_t rom[] = {0x10u, 1u, 0u, 0u, 0x78u, 0x56u, 0x34u, 0x12u};
    const semu_bus_device_ops ops = {counted_read, NULL, NULL};
    uint32_t ring[] = {NEMA_REG_CMDADDR, device, NEMA_CL_PUSH | NEMA_REG_CMDSIZE, 2u};
    semu_error error; semu_bus *bus = make_bus(&error);
    semu_nema_backend *backend = semu_nema_backend_create(&error);
    capture_ctx cap = {0}; unsigned reads = 0u; semu_status status = SEMU_OK;
    SEMU_TEST_ASSERT(context, bus != NULL && backend != NULL);
    if (mode == 4u) {
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_rom(bus, "commands", device,
            rom, sizeof(rom), &error));
    } else {
        if (mode == 3u) {
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_ram(bus, "register", device, 4u, &error));
            SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, device, 4u, NEMA_REG_CLIPMIN, &error));
        }
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_device(bus, "read-side-effect",
            device + (mode == 3u ? 4u : 0u), 16u, &ops, &reads, &error));
    }
    load_words(bus, RING_BASE, ring, 4u, &error);
    if (mode < 2u || mode == 4u)
        status = nema_framing_parse(bus, mode == 0u ? device : RING_BASE,
            mode == 0u ? 4u : RING_WORDS, 0u, mode == 0u ? 2u : 4u,
            on_child, &cap, on_record, &cap, &error);
    else (void)semu_nema_backend_submit(backend, bus, device, 2u, 0u, NULL, NULL, &error);
    SEMU_TEST_EQ_U64(context, 0u, reads);
    SEMU_TEST_EQ_U64(context, mode == 4u ? SEMU_OK : SEMU_ERR_RANGE, error.code);
    SEMU_TEST_EQ_U64(context, mode == 4u ? 1u : 0u, cap.count);
    SEMU_TEST_EQ_U64(context, mode == 4u ? 1u : 0u, cap.children);
    SEMU_TEST_EQ_U64(context, 0u, semu_nema_backend_frame(backend)->generation);
    if (mode == 4u) {
        SEMU_TEST_EQ_U64(context, SEMU_OK, status);
        SEMU_TEST_EQ_U64(context, 0x12345678u, cap.records[0].value);
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
            semu_nema_backend_submit(backend, bus, device, 2u, 0u, NULL, NULL, &error));
    }
    /* Correct the same ring/list input to mapped RAM and retry. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_load(bus, LIST_BASE, rom, sizeof(rom), &error));
    ring[1] = LIST_BASE; load_words(bus, RING_BASE, ring, 4u, &error);
    SEMU_TEST_EQ_U64(context, SEMU_OK, nema_framing_parse(bus, RING_BASE, RING_WORDS,
        0u, 4u, on_child, &cap, on_record, &cap, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_nema_backend_submit(
        backend, bus, LIST_BASE, 2u, 0u, NULL, NULL, &error));
    SEMU_TEST_EQ_U64(context, 0u, reads);
    semu_nema_backend_destroy(backend); semu_bus_destroy(bus);
}
static void test_mmio_ring_has_no_reads(semu_test_context *c) { command_memory_case(c, 0u); }
static void test_mmio_child_has_no_reads(semu_test_context *c) { command_memory_case(c, 1u); }
static void test_mmio_backend_has_no_reads(semu_test_context *c) { command_memory_case(c, 2u); }
static void test_mmio_value_has_no_reads(semu_test_context *c) { command_memory_case(c, 3u); }
static void test_rom_commands_are_little_endian(semu_test_context *c) { command_memory_case(c, 4u); }

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_bootstrap),
        SEMU_TEST_CASE(test_complete_child),
        SEMU_TEST_CASE(test_multiple_children),
        SEMU_TEST_CASE(test_nop_and_holdcmd),
        SEMU_TEST_CASE(test_completion_marker_payload_is_not_a_command),
        SEMU_TEST_CASE(test_truncated_child),
        SEMU_TEST_CASE(test_bad_prefix),
        SEMU_TEST_CASE(test_bad_alignment),
        SEMU_TEST_CASE(test_bad_size),
        SEMU_TEST_CASE(test_repeat_output),
        SEMU_TEST_CASE(test_mmio_ring_has_no_reads),
        SEMU_TEST_CASE(test_mmio_child_has_no_reads),
        SEMU_TEST_CASE(test_mmio_backend_has_no_reads),
        SEMU_TEST_CASE(test_mmio_value_has_no_reads),
        SEMU_TEST_CASE(test_rom_commands_are_little_endian)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
