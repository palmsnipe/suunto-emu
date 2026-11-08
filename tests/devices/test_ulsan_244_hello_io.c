#include "../../src/devices/sapporo_nema_gpu.h"
#include "../../src/devices/ulsan_iom2.h"
#include "../../src/devices/ulsan_motion.h"
#include "test.h"

static void test_race_s_gpu_registers(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus = semu_bus_create(&error);
    semu_nema_gpu *gpu;
    uint32_t value;

    SEMU_TEST_ASSERT(context, bus != NULL);
    gpu = semu_nema_gpu_create(bus, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &error);
    SEMU_TEST_ASSERT(context, gpu != NULL);
    semu_nema_gpu_select_race_s(gpu);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_gpu_attach(gpu, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(bus, NEMA_GPU_BASE + 0x94u, 4u, UINT32_MAX, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_read(bus, NEMA_GPU_BASE + 0x1ecu, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x86362000u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        semu_bus_read(bus, NEMA_GPU_BASE + 0x94u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        semu_bus_write(bus, NEMA_GPU_BASE + 0xf4u, 4u, 0x404u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        semu_bus_write(bus, NEMA_GPU_BASE + 0x94u, 2u, 0xffffu, &error));
    semu_nema_gpu_destroy(gpu);
    semu_bus_destroy(bus);
}

static semu_status iom_write(semu_bus *bus, uint32_t offset, uint32_t value)
{
    return semu_bus_write(bus, SEMU_APOLLO4_IOM2_BASE + offset, 4u, value, NULL);
}

static void test_race_s_iom2_transactions(semu_test_context *context)
{
    const uint32_t target = 0x10034420u;
    semu_error error;
    semu_bus *bus = semu_bus_create(&error);
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    semu_ulsan_iom2 iom = {0};
    uint32_t value;

    SEMU_TEST_ASSERT(context, bus != NULL && scheduler != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_ram(bus, "iom-sram", target, 32u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_ulsan_244_iom2_map(bus, &iom, scheduler, NULL, NULL, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x2c4u, 0x49u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x220u, target));
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x21cu, 1u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x218u, 0x101u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x120u, 0x1c000112u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(bus, target, 1u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x41u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_read(bus, SEMU_APOLLO4_IOM2_BASE + 0x204u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x403u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x208u, UINT32_MAX));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, iom_write(bus, 0x2c4u, 0x50u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x2c4u, 0x48u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x218u, 0x101u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, iom_write(bus, 0x120u, 0x1c000112u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_read(bus, SEMU_APOLLO4_IOM2_BASE + 0x204u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(bus, target, 1u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x41u, value);
    semu_ulsan_244_iom2_destroy(&iom);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
}

static void test_race_s_motion_dma(semu_test_context *context)
{
    const uint32_t base = SEMU_APOLLO4_IOM0_BASE;
    const uint32_t target = 0x10034420u;
    semu_error error;
    semu_bus *bus = semu_bus_create(&error);
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    semu_apollo4_dma *dma;
    semu_ulsan_motion motion = {0};
    uint32_t value;

    SEMU_TEST_ASSERT(context, bus != NULL && scheduler != NULL);
    dma = semu_apollo4_dma_create(bus, scheduler, &error);
    SEMU_TEST_ASSERT(context, dma != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_map_ram(bus, "motion-sram", target, 32u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_244_motion_map(
        bus, &motion, dma, scheduler, NULL, NULL, &error));
    semu_ulsan_244_motion_reset(&motion);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, base + 0x220u, 4u, target, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, base + 0x21cu, 4u, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, base + 0x218u, 4u, 0x101u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(bus, base + 0x120u, 4u, 0x8f000112u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(bus, target, 1u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x6cu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, base + 0x218u, 4u, 0x101u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
        semu_bus_write(bus, base + 0x120u, 4u, 0xff000112u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(bus, target, 1u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x6cu, value);
    semu_apollo4_iom_destroy(motion.controller);
    semu_apollo4_dma_destroy(dma);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_race_s_gpu_registers),
        SEMU_TEST_CASE(test_race_s_iom2_transactions),
        SEMU_TEST_CASE(test_race_s_motion_dma)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
