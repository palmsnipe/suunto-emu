#include "../../src/devices/sapporo_nema_gpu.h"
#include "../../src/devices/ulsan_als.h"
#include "../../src/devices/ulsan_crown.h"
#include "../../src/devices/ulsan_iom2.h"
#include "../../src/devices/ulsan_motion.h"
#include "../../src/compat/ulsan_244_gps.h"
#include "test.h"

#include <string.h>

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

static void test_race_s_als_and_crown(semu_test_context *context)
{
    semu_ulsan_crown crown;
    semu_error error;
    uint8_t bytes[2] = {0x64u, 0u};
    uint8_t out[2] = {0xffu, 0xffu};
    semu_serial_transaction tx = {0x48u, 0u, bytes, 1u, out, 2u};

    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_ulsan_als_transfer(NULL, &tx, &error));
    SEMU_TEST_EQ_U64(context, 0x80u, out[0]);
    SEMU_TEST_EQ_U64(context, 0u, out[1]);
    bytes[0] = 0x66u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, semu_ulsan_als_transfer(NULL, &tx, &error));
    SEMU_TEST_EQ_U64(context, 0x80u, out[0]);

    semu_ulsan_crown_reset(&crown);
    tx.address = 0x75u;
    tx.rx_size = 1u;
    bytes[0] = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_ulsan_crown_transfer(&crown, &tx, &error));
    SEMU_TEST_EQ_U64(context, 0x31u, out[0]);
    tx.tx_size = 2u;
    tx.rx_size = 0u;
    bytes[0] = 0x09u;
    bytes[1] = 0x5au;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_ulsan_crown_transfer(&crown, &tx, &error));
    bytes[1] = 0xffu;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE, semu_ulsan_crown_transfer(&crown, &tx, &error));
    tx.tx_size = 1u;
    tx.rx_size = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, semu_ulsan_crown_transfer(&crown, &tx, &error));
    SEMU_TEST_EQ_U64(context, 0x5au, out[0]);
}

static void test_race_s_iom1_configuration(semu_test_context *context)
{
    semu_error error;
    semu_bus *bus = semu_bus_create(&error);
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    semu_apollo4_dma *dma;
    semu_apollo4_iom *iom;
    uint32_t value;

    SEMU_TEST_ASSERT(context, bus != NULL && scheduler != NULL);
    dma = semu_apollo4_dma_create(bus, scheduler, &error);
    SEMU_TEST_ASSERT(context, dma != NULL);
    iom = semu_apollo4_iom_create(bus, SEMU_APOLLO4_IOM1_BASE, SEMU_APOLLO4_IOM1_IRQ,
        NULL, NULL, semu_apollo4_dma_execute, dma, scheduler, &error);
    SEMU_TEST_ASSERT(context, iom != NULL);
    semu_apollo4_iom_select_race_s(iom);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_iom_write(iom, 0x118u, 4u, 0x301u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_iom_read(iom, 0x118u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x301u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, semu_apollo4_iom_write(iom, 0x120u, 4u, 0x201u, &error));
    semu_apollo4_iom_destroy(iom);
    semu_apollo4_dma_destroy(dma);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
}

static void test_race_s_ohr_startup(semu_test_context *context)
{
    const semu_layer_descriptor *desc = &semu_ulsan_244_gps_layer;
    const uint32_t target = 0x10000000u;
    semu_error error;
    semu_bus *bus = semu_bus_create(&error);
    semu_scheduler *scheduler = semu_scheduler_create(&error);
    semu_apollo4_gpio *gpio;
    semu_ulsan_iom2 iom = {0};
    semu_layer_state layer = {0};
    uint8_t request[59];
    uint32_t value;

    SEMU_TEST_ASSERT(context, bus != NULL && scheduler != NULL);
    gpio = semu_apollo4_gpio_create(bus, NULL, NULL, &error);
    SEMU_TEST_ASSERT(context, gpio != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_ram(bus, "ohr2-sram", target, 64u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_layer_enable_checked(&layer, desc,
        desc->profile_id, desc->component_hashes, desc->component_hash_count, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_244_iom2_map(bus, &iom, scheduler, NULL, NULL, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, iom_write(bus, 0x2c4u, 0x10u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_ulsan_244_iom2_enable_startup(&iom, gpio, &layer, NULL, &error));
    memset(request, 0xff, sizeof(request));
    memcpy(request, (const uint8_t[]){0u, 0x10u, 0u, 0u, 0u, 1u}, 6u);
    memcpy(request + 55u, (const uint8_t[]){0x5du, 0x08u, 0xd0u, 0x67u}, 4u);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_load(bus, target, request, sizeof(request), &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x2c4u, 0x10u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x220u, target));
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x21cu, 59u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x218u, 0x103u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x120u, 0x3b01u));
    SEMU_TEST_EQ_U64(context, 1u, semu_apollo4_gpio_get_input(gpio, 62u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(bus, SEMU_APOLLO4_IOM2_BASE + 0x204u, 4u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x401u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x218u, 0x103u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x120u, 0x3b01u));
    SEMU_TEST_EQ_U64(context, 2u, layer.hits);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, target + 6u, 1u, 0u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, iom_write(bus, 0x218u, 0x103u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, iom_write(bus, 0x120u, 0x3b01u));
    SEMU_TEST_EQ_U64(context, 2u, layer.hits);
    semu_ulsan_244_iom2_reset(&iom);
    SEMU_TEST_EQ_U64(context, 0u, semu_apollo4_gpio_get_input(gpio, 62u));
    semu_ulsan_244_iom2_destroy(&iom);
    semu_apollo4_gpio_destroy(gpio);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
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
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_write(bus, base + 0x218u, 4u, 0x101u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(bus, base + 0x120u, 4u, 0xbb000112u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_read(bus, target, 1u, &value, &error));
    SEMU_TEST_EQ_U64(context, 0x10u, value);
    semu_apollo4_iom_destroy(motion.controller);
    semu_apollo4_dma_destroy(dma);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_race_s_gpu_registers),
        SEMU_TEST_CASE(test_race_s_als_and_crown),
        SEMU_TEST_CASE(test_race_s_iom1_configuration),
        SEMU_TEST_CASE(test_race_s_ohr_startup),
        SEMU_TEST_CASE(test_race_s_iom2_transactions),
        SEMU_TEST_CASE(test_race_s_motion_dma)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
