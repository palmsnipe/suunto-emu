#include "test.h"

#include <string.h>

#include "../../src/boards/sapporo_profile.h"
#include "../../src/boards/sapporo_wiring.h"
#include "semu/types.h"

static void test_valid_profile(semu_test_context *context)
{
    const semu_sapporo_profile *p = semu_sapporo_profile_get();
    semu_error error;

    SEMU_TEST_ASSERT(context, p != NULL);
    SEMU_TEST_ASSERT(context, strcmp(p->id, "sapporo-2.22.60") == 0);
    SEMU_TEST_ASSERT(context, strcmp(p->board, "sapporo") == 0);
    SEMU_TEST_ASSERT(context,
                     strcmp(p->product, "Sapporo") == 0);
    SEMU_TEST_ASSERT(context,
                     strcmp(p->version, "2.22.60.3383-P") == 0);
    SEMU_TEST_EQ_U64(context, 0x00040000u, p->vector_table);
    SEMU_TEST_EQ_U64(context, 0xE0u, p->nvic_priority_mask);
    SEMU_TEST_EQ_U64(context, 240u, p->display_width);
    SEMU_TEST_EQ_U64(context, 240u, p->display_height);

    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_profile_validate(&error));
}

static void test_components(semu_test_context *context)
{
    const semu_sapporo_component *c;
    size_t count;

    (void)semu_sapporo_profile_components(&count);
    SEMU_TEST_EQ_U64(context, 3u, count);

    c = semu_sapporo_profile_find_component(
        SEMU_SAPPORO_COMPONENT_RESIDENT);
    SEMU_TEST_ASSERT(context, c != NULL);
    SEMU_TEST_EQ_U64(context, 0x00019000u, c->load_address);
    SEMU_TEST_EQ_U64(context, 71504u, c->size);
    SEMU_TEST_ASSERT(context,
                     strcmp(c->sha256,
                            "a409b088a061c2fe61689c8f39a79b2c35ed0059cd66987646e0195e47a2f522")
                     == 0);

    c = semu_sapporo_profile_find_component(
        SEMU_SAPPORO_COMPONENT_APPLICATION);
    SEMU_TEST_ASSERT(context, c != NULL);
    SEMU_TEST_EQ_U64(context, 0x00040000u, c->load_address);
    SEMU_TEST_EQ_U64(context, 1493234u, c->size);

    c = semu_sapporo_profile_find_component(
        SEMU_SAPPORO_COMPONENT_RESOURCES);
    SEMU_TEST_ASSERT(context, c != NULL);
    SEMU_TEST_EQ_U64(context, 0x14000000u, c->load_address);
    SEMU_TEST_EQ_U64(context, 16519168u, c->size);
}

static void test_regions(semu_test_context *context)
{
    const semu_sapporo_region *r;
    size_t count;

    (void)semu_sapporo_profile_regions(&count);
    SEMU_TEST_EQ_U64(context, 3u, count);

    r = semu_sapporo_profile_find_region(SEMU_SAPPORO_REGION_MRAM);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, 0x00000000u, r->base);
    SEMU_TEST_EQ_U64(context, 0x00200000u, r->size);

    r = semu_sapporo_profile_find_region(SEMU_SAPPORO_REGION_SRAM);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, 0x10000000u, r->base);
    SEMU_TEST_EQ_U64(context, 0x00180000u, r->size);

    r = semu_sapporo_profile_find_region(
        SEMU_SAPPORO_REGION_EXTERNAL_FLASH);
    SEMU_TEST_ASSERT(context, r != NULL);
    SEMU_TEST_EQ_U64(context, 0x14000000u, r->base);
    SEMU_TEST_EQ_U64(context, 0x02000000u, r->size);
}

static void test_wiring_verified(semu_test_context *context)
{
    const semu_sapporo_wiring *w;
    size_t count;
    semu_error error;

    (void)semu_sapporo_wiring_get(&count);
    SEMU_TEST_EQ_U64(context, 12u, count);

    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_wiring_validate(&error));

    w = semu_sapporo_wiring_find(SEMU_SAPPORO_WIRE_BUTTON_UPPER);
    SEMU_TEST_ASSERT(context, w != NULL);
    SEMU_TEST_EQ_U64(context, 57u, w->controller_instance);
    SEMU_TEST_ASSERT(context, w->active_low);
    SEMU_TEST_ASSERT(context,
                     strcmp(w->evidence_id, "E-SAP-BUTTONS-001") == 0);

    w = semu_sapporo_wiring_find(SEMU_SAPPORO_WIRE_ACCELEROMETER);
    SEMU_TEST_ASSERT(context, w != NULL);
    SEMU_TEST_EQ_U64(context, 0u, w->controller_instance);
    SEMU_TEST_EQ_U64(context, 0x40050000u, w->address);
    SEMU_TEST_EQ_U64(context, 6u, w->irq);

    w = semu_sapporo_wiring_find(SEMU_SAPPORO_WIRE_GPS);
    SEMU_TEST_ASSERT(context, w != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_SAPPORO_BUS_UART, w->bus);
    SEMU_TEST_EQ_U64(context, 0x4001d000u, w->address);
}

static void test_wiring_unverified_omitted(semu_test_context *context)
{
    /* External flash and panel have missing evidence — their roles are
     * absent from the wiring enum entirely, so there is nothing to
     * look up.  Verify that unknown roles beyond the enum return NULL. */
    const semu_sapporo_wiring *w = semu_sapporo_wiring_find(
        (semu_sapporo_wire_role)999);
    SEMU_TEST_ASSERT(context, w == NULL);
}

static void test_wiring_no_duplicate_roles(semu_test_context *context)
{
    size_t i;
    size_t j;
    size_t count;
    const semu_sapporo_wiring *table = semu_sapporo_wiring_get(&count);

    for (i = 0u; i < count; ++i) {
        for (j = i + 1u; j < count; ++j) {
            SEMU_TEST_ASSERT(context,
                             table[i].role != table[j].role);
            SEMU_TEST_ASSERT(context,
                             table[i].evidence_id != NULL);
        }
    }
}

static void test_profile_hash_pinning(semu_test_context *context)
{
    /* Component hashes must be 64 hex chars (SHA-256). */
    size_t i;
    size_t count;
    const semu_sapporo_component *comps =
        semu_sapporo_profile_components(&count);

    for (i = 0u; i < count; ++i) {
        size_t len = strlen(comps[i].sha256);
        SEMU_TEST_EQ_U64(context, 64u, len);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_valid_profile),
        SEMU_TEST_CASE(test_components),
        SEMU_TEST_CASE(test_regions),
        SEMU_TEST_CASE(test_wiring_verified),
        SEMU_TEST_CASE(test_wiring_unverified_omitted),
        SEMU_TEST_CASE(test_wiring_no_duplicate_roles),
        SEMU_TEST_CASE(test_profile_hash_pinning)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
