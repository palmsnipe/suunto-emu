#include "../../src/display/nema_framing.h"
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

static uint32_t w(uint32_t v)
{
    return v;
}

static void test_bootstrap(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t ring[RING_WORDS] = {0};

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_ASSERT(context,
        semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, &error) == SEMU_OK);
    ring[0u] = w(NEMA_REG_INTERRUPT);
    ring[1u] = 0u;
    ring[2u] = w(NEMA_CL_NOP);
    ring[3u] = 0u;
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
    semu_error error;
    semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t ring[RING_WORDS] = {0};
    uint32_t list[8u] = {0};

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_ASSERT(context,
        semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, &error) == SEMU_OK);

    ring[0u] = w(NEMA_REG_CMDADDR);
    ring[1u] = LIST_BASE;
    ring[2u] = w(NEMA_CL_PUSH | NEMA_REG_CMDSIZE);
    ring[3u] = 8u;
    load_words(bus, RING_BASE, ring, 4u, &error);

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
    semu_error error;
    semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t ring[RING_WORDS] = {0};
    uint32_t list1[4u] = {0};
    uint32_t list2[4u] = {0};

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_ASSERT(context,
        semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, &error) == SEMU_OK);

    ring[0u] = w(NEMA_REG_CMDADDR);
    ring[1u] = LIST_BASE;
    ring[2u] = w(NEMA_CL_PUSH | NEMA_REG_CMDSIZE);
    ring[3u] = 4u;
    ring[4u] = w(NEMA_REG_CMDADDR);
    ring[5u] = LIST_BASE + 0x100u;
    ring[6u] = w(NEMA_CL_PUSH | NEMA_REG_CMDSIZE);
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
    semu_error error;
    semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t ring[RING_WORDS] = {0};

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_ASSERT(context,
        semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, &error) == SEMU_OK);

    ring[0u] = w(NEMA_CL_NOP);
    ring[1u] = w(NEMA_HOLDCMD | 0x10u);
    ring[2u] = w(NEMA_HOLDCMD | NEMA_REG_CMDADDR);
    ring[3u] = RING_BASE;
    ring[4u] = w(NEMA_HOLDCMD | NEMA_REG_CMDSIZE);
    ring[5u] = RING_WORDS * 4u;
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
    semu_error error;
    semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t ring[RING_WORDS] = {0};

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_ASSERT(context,
        semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, &error) == SEMU_OK);

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
    semu_error error;
    semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t ring[RING_WORDS] = {0};

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_ASSERT(context,
        semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, &error) == SEMU_OK);

    ring[0u] = w(NEMA_REG_CMDADDR);
    ring[1u] = LIST_BASE;
    ring[2u] = w(NEMA_CL_PUSH | NEMA_REG_CMDSIZE);
    load_words(bus, RING_BASE, ring, 3u, &error);

    st = nema_framing_parse(bus, RING_BASE, RING_WORDS, 0u, 3u,
                             on_child, &cap, on_record, &cap, &error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, st);
    SEMU_TEST_EQ_U64(context, 0u, cap.count);
    semu_bus_destroy(bus);
}

static void test_bad_prefix(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t ring[RING_WORDS] = {0};
    uint32_t list[4u] = {0};

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_ASSERT(context,
        semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, &error) == SEMU_OK);

    ring[0u] = w(NEMA_REG_CMDADDR);
    ring[1u] = LIST_BASE;
    ring[2u] = w(NEMA_CL_PUSH | NEMA_REG_CMDSIZE);
    ring[3u] = 4u;
    load_words(bus, RING_BASE, ring, 4u, &error);

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
    semu_error error;
    semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t ring[RING_WORDS] = {0};

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_ASSERT(context,
        semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, &error) == SEMU_OK);

    ring[0u] = w(NEMA_REG_CMDADDR);
    ring[1u] = LIST_BASE + 1u;
    ring[2u] = w(NEMA_CL_PUSH | NEMA_REG_CMDSIZE);
    ring[3u] = 4u;
    load_words(bus, RING_BASE, ring, 4u, &error);

    st = nema_framing_parse(bus, RING_BASE, RING_WORDS, 0u, 4u,
                             on_child, &cap, on_record, &cap, &error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, st);
    semu_bus_destroy(bus);
}

static void test_bad_size(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    capture_ctx cap = {0};
    semu_status st;
    uint32_t ring[RING_WORDS] = {0};

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_ASSERT(context,
        semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, &error) == SEMU_OK);

    ring[0u] = w(NEMA_REG_CMDADDR);
    ring[1u] = LIST_BASE;
    ring[2u] = w(NEMA_CL_PUSH | NEMA_REG_CMDSIZE);
    ring[3u] = 0u;
    load_words(bus, RING_BASE, ring, 4u, &error);

    st = nema_framing_parse(bus, RING_BASE, RING_WORDS, 0u, 4u,
                             on_child, &cap, on_record, &cap, &error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, st);
    semu_bus_destroy(bus);
}

static void test_repeat_output(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    capture_ctx cap1 = {0};
    capture_ctx cap2 = {0};
    semu_status st;
    uint32_t ring[RING_WORDS] = {0};
    uint32_t list[8u] = {0};
    size_t i;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_ASSERT(context,
        semu_bus_map_ram(bus, "sram", SRAM_BASE, SRAM_SIZE, &error) == SEMU_OK);

    ring[0u] = w(NEMA_REG_CMDADDR);
    ring[1u] = LIST_BASE;
    ring[2u] = w(NEMA_CL_PUSH | NEMA_REG_CMDSIZE);
    ring[3u] = 8u;
    load_words(bus, RING_BASE, ring, 4u, &error);

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
        SEMU_TEST_CASE(test_repeat_output)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
