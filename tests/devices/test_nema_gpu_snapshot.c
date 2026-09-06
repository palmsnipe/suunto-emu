#include "test.h"

#include "../../src/core/scheduler_internal.h"
#include "../../src/devices/sapporo_nema_gpu.h"

#include <stdint.h>

#define SRAM_BASE 0x10000000u
#define SRAM_SIZE 0x00100000u
#define CMD_BASE  (SRAM_BASE + 0x10000u)

#define NEMA_REG_CMDADDR     0x0f0u
#define NEMA_REG_CMDSIZE     0x0f4u
#define NEMA_REG_STATUS      0x0fcu
#define NEMA_REG_CMDRINGSTOP 0x0ecu
#define NEMA_REG_CLID        0x148u
#define NEMA_REG_INTERRUPT   0x0f8u

typedef struct snapshot_fixture {
    semu_error error;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_nema_gpu *gpu;
    int irq_level;
} snapshot_fixture;

static semu_transaction_result accept_submission(
    void *context, semu_bus *bus, const semu_display_list *lists,
    size_t count, uint64_t virtual_time_ns,
    semu_frame_callback frame_callback, void *frame_context,
    semu_error *error)
{
    (void)context;
    (void)bus;
    (void)lists;
    (void)count;
    (void)virtual_time_ns;
    (void)frame_callback;
    (void)frame_context;
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static void finish_submission(void *context) { (void)context; }
static const semu_display_backend_ops accept_ops = {
    accept_submission, finish_submission, finish_submission
};

static void irq_sink(void *context, unsigned irq, int level)
{
    snapshot_fixture *fixture = (snapshot_fixture *)context;
    if (fixture != NULL && irq == NEMA_GPU_IRQ) {
        fixture->irq_level = level;
    }
}

static int fixture_init(snapshot_fixture *fixture)
{
    semu_error_clear(&fixture->error);
    fixture->bus = semu_bus_create(&fixture->error);
    fixture->scheduler = semu_scheduler_create(&fixture->error);
    if (fixture->bus == NULL || fixture->scheduler == NULL ||
        semu_bus_map_ram(fixture->bus, "sram", SRAM_BASE, SRAM_SIZE,
                         &fixture->error) != SEMU_OK) {
        return 0;
    }
    fixture->gpu = semu_nema_gpu_create(
        fixture->bus, &accept_ops, NULL, NULL, NULL,
        irq_sink, fixture, fixture->scheduler, &fixture->error);
    if (fixture->gpu == NULL ||
        semu_nema_gpu_attach(fixture->gpu, &fixture->error) != SEMU_OK) {
        return 0;
    }
    fixture->irq_level = -1;
    return 1;
}

static void fixture_destroy(snapshot_fixture *fixture)
{
    semu_nema_gpu_destroy(fixture->gpu);
    semu_scheduler_destroy(fixture->scheduler);
    semu_bus_destroy(fixture->bus);
}

static void configure_ring(snapshot_fixture *fixture)
{
    semu_bus_write(fixture->bus, NEMA_GPU_BASE + NEMA_REG_CMDADDR,
                   4u, CMD_BASE, &fixture->error);
    semu_bus_write(fixture->bus, NEMA_GPU_BASE + NEMA_REG_CMDSIZE,
                   4u, 64u * 4u, &fixture->error);
    semu_bus_write(fixture->bus, NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP,
                   4u, CMD_BASE | 0x6u, &fixture->error);
    semu_bus_write(fixture->bus, NEMA_GPU_BASE + NEMA_REG_STATUS,
                   4u, 0u, &fixture->error);
}

static void test_pending_completion_rebinds_callbacks(
    semu_test_context *context)
{
    static const uint8_t marker[] = {
        0x48u, 0x01u, 0x00u, 0x00u,
        0x07u, 0x00u, 0x00u, 0x00u,
        0xf8u, 0x00u, 0x00u, 0x00u,
        0x01u, 0x00u, 0x00u, 0x00u
    };
    snapshot_fixture source = {0};
    snapshot_fixture target = {0};
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    semu_scheduled_event_state state;
    const semu_scheduled_event_state *saved;
    semu_event_callback callback = NULL;
    void *event_context = NULL;
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&source));
    configure_ring(&source);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_load(source.bus, CMD_BASE, marker,
                                   sizeof(marker), &source.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(source.bus,
                                    NEMA_GPU_BASE + NEMA_REG_CMDRINGSTOP,
                                    4u, CMD_BASE + sizeof(marker),
                                    &source.error));
    SEMU_TEST_EQ_U64(context, 1u,
                     semu_scheduler_event_count(source.scheduler));
    saved = semu_scheduler_event_get(source.scheduler, 0u);
    SEMU_TEST_ASSERT(context, saved != NULL);
    state = *saved;

    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_nema_gpu_snapshot_write(source.gpu, &writer,
                                                  &source.error));

    SEMU_TEST_ASSERT(context, fixture_init(&target));
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_nema_gpu_snapshot_read(target.gpu, &reader,
                                                 &target.error));
    SEMU_TEST_ASSERT(context, semu_snapshot_reader_done(&reader));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_nema_gpu_snapshot_resolve_event(
                         target.gpu, state.subject, &callback, &event_context,
                         &target.error));
    SEMU_TEST_ASSERT(context, callback != NULL && event_context != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_restore_begin(
                         target.scheduler, 0u, state.sequence + 1u,
                         state.id + 1u, &target.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_restore_event(target.scheduler, &state,
                                                  callback, event_context,
                                                  &target.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(target.scheduler, state.due_ns,
                                            &target.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(target.bus, NEMA_GPU_BASE + NEMA_REG_CLID,
                                   4u, &value, &target.error));
    SEMU_TEST_EQ_U64(context, 7u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(target.bus,
                                   NEMA_GPU_BASE + NEMA_REG_INTERRUPT,
                                   4u, &value, &target.error));
    SEMU_TEST_EQ_U64(context, 1u, value);
    SEMU_TEST_EQ_U64(context, 1, target.irq_level);

    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

static void test_initialized_snapshot_requires_command_ring(
    semu_test_context *context)
{
    snapshot_fixture source = {0};
    snapshot_fixture target = {0};
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;
    size_t index;

    SEMU_TEST_ASSERT(context, fixture_init(&source));
    SEMU_TEST_ASSERT(context, fixture_init(&target));
    configure_ring(&source);
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_nema_gpu_snapshot_write(source.gpu, &writer,
                                                  &source.error));
    for (index = 0u; index < sizeof(uint32_t); ++index) {
        writer.data[NEMA_REG_CMDADDR + index] = 0u;
        writer.data[NEMA_REG_CMDSIZE + index] = 0u;
    }
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_nema_gpu_snapshot_read(target.gpu, &reader,
                                                 &target.error));

    semu_snapshot_writer_destroy(&writer);
    fixture_destroy(&target);
    fixture_destroy(&source);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_pending_completion_rebinds_callbacks),
        SEMU_TEST_CASE(test_initialized_snapshot_requires_command_ring)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
