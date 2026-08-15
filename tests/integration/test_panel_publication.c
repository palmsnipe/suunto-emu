#include "../../src/display/panel.h"
#include "test.h"

#include <string.h>

static void test_half_frame_not_published(semu_test_context *context)
{
    semu_error err;
    semu_panel *panel;
    uint8_t half[PANEL_WIDTH * 120u * 2u];
    uint32_t i;
    semu_error_clear(&err);
    panel = semu_panel_create(&err);
    SEMU_TEST_ASSERT(context, panel != NULL);

    for (i = 0u; i < sizeof(half); i += 2u) {
        half[i] = 0x1Fu;
        half[i + 1u] = 0x00u;
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_panel_commit_region(panel, 0u, 0u, PANEL_WIDTH, 120u,
                                   half, PANEL_WIDTH * 2u, &err));
    SEMU_TEST_EQ_U64(context, 0, semu_panel_is_complete(panel));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        semu_panel_publish(panel, NULL, NULL, &err));
    semu_panel_destroy(panel);
}

static void test_full_frame_refused(semu_test_context *context)
{
    semu_error err;
    semu_panel *panel;
    uint8_t full[PANEL_BYTES];
    uint32_t i;
    const semu_frame *frame;
    semu_error_clear(&err);
    panel = semu_panel_create(&err);
    SEMU_TEST_ASSERT(context, panel != NULL);

    for (i = 0u; i < sizeof(full); i += 2u) {
        full[i] = 0xE0u;
        full[i + 1u] = 0x07u;
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_panel_commit_region(panel, 0u, 0u, PANEL_WIDTH, PANEL_HEIGHT,
                                   full, PANEL_WIDTH * 2u, &err));
    SEMU_TEST_EQ_U64(context, 1, semu_panel_is_complete(panel));

    /* E-NEMA-PANEL-001 is missing: publication must refuse. */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        semu_panel_publish(panel, NULL, NULL, &err));
    frame = semu_panel_frame(panel);
    SEMU_TEST_ASSERT(context, frame != NULL);
    SEMU_TEST_EQ_U64(context, PANEL_BYTES, frame->size);
    SEMU_TEST_EQ_U64(context, 0u, frame->generation);
    semu_panel_destroy(panel);
}

static int callback_invoked;

static void test_callback(void *context, const semu_frame *frame)
{
    (void)context;
    (void)frame;
    callback_invoked = 1;
}

static void test_callback_not_invoked(semu_test_context *context)
{
    semu_error err;
    semu_panel *panel;
    uint8_t full[PANEL_BYTES];
    semu_error_clear(&err);
    panel = semu_panel_create(&err);
    SEMU_TEST_ASSERT(context, panel != NULL);

    memset(full, 0xFF, sizeof(full));
    semu_panel_commit_region(panel, 0u, 0u, PANEL_WIDTH, PANEL_HEIGHT,
                               full, PANEL_WIDTH * 2u, &err);
    callback_invoked = 0;
    semu_panel_publish(panel, test_callback, NULL, &err);
    SEMU_TEST_EQ_U64(context, 0, callback_invoked);
    semu_panel_destroy(panel);
}

static void test_region_order(semu_test_context *context)
{
    semu_error err;
    semu_panel *panel;
    uint8_t top[PANEL_WIDTH * 60u * 2u];
    uint8_t bot[PANEL_WIDTH * 180u * 2u];
    const semu_frame *frame;
    uint32_t i;
    semu_error_clear(&err);
    panel = semu_panel_create(&err);
    SEMU_TEST_ASSERT(context, panel != NULL);

    for (i = 0u; i < sizeof(top); i += 2u) {
        top[i] = 0x1Fu; top[i + 1u] = 0x00u;
    }
    for (i = 0u; i < sizeof(bot); i += 2u) {
        bot[i] = 0xE0u; bot[i + 1u] = 0x07u;
    }

    /* Commit in two regions: top then bottom */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_panel_commit_region(panel, 0u, 0u, PANEL_WIDTH, 60u,
                                   top, PANEL_WIDTH * 2u, &err));
    SEMU_TEST_EQ_U64(context, 0, semu_panel_is_complete(panel));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_panel_commit_region(panel, 0u, 60u, PANEL_WIDTH, 180u,
                                   bot, PANEL_WIDTH * 2u, &err));
    SEMU_TEST_EQ_U64(context, 1, semu_panel_is_complete(panel));

    frame = semu_panel_frame(panel);
    SEMU_TEST_ASSERT(context, frame != NULL);
    {
        uint16_t px_top = (uint16_t)(frame->pixels[0] |
                                       (frame->pixels[1] << 8));
        uint16_t px_bot = (uint16_t)(frame->pixels[60u * frame->stride] |
                                       (frame->pixels[60u * frame->stride + 1u] << 8));
        SEMU_TEST_EQ_U64(context, 0x001Fu, px_top);
        SEMU_TEST_EQ_U64(context, 0x07E0u, px_bot);
    }
    semu_panel_destroy(panel);
}

static void test_reset(semu_test_context *context)
{
    semu_error err;
    semu_panel *panel;
    uint8_t full[PANEL_BYTES];
    const semu_frame *frame;
    semu_error_clear(&err);
    panel = semu_panel_create(&err);
    SEMU_TEST_ASSERT(context, panel != NULL);

    memset(full, 0xFF, sizeof(full));
    semu_panel_commit_region(panel, 0u, 0u, PANEL_WIDTH, PANEL_HEIGHT,
                               full, PANEL_WIDTH * 2u, &err);
    SEMU_TEST_EQ_U64(context, 1, semu_panel_is_complete(panel));

    semu_panel_reset(panel);
    SEMU_TEST_EQ_U64(context, 0, semu_panel_is_complete(panel));
    frame = semu_panel_frame(panel);
    SEMU_TEST_EQ_U64(context, 0u, frame->generation);
    SEMU_TEST_EQ_U64(context, 0u, frame->pixels[0]);

    semu_panel_destroy(panel);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_half_frame_not_published),
        SEMU_TEST_CASE(test_full_frame_refused),
        SEMU_TEST_CASE(test_callback_not_invoked),
        SEMU_TEST_CASE(test_region_order),
        SEMU_TEST_CASE(test_reset)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
