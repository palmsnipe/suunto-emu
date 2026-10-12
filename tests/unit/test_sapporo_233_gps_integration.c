/* Ticket 710 instance-35 integration attachment / E-SAP-0071: the 2.33
 * GPS startup layer is registered, bound through the device factory,
 * dispatched at the machine boundary, and cleared on reset. Mirrors the
 * 2.35 GPS binding precedent (test_sapporo_235_gps.c). No firmware
 * bytes: the native tuple is synthetic memory shaped by the evidence. */
#include "test.h"
#include "semu/manifest.h"
#include "sapporo_233_gps.h"
#include "../src/devices/sapporo_devices_internal.h"
#include "../../src/boards/machine_internal.h"
#include <string.h>

static uint8_t received[20];
static unsigned rx_count;
static void receive(void *unused_context, uint8_t byte, uint64_t now)
{
    (void)unused_context; (void)now;
    if (rx_count < sizeof(received)) received[rx_count] = byte;
    ++rx_count;
}

static void test_sapporo_233_gps_registration(semu_test_context *context)
{
    semu_profile profile;
    semu_error error;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_profile_load("profiles/sapporo/2.33.16/profile.semu", &profile,
                          &error));
    /* Four staged-boot layers: production data, logical files, the
     * first-boot translations and the GPS startup fixture (instances
     * 33-36). */
    SEMU_TEST_EQ_U64(context, 4u, profile.layer_count);
    SEMU_TEST_ASSERT(context, strcmp(profile.layers[0],
        "sapporo-2.33-gps-startup") == 0);
    SEMU_TEST_ASSERT(context, strcmp(profile.layers[1],
        "sapporo-2.33-production-data") == 0);
    SEMU_TEST_ASSERT(context, strcmp(profile.layers[2],
        "sapporo-2.33-logical-files") == 0);
    SEMU_TEST_ASSERT(context, strcmp(profile.layers[3],
        "sapporo-2.33-firstboot") == 0);
}

static void test_sapporo_233_gps_bind_dispatch_reset(semu_test_context *context)
{
    semu_sapporo_devices *devices;
    semu_scheduler *scheduler;
    semu_bus *bus;
    semu_sapporo_cxd5610 *gps;
    semu_layer_state state;
    semu_logger logger;
    FILE *log;
    semu_error e;
    semu_cpu_state cpu;
    const char *hashes[3];
    unsigned i;
    log = tmpfile();
    scheduler = semu_scheduler_create(&e);
    bus = semu_bus_create(&e);
    gps = semu_sapporo_cxd5610_create(scheduler, NULL, NULL, receive, NULL,
                                      NULL, NULL, &e);
    SEMU_TEST_ASSERT(context, log && scheduler && bus && gps);
    (void)gps;
    semu_log_init(&logger, log, SEMU_LOG_WARNING);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_bus_map_ram(bus, "sram",
        0x10000000u, 0x180000u, &e));
    for (i = 0u; i < 3u; ++i) hashes[i] = semu_sapporo_233_gps_layer.
        component_hashes[i];
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_layer_enable_checked(&state,
        &semu_sapporo_233_gps_layer, "sapporo-2.33.16", hashes, 3u, &e));
    devices = semu_sapporo_devices_create(scheduler, NULL, &e);
    SEMU_TEST_ASSERT(context, devices != NULL);
    /* The dispatch queues transport RX, which requires a sink (the real
     * wiring is the UART bridge set in attach). */
    semu_sapporo_cxd5610_set_rx_sink(devices->gps, receive, NULL);
    /* The bind refuses before the profile selection enables the fixture. */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_sapporo_devices_bind_233_gps(devices, &state, &logger, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_devices_select_profile(
        devices, "sapporo-2.33.16", &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_devices_bind_233_gps(devices, &state, &logger, &e));
    /* A second owner with the same descriptor still refuses. */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_sapporo_devices_bind_233_gps(devices, &state, &logger, &e));
    /* The dispatch admits the pinned E-SAP-0071 arm and queues one hit. */
    memset(&cpu, 0, sizeof(cpu));
    cpu.r[15] = SEMU_SAPPORO_233_GPS_PC;
    cpu.r[0] = 2u;
    cpu.r[4] = 0x10039d40u;
    cpu.r[5] = 0x10039fb0u;
    cpu.r[6] = 0x1003a078u;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(bus, 0x10039f60u, 4u, 0x10048d40u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(bus, 0x10048d44u, 4u, 0x11d593u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_bus_write(bus, 0x10039fb2u, 2u, 0x0204u, &e));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_devices_apply_compat_hook(
        devices, bus, &cpu, &state, &logger, &e));
    SEMU_TEST_EQ_U64(context, 1u, state.hits);
    SEMU_TEST_EQ_U64(context, 1u, semu_scheduler_event_count(scheduler));
    /* The per-owner refusal propagation keeps the idle gate exact. */
    SEMU_TEST_EQ_U64(context, 1u, semu_sapporo_devices_compat_idle(devices,
        0x40000u, &e));
    SEMU_TEST_EQ_U64(context, 0u, semu_sapporo_devices_compat_idle(devices,
        SEMU_SAPPORO_233_GPS_PC, &e));
    /* Reset clears the owner, its refusal and the queued transport event. */
    semu_sapporo_devices_reset(devices);
    SEMU_TEST_EQ_U64(context, 0u, semu_scheduler_event_count(scheduler));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_sapporo_devices_bind_233_gps(devices, &state, &logger, &e));
    semu_sapporo_cxd5610_destroy(gps);
    semu_sapporo_devices_destroy(devices);
    semu_bus_destroy(bus);
    semu_scheduler_destroy(scheduler);
    (void)fclose(log);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_sapporo_233_gps_registration),
        SEMU_TEST_CASE(test_sapporo_233_gps_bind_dispatch_reset)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
