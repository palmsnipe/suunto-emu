#include "../../src/display/nema_tsc6a.h"
#include "test.h"

#include "semu/bus.h"

#include <string.h>

#define SRAM_BASE UINT32_C(0x10000000)
#define FSTRIDE_TSC UINT32_C(0x170005a0)
#define FSTRIDE_RGB UINT32_C(0x040001e0)

static semu_bus *make_bus(semu_error *error)
{
    semu_bus *bus = semu_bus_create(error);
    if (bus != NULL && semu_bus_map_ram(bus, "sram", SRAM_BASE,
                                        UINT32_C(0x20000), error) != SEMU_OK) {
        semu_bus_destroy(bus);
        return NULL;
    }
    return bus;
}

static nema_draw_snapshot target_triangle(void)
{
    nema_draw_snapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.target_base = SRAM_BASE;
    snapshot.target_format = NEMA_FMT_TSC6A;
    snapshot.target_stride = FSTRIDE_TSC & 0xffffu;
    snapshot.target_width = NEMA_TSC6A_WIDTH;
    snapshot.target_height = NEMA_TSC6A_HEIGHT;
    snapshot.clip_max_x = NEMA_TSC6A_WIDTH;
    snapshot.clip_max_y = NEMA_TSC6A_HEIGHT;
    snapshot.draw_cmd = NEMA_DRAW_TRI_AA;
    snapshot.draw_color = UINT32_C(0xff55ff00);
    snapshot.matmult = UINT32_C(0x90000000);
    snapshot.codeptr = UINT32_C(0x941eb400);
    snapshot.point1_x = UINT32_C(2u << 16);
    snapshot.point2_y = UINT32_C(2u << 16);
    return snapshot;
}

static nema_draw_snapshot resolve_state(void)
{
    nema_draw_snapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.src_base = SRAM_BASE + 0x1000u;
    snapshot.src_format = NEMA_FMT_TSC6A;
    snapshot.src_sampling = 1u;
    snapshot.src_stride = FSTRIDE_TSC & 0xffffu;
    snapshot.src_width = NEMA_TSC6A_WIDTH;
    snapshot.src_height = NEMA_TSC6A_HEIGHT;
    snapshot.target_base = SRAM_BASE + 0x2000u;
    snapshot.target_format = NEMA_FMT_RGB565;
    snapshot.target_stride = FSTRIDE_RGB & 0xffffu;
    snapshot.target_width = 240u;
    snapshot.target_height = 240u;
    snapshot.clip_max_x = 1u;
    snapshot.clip_max_y = 1u;
    snapshot.draw_cmd = NEMA_DRAW_TSC6A_RESOLVE;
    snapshot.draw_color = UINT32_C(0xff55ff00);
    snapshot.tex_color = UINT32_C(0xff55ff00);
    snapshot.codeptr = UINT32_C(0x941e8000);
    snapshot.imem_addr = 0u;
    snapshot.imem_datah = UINT32_C(0x004e0002);
    snapshot.imem_datal = UINT32_C(0x804b1286);
    snapshot.matrix_present = 1u;
    snapshot.mm00 = UINT32_C(0x3f800000);
    snapshot.mm11 = UINT32_C(0x3f800000);
    return snapshot;
}

static void test_target_triangle_and_resolve(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot target;
    nema_draw_snapshot resolve;
    uint8_t panel[480u];

    semu_error_clear(&error);
    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_create(&surface, &error));
    target = target_triangle();
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_draw_target(surface, bus, &target, &error));
    memset(panel, 0, sizeof(panel));
    resolve = resolve_state();
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_resolve(surface, &resolve, panel, 480u,
                                        &error));
    SEMU_TEST_ASSERT(context, panel[0] != 0u || panel[1] != 0u);
    nema_tsc6a_destroy(surface);
    semu_bus_destroy(bus);
}

static void test_refuses_unobserved_state(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot target;
    nema_draw_snapshot resolve;
    uint8_t panel[480u];

    semu_error_clear(&error);
    bus = make_bus(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_create(&surface, &error));
    target = target_triangle();
    target.codeptr = UINT32_C(0x12345678);
    SEMU_TEST_ASSERT(context,
                     nema_tsc6a_draw_target(surface, bus, &target, &error) !=
                     SEMU_OK);
    resolve = resolve_state();
    resolve.codeptr = UINT32_C(0x12345678);
    SEMU_TEST_ASSERT(context,
                     nema_tsc6a_resolve(surface, &resolve, panel, 480u,
                                        &error) != SEMU_OK);
    nema_tsc6a_destroy(surface);
    semu_bus_destroy(bus);
}

static void test_compressed_asset_refusal_diagnostic(semu_test_context *context)
{
    semu_error error;
    nema_tsc6a *surface = NULL;
    nema_draw_snapshot resolve = resolve_state();
    uint8_t panel[480u * 240u];
    uint8_t before[sizeof(panel)];

    /* E-EMU-SAP235-MAIN-TSC6A-001 descriptor, one near-miss away from the
     * ticket-793 acceptance tuple (stride 181 instead of the captured
     * 180), resolved with bus == NULL.  Ticket 793 accepted the exact
     * compressed tuple in nema_tsc6a_resolve_mask; a null bus is
     * fail-closed, and any compressed-shaped state that is not the exact
     * tuple keeps the specialized diagnostic.  The exact-tuple acceptance
     * and the zero-write auxiliary refusal are pinned in
     * test_nema_tsc6a_expand.c with a real bus.
     * No compressed firmware bytes or private frame pixels are needed. */
    resolve.src_base = UINT32_C(0x100a490c);
    resolve.src_present = 1u;
    resolve.src_width = 60u;
    resolve.src_height = 60u;
    resolve.src_stride = 181u;
    resolve.target_base = UINT32_C(0x10121d40);
    resolve.clip_min_y = 81u;
    resolve.clip_max_x = 240u;
    resolve.clip_max_y = 162u;
    resolve.draw_cmd = NEMA_DRAW_QUAD;
    resolve.draw_color = UINT32_C(0xff555555);
    resolve.tex_color = UINT32_C(0xffffffff);
    resolve.point0_x = resolve.point3_x = 171u << 16u;
    resolve.point1_x = resolve.point2_x = 231u << 16u;
    resolve.point0_y = resolve.point1_y = 90u << 16u;
    resolve.point2_y = resolve.point3_y = 150u << 16u;
    resolve.mm02 = UINT32_C(0xc32b0001);
    resolve.mm12 = UINT32_C(0xc2b40000);
    memset(panel, 0xa5, sizeof(panel));
    memcpy(before, panel, sizeof(panel));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     nema_tsc6a_create(&surface, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_tsc6a_resolve_mask(surface, NULL, &resolve, panel, 480u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, error.code);
    SEMU_TEST_ASSERT(context, strcmp(error.text,
        "nema_tsc6a: compressed source 60x60 stride 181 is unsupported; "
        "only the 480x480 semantic shadow is modeled") == 0);
    SEMU_TEST_ASSERT(context, memcmp(before, panel, sizeof(panel)) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_tsc6a_resolve_mask(surface, NULL, &resolve, panel, 480u, NULL));
    SEMU_TEST_ASSERT(context, memcmp(before, panel, sizeof(panel)) == 0);

    /* A supported shadow descriptor still resolves; its invalid shader
     * retains the existing generic state diagnostic. */
    resolve.src_width = NEMA_TSC6A_WIDTH;
    resolve.src_height = NEMA_TSC6A_HEIGHT;
    resolve.src_stride = FSTRIDE_TSC & 0xffffu;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        nema_tsc6a_resolve_mask(surface, NULL, &resolve, panel, 480u, &error));
    SEMU_TEST_ASSERT(context, memcmp(before, panel, sizeof(panel)) == 0);
    resolve.codeptr = UINT32_C(0x12345678);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        nema_tsc6a_resolve_mask(surface, NULL, &resolve, panel, 480u, &error));
    SEMU_TEST_ASSERT(context, strcmp(error.text,
        "nema_tsc6a: unsupported mask resolve state") == 0);
    SEMU_TEST_ASSERT(context, memcmp(before, panel, sizeof(panel)) == 0);
    nema_tsc6a_destroy(surface);
}

int main(void)
{
    const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_target_triangle_and_resolve),
        SEMU_TEST_CASE(test_refuses_unobserved_state),
        SEMU_TEST_CASE(test_compressed_asset_refusal_diagnostic),
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
