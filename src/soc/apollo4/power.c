#include "power.h"

#include <stdlib.h>

enum {
    PERFORMANCE_CONTROL = 0x00u,
    DEVICE_POWER_ENABLE = 0x04u,
    DEVICE_POWER_STATUS = 0x08u,
    LEGACY_STATUS_14 = 0x14u,
    LEGACY_STATUS_18 = 0x18u,
    LEGACY_CONTROL_1C = 0x1cu,
    SHARED_SRAM_ENABLE = 0x24u,
    SHARED_SRAM_STATUS = 0x28u,
    SHARED_SRAM_RETENTION = 0x2cu,
    DSP0_MEMORY_ENABLE = 0x58u, DSP0_MEMORY_STATUS = 0x5cu,
    DSP0_MEMORY_RETENTION = 0x60u,
    DSP1_MEMORY_ENABLE = 0x78u, DSP1_MEMORY_STATUS = 0x7cu,
    DSP1_MEMORY_RETENTION = 0x80u,
    SIMO_BUCK_ENABLE = 0x100u,
    VOLTAGE_REGULATORS_STATUS = 0x108u,
    LEGACY_WINDOW_FIRST = 0x140u,
    LEGACY_WINDOW_LAST = 0x188u
};

static const uint32_t NEMA_POWER_MASK = UINT32_C(1) << 17;
static const uint32_t DEVICE_POWER_LOW_MASK = UINT32_C(0x1e);
static const uint32_t DEVICE_POWER_MID_MASK = UINT32_C(0x20);
static const uint32_t DEVICE_POWER_HIGH_MASK = UINT32_C(0x1400);
static const uint32_t DEVICE_POWER_STATUS_LOW = UINT32_C(0x1e);
static const uint32_t DEVICE_POWER_STATUS_MID = UINT32_C(0x1e0);
static const uint32_t DEVICE_POWER_STATUS_HIGH = UINT32_C(0x1e00);
static const uint32_t SIMO_STATUS_MASK = UINT32_C(0x30);
static const uint32_t SHARED_SRAM_ENABLE_MASK = UINT32_C(0x3);
static const uint32_t SHARED_SRAM_RETENTION_MASK = UINT32_C(0x3ff);
static const uint32_t DSP_MEMORY_ENABLE_MASK = UINT32_C(0x3);
static const uint32_t DSP_MEMORY_RETENTION_MASK = UINT32_C(0x1f);

struct semu_apollo4_power {
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4_power_state_callback callback;
    void *callback_context;
    uint32_t performance_control;
    uint32_t device_power_enable;
    uint32_t device_power_status;
    uint32_t legacy_status_14;
    uint32_t legacy_control_1c;
    uint32_t shared_sram_enable;
    uint32_t shared_sram_retention;
    uint32_t dsp0_memory_enable, dsp0_memory_retention;
    uint32_t dsp1_memory_enable, dsp1_memory_retention;
    uint32_t simo_buck_enable;
};

static const semu_bus_device_ops power_ops = {
    semu_apollo4_power_read,
    semu_apollo4_power_write,
    semu_apollo4_power_reset
};

static int is_known_read_offset(uint32_t offset)
{
    return offset == PERFORMANCE_CONTROL || offset == DEVICE_POWER_ENABLE ||
           offset == DEVICE_POWER_STATUS || offset == LEGACY_STATUS_14 ||
           offset == LEGACY_STATUS_18 || offset == LEGACY_CONTROL_1C ||
           offset == SHARED_SRAM_ENABLE || offset == SHARED_SRAM_STATUS ||
           offset == SHARED_SRAM_RETENTION ||
           offset == DSP0_MEMORY_ENABLE || offset == DSP0_MEMORY_STATUS ||
           offset == DSP0_MEMORY_RETENTION ||
           offset == DSP1_MEMORY_ENABLE || offset == DSP1_MEMORY_STATUS ||
           offset == DSP1_MEMORY_RETENTION || offset == SIMO_BUCK_ENABLE ||
           offset == VOLTAGE_REGULATORS_STATUS;
}

static int is_legacy_window_offset(uint32_t offset)
{
    return offset >= LEGACY_WINDOW_FIRST && offset <= LEGACY_WINDOW_LAST &&
           ((offset - LEGACY_WINDOW_FIRST) % 4u) == 0u;
}

static int is_known_write_offset(uint32_t offset)
{
    return offset == PERFORMANCE_CONTROL || offset == DEVICE_POWER_ENABLE ||
           offset == LEGACY_STATUS_14 || offset == LEGACY_CONTROL_1C ||
           offset == SHARED_SRAM_ENABLE || offset == SHARED_SRAM_RETENTION ||
           offset == DSP0_MEMORY_ENABLE ||
           offset == DSP0_MEMORY_RETENTION ||
           offset == DSP1_MEMORY_ENABLE ||
           offset == DSP1_MEMORY_RETENTION ||
           offset == SIMO_BUCK_ENABLE || is_legacy_window_offset(offset);
}

static semu_status validate_masked_value(uint32_t offset, uint32_t value,
                                         uint32_t mask, semu_error *error)
{
    if ((value & ~mask) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 power offset 0x%08x value 0x%08x is unsupported",
                       offset, value);
        return SEMU_ERR_UNSUPPORTED;
    }
    return SEMU_OK;
}

static uint32_t device_power_status_for(uint32_t enable)
{
    uint32_t status = 0u;

    if ((enable & DEVICE_POWER_LOW_MASK) != 0u) {
        status |= DEVICE_POWER_STATUS_LOW;
    }
    if ((enable & DEVICE_POWER_MID_MASK) != 0u) {
        status |= DEVICE_POWER_STATUS_MID;
    }
    if ((enable & DEVICE_POWER_HIGH_MASK) != 0u) {
        status |= DEVICE_POWER_STATUS_HIGH;
    }
    if ((enable & NEMA_POWER_MASK) != 0u) {
        status |= NEMA_POWER_MASK;
    }
    return status;
}

static semu_status validate_access(void *context, uint32_t offset,
                                   unsigned width, semu_error *error)
{
    if (context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Apollo4 power context required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 power supports 32-bit accesses only");
        return SEMU_ERR_UNSUPPORTED;
    }
    if (!is_known_read_offset(offset) && !is_known_write_offset(offset)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 power offset 0x%08x is unsupported", offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

static uint64_t virtual_time(const semu_apollo4_power *power)
{
    return semu_scheduler_now(power->scheduler);
}

static void report_gate(semu_apollo4_power *power,
                        semu_apollo4_power_gate gate, int old_enabled,
                        int new_enabled)
{
    if (power->callback != NULL && old_enabled != new_enabled) {
        power->callback(power->callback_context, gate, new_enabled,
                        virtual_time(power));
    }
}

static void reset_state(semu_apollo4_power *power, int report)
{
    int old_nema = power->device_power_status != 0u;
    int old_sram = power->shared_sram_enable != 0u;
    int old_simo = power->simo_buck_enable != 0u;

    power->performance_control = UINT32_C(0x0d);
    power->device_power_enable = 0u;
    power->device_power_status = 0u;
    power->shared_sram_enable = 0u;
    power->legacy_status_14 = 0x3fu;
    power->legacy_control_1c = 0x8u;
    power->shared_sram_retention = 0x3fcu;
    power->dsp0_memory_enable = power->dsp0_memory_retention = 0u;
    power->dsp1_memory_enable = power->dsp1_memory_retention = 0u;
    power->simo_buck_enable = 0u;
    if (report) {
        report_gate(power, SEMU_APOLLO4_POWER_GATE_NEMA, old_nema, 0);
        report_gate(power, SEMU_APOLLO4_POWER_GATE_SHARED_SRAM, old_sram, 0);
        report_gate(power, SEMU_APOLLO4_POWER_GATE_SIMO_BUCK, old_simo, 0);
    }
}

semu_apollo4_power *semu_apollo4_power_create(
    semu_bus *bus, semu_scheduler *scheduler,
    semu_apollo4_power_state_callback callback, void *callback_context,
    semu_error *error)
{
    semu_apollo4_power *power;
    semu_status status;

    if (bus == NULL || scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 power requires bus and scheduler");
        return NULL;
    }
    power = (semu_apollo4_power *)calloc(1u, sizeof(*power));
    if (power == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate Apollo4 power controller");
        return NULL;
    }
    power->bus = bus;
    power->scheduler = scheduler;
    power->callback = callback;
    power->callback_context = callback_context;
    reset_state(power, 0);
    status = semu_bus_map_device(bus, "apollo4.power", SEMU_APOLLO4_POWER_BASE,
                                 SEMU_APOLLO4_POWER_SIZE, &power_ops, power,
                                 error);
    if (status != SEMU_OK) {
        free(power);
        return NULL;
    }
    semu_error_clear(error);
    return power;
}

void semu_apollo4_power_destroy(semu_apollo4_power *power)
{
    free(power);
}

void semu_apollo4_power_reset(void *context)
{
    semu_apollo4_power *power = (semu_apollo4_power *)context;
    if (power != NULL) {
        reset_state(power, 1);
    }
}

semu_status semu_apollo4_power_read(void *context, uint32_t offset,
                                    unsigned width, uint32_t *value,
                                    semu_error *error)
{
    semu_apollo4_power *power = (semu_apollo4_power *)context;
    semu_status status = validate_access(context, offset, width, error);

    if (status != SEMU_OK) {
        return status;
    }
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "power read value required");
        return SEMU_ERR_ARGUMENT;
    }
    switch (offset) {
    case PERFORMANCE_CONTROL:
        *value = power->performance_control;
        break;
    case DEVICE_POWER_ENABLE:
        *value = power->device_power_enable;
        break;
    case DEVICE_POWER_STATUS:
        *value = power->device_power_status;
        break;
    case LEGACY_STATUS_14:
        *value = power->legacy_status_14;
        break;
    case LEGACY_STATUS_18:
        *value = 0x3fu;
        break;
    case LEGACY_CONTROL_1C:
        *value = power->legacy_control_1c;
        break;
    case SHARED_SRAM_ENABLE:
        *value = power->shared_sram_enable;
        break;
    case SHARED_SRAM_STATUS:
        *value = power->shared_sram_enable;
        break;
    case SHARED_SRAM_RETENTION:
        *value = power->shared_sram_retention;
        break;
    case DSP0_MEMORY_ENABLE:
    case DSP0_MEMORY_STATUS:
        *value = power->dsp0_memory_enable;
        break;
    case DSP0_MEMORY_RETENTION:
        *value = power->dsp0_memory_retention;
        break;
    case DSP1_MEMORY_ENABLE:
    case DSP1_MEMORY_STATUS:
        *value = power->dsp1_memory_enable;
        break;
    case DSP1_MEMORY_RETENTION:
        *value = power->dsp1_memory_retention;
        break;
    case SIMO_BUCK_ENABLE:
        *value = power->simo_buck_enable;
        break;
    case VOLTAGE_REGULATORS_STATUS:
        *value = power->simo_buck_enable != 0u ? SIMO_STATUS_MASK : 0u;
        break;
    default:
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_apollo4_power_write(void *context, uint32_t offset,
                                     unsigned width, uint32_t value,
                                     semu_error *error)
{
    semu_apollo4_power *power = (semu_apollo4_power *)context;
    uint32_t next;
    int old_enabled;
    int new_enabled;
    semu_status status = validate_access(context, offset, width, error);

    if (status != SEMU_OK) {
        return status;
    }
    if (!is_known_write_offset(offset)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 power offset 0x%08x is read-only", offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    switch (offset) {
    case PERFORMANCE_CONTROL:
        next = value & UINT32_C(0x3);
        power->performance_control = next | UINT32_C(0x4) | (next << 3);
        break;
    case DEVICE_POWER_ENABLE:
        next = value;
        old_enabled = power->device_power_status != 0u;
        new_enabled = (next & NEMA_POWER_MASK) != 0u;
        power->device_power_enable = next;
        power->device_power_status = device_power_status_for(next);
        report_gate(power, SEMU_APOLLO4_POWER_GATE_NEMA, old_enabled,
                    new_enabled);
        break;
    case LEGACY_STATUS_14:
        power->legacy_status_14 = value;
        break;
    case LEGACY_CONTROL_1C:
        if (value != 0x8u) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "Apollo4 power legacy control 0x1c value 0x%08x is unsupported",
                           value);
            return SEMU_ERR_UNSUPPORTED;
        }
        power->legacy_control_1c = value;
        break;
    case SHARED_SRAM_ENABLE:
        next = value & SHARED_SRAM_ENABLE_MASK;
        old_enabled = power->shared_sram_enable != 0u;
        new_enabled = next != 0u;
        power->shared_sram_enable = next;
        report_gate(power, SEMU_APOLLO4_POWER_GATE_SHARED_SRAM, old_enabled,
                    new_enabled);
        break;
    case SHARED_SRAM_RETENTION:
        power->shared_sram_retention = value & SHARED_SRAM_RETENTION_MASK;
        break;
    case DSP0_MEMORY_ENABLE:
        if (validate_masked_value(offset, value, DSP_MEMORY_ENABLE_MASK,
                                  error) != SEMU_OK) return error->code;
        power->dsp0_memory_enable = value;
        break;
    case DSP0_MEMORY_RETENTION:
        if (validate_masked_value(offset, value, DSP_MEMORY_RETENTION_MASK,
                                  error) != SEMU_OK) return error->code;
        power->dsp0_memory_retention = value;
        break;
    case DSP1_MEMORY_ENABLE:
        if (validate_masked_value(offset, value, DSP_MEMORY_ENABLE_MASK,
                                  error) != SEMU_OK) return error->code;
        power->dsp1_memory_enable = value;
        break;
    case DSP1_MEMORY_RETENTION:
        if (validate_masked_value(offset, value, DSP_MEMORY_RETENTION_MASK,
                                  error) != SEMU_OK) return error->code;
        power->dsp1_memory_retention = value;
        break;
    case SIMO_BUCK_ENABLE:
        next = value & UINT32_C(0x1);
        old_enabled = power->simo_buck_enable != 0u;
        new_enabled = next != 0u;
        power->simo_buck_enable = next;
        report_gate(power, SEMU_APOLLO4_POWER_GATE_SIMO_BUCK, old_enabled,
                    new_enabled);
        break;
    default:
        if (is_legacy_window_offset(offset)) {
            if (value != 0u) {
                semu_error_set(error,
                               SEMU_ERR_UNSUPPORTED,
                               "Apollo4 power legacy window 0x%08x value 0x%08x is unsupported",
                               offset, value);
                return SEMU_ERR_UNSUPPORTED;
            }
            break;
        }
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

const semu_bus_device_ops *semu_apollo4_power_bus_ops(void)
{
    return &power_ops;
}

const char *semu_apollo4_power_gate_name(semu_apollo4_power_gate gate)
{
    switch (gate) {
    case SEMU_APOLLO4_POWER_GATE_NEMA:
        return "nema";
    case SEMU_APOLLO4_POWER_GATE_SHARED_SRAM:
        return "shared-sram";
    case SEMU_APOLLO4_POWER_GATE_SIMO_BUCK:
        return "simo-buck";
    default:
        return NULL;
    }
}

semu_status semu_apollo4_power_snapshot_write(
    const semu_apollo4_power *power, semu_snapshot_writer *writer,
    semu_error *error)
{
    const uint32_t values[] = {
        power != NULL ? power->performance_control : 0u,
        power != NULL ? power->device_power_enable : 0u,
        power != NULL ? power->device_power_status : 0u,
        power != NULL ? power->legacy_status_14 : 0u,
        power != NULL ? power->legacy_control_1c : 0u,
        power != NULL ? power->shared_sram_enable : 0u,
        power != NULL ? power->shared_sram_retention : 0u,
        power != NULL ? power->dsp0_memory_enable : 0u,
        power != NULL ? power->dsp0_memory_retention : 0u,
        power != NULL ? power->dsp1_memory_enable : 0u,
        power != NULL ? power->dsp1_memory_retention : 0u,
        power != NULL ? power->simo_buck_enable : 0u
    };
    size_t index;
    if (power == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "power snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    for (index = 0u; index < SEMU_ARRAY_LEN(values); ++index) {
        if (semu_snapshot_writer_u32(writer, values[index], error) != SEMU_OK)
            return error->code;
    }
    return SEMU_OK;
}

semu_status semu_apollo4_power_snapshot_read(
    semu_apollo4_power *power, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_apollo4_power candidate;
    uint32_t *values[12];
    size_t index;
    if (power == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "power snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *power;
    values[0] = &candidate.performance_control;
    values[1] = &candidate.device_power_enable;
    values[2] = &candidate.device_power_status;
    values[3] = &candidate.legacy_status_14;
    values[4] = &candidate.legacy_control_1c;
    values[5] = &candidate.shared_sram_enable;
    values[6] = &candidate.shared_sram_retention;
    values[7] = &candidate.dsp0_memory_enable;
    values[8] = &candidate.dsp0_memory_retention;
    values[9] = &candidate.dsp1_memory_enable;
    values[10] = &candidate.dsp1_memory_retention;
    values[11] = &candidate.simo_buck_enable;
    for (index = 0u; index < SEMU_ARRAY_LEN(values); ++index) {
        if (semu_snapshot_reader_u32(reader, values[index], error) != SEMU_OK)
            return error->code;
    }
    if ((candidate.performance_control & ~UINT32_C(0x1f)) != 0u ||
        (candidate.performance_control & UINT32_C(0x4)) == 0u ||
        ((candidate.performance_control >> 3u) & UINT32_C(0x3)) !=
            (candidate.performance_control & UINT32_C(0x3)) ||
        candidate.device_power_status !=
            device_power_status_for(candidate.device_power_enable) ||
        candidate.legacy_control_1c != UINT32_C(0x8) ||
        (candidate.shared_sram_enable & ~SHARED_SRAM_ENABLE_MASK) != 0u ||
        (candidate.shared_sram_retention & ~SHARED_SRAM_RETENTION_MASK) != 0u ||
        (candidate.dsp0_memory_enable & ~DSP_MEMORY_ENABLE_MASK) != 0u ||
        (candidate.dsp0_memory_retention & ~DSP_MEMORY_RETENTION_MASK) != 0u ||
        (candidate.dsp1_memory_enable & ~DSP_MEMORY_ENABLE_MASK) != 0u ||
        (candidate.dsp1_memory_retention & ~DSP_MEMORY_RETENTION_MASK) != 0u ||
        (candidate.simo_buck_enable & ~UINT32_C(0x1)) != 0u) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "power snapshot state is unreachable");
        return SEMU_ERR_FORMAT;
    }
    *power = candidate;
    return SEMU_OK;
}
