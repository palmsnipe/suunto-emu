#include "sapporo_222.h"

#include <string.h>

#define FIRMWARE_BASE 0x00040000u
#define CRC_TABLE_ADDRESS 0x00199ee8u
#define PRODUCTION_ADDRESS 0x14fff000u
#define RECORD_SIZE 256u
#define SECTOR_SIZE 4096u
#define CHECKSUM_OFFSET 252u

/* E-SAP-PROFILE-001 component hashes (resident, application, resources). */
static const char *const sapporo_hashes[] = {
    "a409b088a061c2fe61689c8f39a79b2c35ed0059cd66987646e0195e47a2f522",
    "c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc",
    "ec2a4b1c472844ac6ff9cc575cb9383c29a74302107af3619f9a08fdf6abcaf1"
};

/* Intervention table: each trigger has its own per-hit budget. */
static semu_layer_intervention sapporo_interventions[] = {
    { "production-data",
      "install synthetic ProductionData/ACCR/ACCC/MAGN/HLAT records",
      "E-SAP-COMPAT-PROD-001", 1u, 0u },
    { "gps-startup",
      "inject $PSS0000 response to @VER startup request",
      "E-SAP-COMPAT-GPS-001", 1u, 0u },
    { "gps-state-startup",
      "enter native GPS UART-open after successful startup service request",
      "E-SAP-COMPAT-GPS-002", 1u, 0u },
    { "gps-state-dispatch",
      "translate the observed initial GPS dispatcher state 4 to state 2",
      "E-SAP-COMPAT-GPS-003", 1u, 0u },
    { "gps-running-status",
      "arm the later @GSR to $PSS running-status exchange",
      "E-SAP-COMPAT-GPS-004", 1u, 0u },
    { "gps-awake-pulse",
      "replay the observed GPIO24 awake pulse after GPS state 10",
      "E-SAP-COMPAT-GPS-005", 11u, 0u },
    { "ohr-startup",
      "supply synthetic BSL-to-MAIN startup body responses",
      "E-SAP-COMPAT-OHR-001", 1u, 0u },
    { "resource-status",
      "translate the observed resource-wrapper 0xcc sentinel to success",
      "E-SAP-COMPAT-RESOURCE-001", 1u, 0u },
    { "diap-worker-wake",
      "replay the observed DIAP4 worker task handoff after the wait boundary",
      "E-SAP-COMPAT-DIAP-001", 1u, 0u },
    { "diap-worker-irq",
      "replay the observed DIAP pending byte and IRQ21 wake after the DIAP4 worker handoff",
      "E-SAP-COMPAT-DIAP-002", 1u, 0u }
};

const semu_layer_descriptor semu_sapporo_222_no_device_layer = {
    .id = "sapporo-2.22-no-device",
    .kind = SEMU_LAYER_SYNTHETIC_STATE,
    .profile_id = "sapporo-2.22.60",
    .evidence = "suunto-firmware/tools/build_production_data_fixture.py and "
                "docs/research/factory-calibration-records.md",
    .component_hashes = sapporo_hashes,
    .component_hash_count = sizeof(sapporo_hashes) / sizeof(sapporo_hashes[0]),
    .interventions = sapporo_interventions,
    .intervention_count = sizeof(sapporo_interventions) /
                           sizeof(sapporo_interventions[0]),
    /* Nine one-shot interventions plus eleven bounded awake pulses. */
    .maximum_hits = 20u
};

static uint32_t firmware_checksum(const uint8_t *data, size_t size,
                                  const uint32_t table[16])
{
    uint32_t value = 0u;
    size_t i;
    for (i = 0u; i < size; ++i) {
        uint32_t intermediate = table[(data[i] ^ value) & 0x0fu] ^ (value >> 4u);
        value = table[(intermediate ^ (data[i] >> 4u)) & 0x0fu] ^
                (intermediate >> 4u);
    }
    return value;
}

static void put_u16(uint8_t *data, size_t offset, uint16_t value)
{
    data[offset] = (uint8_t)value;
    data[offset + 1u] = (uint8_t)(value >> 8u);
}

static void put_u32(uint8_t *data, size_t offset, uint32_t value)
{
    data[offset] = (uint8_t)value;
    data[offset + 1u] = (uint8_t)(value >> 8u);
    data[offset + 2u] = (uint8_t)(value >> 16u);
    data[offset + 3u] = (uint8_t)(value >> 24u);
}

static void finish_record(uint8_t record[RECORD_SIZE],
                          const uint32_t table[16])
{
    put_u32(record, CHECKSUM_OFFSET,
            firmware_checksum(record, CHECKSUM_OFFSET, table));
}

static void build_production(uint8_t record[RECORD_SIZE],
                             const uint32_t table[16])
{
    memset(record, 0, RECORD_SIZE);
    memcpy(record, "ProductionData", 14u);
    put_u16(record, 14u, 1000u);
    memcpy(record + 20u, "EMU000", 6u);
    memcpy(record + 26u, "00000", 5u);
    record[31u] = (uint8_t)'E';
    memcpy(record + 32u, "EMU000001", 9u);
    memcpy(record + 41u, "SUUNTO-EMU-001", 14u);
    memcpy(record + 55u, "000000", 6u);
    record[61u] = 1u;
    finish_record(record, table);
}

static void build_calibration(uint8_t record[RECORD_SIZE], const char magic[4],
                              uint8_t page, uint16_t payload_size,
                              const uint8_t *payload,
                              const uint32_t table[16])
{
    memset(record, 0, RECORD_SIZE);
    memcpy(record, magic, 4u);
    put_u16(record, 4u, 1u);
    put_u16(record, 6u, payload_size);
    record[12u] = page;
    record[13u] = 1u;
    if (payload != NULL) {
        memcpy(record + 16u, payload, payload_size);
    }
    finish_record(record, table);
}

static int intervention_is_unused(const semu_layer_state *state,
                                   size_t intervention_index)
{
    if (state == NULL || state->descriptor == NULL ||
        intervention_index >= state->descriptor->intervention_count ||
        state->descriptor->interventions == NULL) {
        return 0;
    }
    return state->descriptor->interventions[intervention_index].hits == 0u;
}

semu_status semu_sapporo_222_install_no_device(
    semu_bus *bus, semu_layer_state *state, semu_logger *logger,
    semu_error *error)
{
    uint8_t sector[SECTOR_SIZE];
    uint8_t table_bytes[64];
    uint8_t hlat[112];
    uint32_t table[16];
    size_t i;
    semu_status status;
    if (bus == NULL || state == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid Sapporo fixture target");
        return SEMU_ERR_ARGUMENT;
    }
    status = semu_bus_copy_out(bus, CRC_TABLE_ADDRESS, table_bytes,
                               sizeof(table_bytes), error);
    if (status != SEMU_OK) {
        return status;
    }
    for (i = 0u; i < 16u; ++i) {
        table[i] = (uint32_t)table_bytes[i * 4u] |
                   (uint32_t)table_bytes[i * 4u + 1u] << 8u |
                   (uint32_t)table_bytes[i * 4u + 2u] << 16u |
                   (uint32_t)table_bytes[i * 4u + 3u] << 24u;
    }
    memset(sector, 0xff, sizeof(sector));
    build_production(sector, table);
    build_calibration(sector + RECORD_SIZE, "ACCR", 1u, 36u, NULL, table);
    build_calibration(sector + RECORD_SIZE * 2u, "ACCC", 2u, 60u, NULL, table);
    build_calibration(sector + RECORD_SIZE * 3u, "MAGN", 3u, 108u, NULL, table);
    memset(hlat, 0, sizeof(hlat));
    memcpy(hlat, "EMUHLAT00001", 12u);
    build_calibration(sector + RECORD_SIZE * 6u, "HLAT", 6u, 112u,
                      hlat, table);
    status = semu_bus_load(bus, PRODUCTION_ADDRESS, sector, sizeof(sector), error);
    if (status != SEMU_OK) {
        return status;
    }
    return semu_layer_intervention_hit(state, logger,
        SEMU_SAPPORO_222_IV_PRODUCTION, error);
}

semu_status semu_sapporo_222_apply_firmware_hook(
    semu_bus *bus, semu_cpu_state *cpu_state, semu_layer_state *state,
    semu_logger *logger, semu_error *error)
{
    if (bus == NULL || cpu_state == NULL || state == NULL || logger == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo firmware hook arguments are incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    if (cpu_state->r[15] == UINT32_C(0x001145be) &&
        cpu_state->r[0] == UINT32_C(0x000000cc) &&
        cpu_state->r[1] == UINT32_C(0x00000070) &&
        cpu_state->r[2] == UINT32_C(0x00001d00) &&
        cpu_state->r[5] == UINT32_C(0x10040e68)) {
        uint32_t session_slot = 0u;
        if (state->descriptor == NULL || !state->enabled) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "disabled layer was invoked");
            return SEMU_ERR_STATE;
        }
        if (semu_bus_read(bus, cpu_state->r[5] + 0x14u, 2u,
                          &session_slot, error) != SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        }
        if (session_slot != UINT32_C(0xffff)) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "Sapporo resource session slot is not empty");
            return SEMU_ERR_STATE;
        }
        if (semu_bus_validate_write(bus, cpu_state->r[5] + 0x14u, 2u,
                                    error) != SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        }
        if (semu_layer_intervention_hit(state, logger,
                SEMU_SAPPORO_222_IV_RESOURCE_STATUS, error) != SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_STATE;
        }
        if (semu_bus_write(bus, cpu_state->r[5] + 0x14u, 2u, 0u,
                           error) != SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        }
        cpu_state->r[0] = UINT32_C(0x000000c8);
        /* The recovered no-device run reaches the same successful status
         * boundary; native storage exposes a zero session slot, while this
         * OTA wrapper enters with the observed empty 0xffff slot. */
        return SEMU_OK;
    }
    if (cpu_state->r[15] == UINT32_C(0x0009aaec)) {
        uint32_t current = 0u;
        uint32_t bitmap = 0u;
        uint32_t isr_word = 0u;
        uint32_t app_state = 0u;
        if (semu_bus_read(bus, UINT32_C(0x10030370), 4u, &current,
                          error) != SEMU_OK ||
            semu_bus_read(bus, UINT32_C(0x1003039c), 4u, &bitmap,
                          error) != SEMU_OK ||
            semu_bus_read(bus, UINT32_C(0x10056608), 4u, &isr_word,
                          error) != SEMU_OK ||
            semu_bus_read(bus, UINT32_C(0x10058510), 1u, &app_state,
                          error) != SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        }
        if (cpu_state->r[0] == UINT32_C(0x10000000) &&
            cpu_state->r[1] == UINT32_C(0xe000ed04) &&
            current != UINT32_C(0x10033600) && bitmap == 1u &&
            isr_word == UINT32_C(0x00010101) && app_state == 3u &&
            intervention_is_unused(state,
                SEMU_SAPPORO_222_IV_DIAP_WORKER_WAKE)) {
            if (semu_layer_intervention_hit(state, logger,
                    SEMU_SAPPORO_222_IV_DIAP_WORKER_WAKE, error) != SEMU_OK) {
                return error != NULL ? error->code : SEMU_ERR_STATE;
            }
            if (semu_bus_write(bus, UINT32_C(0xe000ed04), 4u,
                               UINT32_C(1) << 28u, error) != SEMU_OK) {
                return error != NULL ? error->code : SEMU_ERR_STATE;
            }
            semu_error_clear(error);
            return SEMU_OK;
        }
        return SEMU_OK;
    }
    if (cpu_state->r[15] == UINT32_C(0x0009a3b8)) {
        uint32_t current = 0u;
        uint32_t bitmap = 0u;
        uint32_t isr_word = 0u;
        uint32_t app_state = 0u;
        if (semu_bus_read(bus, UINT32_C(0x10030370), 4u, &current,
                          error) != SEMU_OK ||
            semu_bus_read(bus, UINT32_C(0x1003039c), 4u, &bitmap,
                          error) != SEMU_OK ||
            semu_bus_read(bus, UINT32_C(0x10056608), 4u, &isr_word,
                          error) != SEMU_OK ||
            semu_bus_read(bus, UINT32_C(0x10058510), 1u, &app_state,
                          error) != SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        }
        if (current == UINT32_C(0x10033600) && bitmap == 1u &&
            isr_word == UINT32_C(0x00010101) && app_state == 3u &&
            intervention_is_unused(state,
                SEMU_SAPPORO_222_IV_DIAP_WORKER_IRQ)) {
            if (semu_layer_intervention_hit(state, logger,
                    SEMU_SAPPORO_222_IV_DIAP_WORKER_IRQ, error) != SEMU_OK) {
                return error != NULL ? error->code : SEMU_ERR_STATE;
            }
            if (semu_bus_write(bus, UINT32_C(0x10056a3c), 1u, 1u,
                               error) != SEMU_OK) {
                return error != NULL ? error->code : SEMU_ERR_STATE;
            }
            if (semu_bus_write(bus, UINT32_C(0xe000e200), 4u,
                               UINT32_C(1) << 21u, error) != SEMU_OK) {
                return error != NULL ? error->code : SEMU_ERR_STATE;
            }
            semu_error_clear(error);
            return SEMU_OK;
        }
        return SEMU_OK;
    }
    if (cpu_state->r[15] == UINT32_C(0x0010f6d8)) {
        uint32_t current_state = 0u;
        uint32_t expected_state = 0u;
        semu_error read_error;
        semu_error_clear(&read_error);
        if (semu_bus_read(bus, cpu_state->r[0] + 0x1edu, 1u,
                          &current_state, &read_error) != SEMU_OK ||
            semu_bus_read(bus, cpu_state->r[0] + 0x1eeu, 1u,
                          &expected_state, &read_error) != SEMU_OK) {
            if (error != NULL) *error = read_error;
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        }
        if (cpu_state->r[1] != 0u || current_state != 4u ||
            expected_state != 2u) {
            return SEMU_OK;
        }
        if (!intervention_is_unused(state,
                SEMU_SAPPORO_222_IV_GPS_STATE_DISPATCH)) {
            return SEMU_OK;
        }
        if (semu_layer_intervention_hit(state, logger,
                SEMU_SAPPORO_222_IV_GPS_STATE_DISPATCH, error) != SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_STATE;
        }
        return semu_bus_write(bus, cpu_state->r[0] + 0x1edu, 1u, 2u,
                              error);
    }
    if (cpu_state->r[15] != UINT32_C(0x0010f4fc)) {
        return SEMU_OK;
    }
    /* The synthetic call returns to this instruction after the native
     * UART-open routine.  Let the original epilogue execute on that pass. */
    if (!intervention_is_unused(state, SEMU_SAPPORO_222_IV_GPS_STATE_STARTUP)) {
        return SEMU_OK;
    }
    if (cpu_state->r[0] != 1u || cpu_state->r[4] < UINT32_C(0x10000000) ||
        cpu_state->r[4] >= UINT32_C(0x10180000)) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "Sapporo GPS hook reached with unexpected state");
        return SEMU_ERR_STATE;
    }
    if (semu_layer_intervention_hit(state, logger,
            SEMU_SAPPORO_222_IV_GPS_STATE_STARTUP, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    cpu_state->r[0] = cpu_state->r[4];
    cpu_state->r[14] = UINT32_C(0x0010f4fd);
    cpu_state->r[15] = UINT32_C(0x0010f608);
    return SEMU_OK;
}

semu_status semu_sapporo_222_arm_gps_startup(
    semu_sapporo_cxd5610 *transport, semu_layer_state *state,
    semu_logger *logger, semu_error *error)
{
    static const uint8_t response[] = {
        '$', 'P', 'S', 'S', '0', '0', '0', '0', '\r', '\n'
    };
    if (transport == NULL || state == NULL || logger == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo GPS startup fixture arguments are incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    if (!intervention_is_unused(state, SEMU_SAPPORO_222_IV_GPS_STARTUP)) {
        return SEMU_OK;
    }
    if (!state->enabled || state->hits >= state->descriptor->maximum_hits ||
        state->descriptor->interventions[SEMU_SAPPORO_222_IV_GPS_STARTUP].max_hits == 0u) {
        semu_error_set(error, SEMU_ERR_STATE, "GPS startup fixture is disabled or exhausted");
        return SEMU_ERR_STATE;
    }
    semu_status status = semu_sapporo_cxd5610_inject_rx_after(transport, response,
        sizeof(response), UINT64_C(10000000), error);
    if (status != SEMU_OK) return status;
    return semu_layer_intervention_hit(state, logger,
        SEMU_SAPPORO_222_IV_GPS_STARTUP, error);
}

semu_status semu_sapporo_222_arm_gps_running_status(
    semu_sapporo_cxd5610 *transport,
    semu_sapporo_222_fixture_context *context, semu_error *error)
{
    static const uint8_t response[] = {
        '$', 'P', 'S', 'S', '0', '0', '0', '0', '\r', '\n'
    };
    if (transport == NULL || context == NULL || context->state == NULL ||
        context->logger == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo GPS running-status fixture arguments are incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    if (!intervention_is_unused(context->state,
                                SEMU_SAPPORO_222_IV_GPS_RUNNING_STATUS)) {
        return SEMU_OK;
    }
    if (semu_layer_intervention_hit(context->state, context->logger,
            SEMU_SAPPORO_222_IV_GPS_RUNNING_STATUS, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    if (semu_sapporo_cxd5610_inject_rx_after(transport, response,
            sizeof(response), UINT64_C(10000000), error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    context->gps_running_status_armed = 1;
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_sapporo_222_arm_gps_awake_pulse(
    semu_sapporo_cxd5610 *transport,
    semu_sapporo_222_fixture_context *context, semu_error *error)
{
    if (transport == NULL || context == NULL || context->state == NULL ||
        context->logger == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo GPS awake fixture arguments are incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    if (semu_layer_intervention_hit(context->state, context->logger,
            SEMU_SAPPORO_222_IV_GPS_AWAKE_PULSE, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    return semu_sapporo_cxd5610_pulse_awake_after(
        transport, UINT64_C(100000000), error);
}

semu_transaction_result semu_sapporo_222_gps_exchange(
    void *context, const uint8_t *request, size_t count,
    semu_sapporo_cxd5610 *transport, semu_error *error)
{
    static const uint8_t expected[] = { '@', 'V', 'E', 'R', '\r', '\n' };
    static const uint8_t running_status[] = { '@', 'G', 'S', 'R', '\r', '\n' };
    static const uint8_t use_command[] = {
        '@', 'G', 'U', 'S', 'E', ' ', '0', '\r', '\n'
    };
    semu_sapporo_222_fixture_context *ctx =
        (semu_sapporo_222_fixture_context *)context;
    if (count == sizeof(use_command) &&
        memcmp(request, use_command, count) == 0) {
        /* The native state-10 exchange accepts @GUSE 0 and produces no
         * response; the following state-12 poll is a separate boundary. */
        semu_error_clear(error);
        return SEMU_TRANSACTION_OK;
    }
    if (count == sizeof(running_status) &&
        memcmp(request, running_status, count) == 0) {
        if (ctx == NULL || !ctx->gps_running_status_armed) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "GPS fixture: running-status exchange is not armed");
            return SEMU_TRANSACTION_REFUSE;
        }
        if (semu_sapporo_cxd5610_inject_rx_after(transport,
                (const uint8_t[]){ '$', 'P', 'S', 'S', '0', '0', '0', '0',
                                   '\r', '\n' }, 10u,
                UINT64_C(10000000), error) != SEMU_OK) {
            return SEMU_TRANSACTION_REFUSE;
        }
        return SEMU_TRANSACTION_OK;
    }
    if (count != sizeof(expected) ||
        memcmp(request, expected, count) != 0) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "GPS fixture: unexpected request");
        return SEMU_TRANSACTION_REFUSE;
    }
    if (ctx != NULL && ctx->state != NULL && ctx->logger != NULL &&
        intervention_is_unused(ctx->state, SEMU_SAPPORO_222_IV_GPS_STARTUP)) {
        if (semu_sapporo_222_arm_gps_startup(transport, ctx->state,
                ctx->logger, error) != SEMU_OK) {
            return SEMU_TRANSACTION_REFUSE;
        }
    } else if (semu_sapporo_cxd5610_inject_rx_after(transport,
            (const uint8_t[]){ '$', 'P', 'S', 'S', '0', '0', '0', '0',
                               '\r', '\n' }, 10u,
            UINT64_C(10000000), error) != SEMU_OK) {
            return SEMU_TRANSACTION_REFUSE;
    }
    return SEMU_TRANSACTION_OK;
}
