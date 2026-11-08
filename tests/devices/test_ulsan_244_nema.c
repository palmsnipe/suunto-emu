#include "../../src/display/nema_backend.h"
#include "test.h"

#define LIST 0x10001000u
#define SOURCE 0x10002000u
#define TARGET 0x10060000u
#define FRAME_BYTES (466u * 466u * 2u)

typedef struct {
    unsigned count, width, height, stride;
    uint16_t first_pixel;
} capture;

typedef struct {
    semu_bus *bus;
    uint32_t address;
    semu_status status;
} list_writer;

static void frame_callback(void *context, const semu_frame *frame)
{
    capture *state = context;
    ++state->count;
    state->width = frame->width;
    state->height = frame->height;
    state->stride = frame->stride;
    state->first_pixel = (uint16_t)((uint16_t)frame->pixels[0] |
        (uint16_t)frame->pixels[1] << 8u);
}

static void emit(list_writer *w, uint32_t reg, uint32_t value)
{
    if (w->status != SEMU_OK) return;
    w->status = semu_bus_write(w->bus, w->address, 4u, reg, NULL);
    if (w->status == SEMU_OK)
        w->status = semu_bus_write(w->bus, w->address + 4u, 4u, value, NULL);
    w->address += 8u;
}

static void begin_draw(list_writer *w, uint32_t size, uint32_t y0, uint32_t y1)
{
    emit(w, NEMA_REG_TEX0_BASE, TARGET);
    emit(w, NEMA_REG_TEX0_FSTRIDE, 0x040003a4u);
    emit(w, NEMA_REG_TEX0_RESXY, size);
    emit(w, NEMA_REG_CLIPMIN, y0 << 16u);
    emit(w, NEMA_REG_CLIPMAX, y1 << 16u | 466u);
    emit(w, NEMA_REG_IMEM_ADDR, 0u);
    emit(w, NEMA_REG_IMEM_DATAH, 0x004e0002u);
    emit(w, NEMA_REG_IMEM_DATAL, 0x804b1286u);
}

static void points(list_writer *w, uint32_t width, uint32_t y0, uint32_t y1)
{
    emit(w, NEMA_REG_POINT0_X, 0u);
    emit(w, NEMA_REG_POINT0_Y, y0 << 16u);
    emit(w, NEMA_REG_POINT1_X, width << 16u);
    emit(w, NEMA_REG_POINT1_Y, y0 << 16u);
    emit(w, NEMA_REG_POINT2_X, width << 16u);
    emit(w, NEMA_REG_POINT2_Y, y1 << 16u);
    emit(w, NEMA_REG_POINT3_X, 0u);
    emit(w, NEMA_REG_POINT3_Y, y1 << 16u);
}

static void test_race_s_bands_and_mask(semu_test_context *context)
{
    static const uint32_t boundaries[] = {0u, 156u, 312u, 466u};
    semu_error error;
    semu_bus *bus = semu_bus_create(&error);
    semu_nema_backend *backend = semu_nema_backend_create_race_s(&error);
    capture frames = {0};
    list_writer list = {bus, LIST, SEMU_OK};
    uint32_t value;
    size_t i;

    SEMU_TEST_ASSERT(context, bus != NULL && backend != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_ram(bus, "native-lists", LIST, 1024u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_ram(bus, "native-source", SOURCE, 4u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_ram(bus, "native-target", TARGET, FRAME_BYTES, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, TARGET, 2u, 0xffffu, &error));
    for (i = 0u; i < 3u; ++i) {
        list.address = LIST;
        begin_draw(&list, 0x01d201d2u, boundaries[i], boundaries[i + 1u]);
        emit(&list, NEMA_REG_MATMULT, 0x90000000u);
        emit(&list, NEMA_REG_CODEPTR, 0x941eb400u);
        emit(&list, NEMA_REG_DRAW_COLOR, 0xff000000u);
        points(&list, 466u, boundaries[i], boundaries[i + 1u]);
        emit(&list, NEMA_REG_DRAW_CMD, 4u);
        SEMU_TEST_EQ_U64(context, SEMU_OK, list.status);
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
            semu_nema_backend_submit(backend, bus, LIST, (list.address - LIST) / 4u,
                                     0u, frame_callback, &frames, &error));
        SEMU_TEST_EQ_U64(context, i == 2u ? 1u : 0u, frames.count);
    }
    SEMU_TEST_EQ_U64(context, 466u, frames.width);
    SEMU_TEST_EQ_U64(context, 466u, frames.height);
    SEMU_TEST_EQ_U64(context, 932u, frames.stride);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(bus, TARGET, 2u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, SOURCE, 1u, 3u, &error));
    list.address = LIST;
    begin_draw(&list, 0x01d201d2u, 0u, 1u);
    emit(&list, NEMA_REG_TEX1_BASE, SOURCE);
    emit(&list, NEMA_REG_TEX1_FSTRIDE, 0x28010001u);
    emit(&list, NEMA_REG_TEX1_RESXY, 0x00010001u);
    emit(&list, NEMA_REG_TEX_COLOR, 0xffffffffu);
    emit(&list, NEMA_REG_MATMULT, 0u);
    emit(&list, NEMA_REG_CODEPTR, 0x941d8000u);
    emit(&list, NEMA_REG_MM00, 0x3f800000u);
    emit(&list, NEMA_REG_MM01, 0u);
    emit(&list, NEMA_REG_MM02, 0u);
    emit(&list, NEMA_REG_MM10, 0u);
    emit(&list, NEMA_REG_MM11, 0x3f800000u);
    emit(&list, NEMA_REG_MM12, 0u);
    points(&list, 1u, 0u, 1u);
    emit(&list, NEMA_REG_DRAW_CMD, 5u);
    SEMU_TEST_EQ_U64(context, SEMU_OK, list.status);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        semu_nema_backend_submit(backend, bus, LIST, (list.address - LIST) / 4u,
                                 0u, frame_callback, &frames, &error));
    SEMU_TEST_EQ_U64(context, 2u, frames.count);
    SEMU_TEST_EQ_U64(context, 0xffffu, frames.first_pixel);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(bus, TARGET, 2u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0xffffu, value);

    list.address = LIST;
    emit(&list, NEMA_REG_TEX_COLOR, 0xff000000u);
    emit(&list, NEMA_REG_DRAW_CMD, 5u);
    emit(&list, NEMA_REG_TEX0_RESXY, 0x01d201d1u);
    emit(&list, NEMA_REG_DRAW_CMD, 5u);
    SEMU_TEST_EQ_U64(context, SEMU_OK, list.status);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        semu_nema_backend_submit(backend, bus, LIST, (list.address - LIST) / 4u,
                                 0u, frame_callback, &frames, &error));
    SEMU_TEST_EQ_U64(context, 2u, frames.count);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(bus, TARGET, 2u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0xffffu, value);
    semu_nema_backend_destroy(backend);
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {SEMU_TEST_CASE(test_race_s_bands_and_mask)};
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
