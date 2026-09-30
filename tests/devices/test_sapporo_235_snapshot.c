#include "test.h"

#include "semu/hash.h"
#include "semu/machine.h"
#include "semu/trace.h"

#include "../../src/boards/machine_internal.h"
#include "../../src/core/scheduler_internal.h"
#include "../../src/devices/sapporo_iom4.h"
#include "../../src/devices/sapporo_iom4_internal.h"
#include "../../src/devices/sapporo_rtc.h"
#include "../../src/soc/apollo4/apollo4_internal.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * Ticket 792: the 2.35 live RTC/IOM4 snapshot codecs. Machine-level cases
 * prove save/load round-trip equality, save-twice byte identity, restored
 * alarm rearm, and the truncated/section-missing refusals with atomic
 * rollback; device-level cases prove the mid-command IOM4 refusal on both
 * directions (E-SAP-0036/0040 state only, no invented fields).
 */

static int write_bytes(const char *path, const uint8_t *program, size_t size)
{
    FILE *stream = fopen(path, "wb");
    if (stream == NULL) return 0;
    if (fwrite(program, 1u, size, stream) != size) {
        (void)fclose(stream); return 0;
    }
    return fclose(stream) == 0;
}

static void copy_text(char *destination, size_t capacity, const char *source)
{
    (void)snprintf(destination, capacity, "%s", source);
}

static int make_contract(const char *path, semu_profile *profile,
                         semu_firmware_manifest *firmware, semu_error *error)
{
    semu_component *required;
    semu_component *component;
    uint64_t size;
    memset(profile, 0, sizeof(*profile));
    memset(firmware, 0, sizeof(*firmware));
    profile->format = 1u;
    copy_text(profile->id, sizeof(profile->id), "sapporo-2.35.34");
    copy_text(profile->board, sizeof(profile->board), "sapporo");
    copy_text(profile->product, sizeof(profile->product), "Synthetic Sapporo");
    copy_text(profile->version, sizeof(profile->version), "machine-test");
    profile->display_width = 240u;
    profile->display_height = 240u;
    profile->required_count = 1u;
    firmware->format = 1u;
    copy_text(firmware->product, sizeof(firmware->product), profile->product);
    copy_text(firmware->version, sizeof(firmware->version), profile->version);
    firmware->component_count = 1u;
    component = &firmware->components[0];
    copy_text(component->id, sizeof(component->id), "synthetic-reset");
    copy_text(component->role, sizeof(component->role), "application");
    copy_text(component->path, sizeof(component->path), path);
    component->load_address = 0u;
    if (semu_sha256_file(path, component->sha256, &size, error) != SEMU_OK)
        return 0;
    component->size = size;
    required = &profile->required[0];
    *required = *component;
    required->path[0] = '\0';
    return 1;
}

static void wr(semu_test_context *c, semu_bus *b, uint32_t a, uint32_t v)
{
    semu_error error;

    semu_error_clear(&error);
    SEMU_TEST_ASSERT(c, semu_bus_write(b, a, 4u, v, &error) == SEMU_OK);
}

static void rd_eq(semu_test_context *c, semu_bus *b, uint32_t a, uint32_t want)
{
    semu_error error;
    uint32_t value = 0xdeadbeefu;

    semu_error_clear(&error);
    SEMU_TEST_ASSERT(c, semu_bus_read(b, a, 4u, &value, &error) == SEMU_OK);
    SEMU_TEST_EQ_U64(c, want, value);
}

/* The E-SAP-0035 census arming: RPT=Second alarm plus I2C-enabled IOM4
 * state (FIFO word, threshold, INTEN, one gauge register read). */
static void drive_235_state(semu_test_context *c, semu_machine *machine)
{
    semu_error error;

    semu_error_clear(&error);
    SEMU_TEST_ASSERT(c, semu_scheduler_advance(
        machine->scheduler, UINT64_C(11800000), &error) == SEMU_OK);
    wr(c, machine->bus, UINT32_C(0x40004830), 0u);   /* AlarmsLower = 0 */
    wr(c, machine->bus, UINT32_C(0x40004800), 0xEu);  /* CTRL: RPT=Second */
    wr(c, machine->bus, UINT32_C(0x40004a08), 1u);   /* clear */
    wr(c, machine->bus, UINT32_C(0x40004a00), 1u);   /* enable */
    wr(c, machine->bus, UINT32_C(0x40054000), UINT32_C(0x12345678));
    wr(c, machine->bus, UINT32_C(0x40054104), UINT32_C(0x0203));
    wr(c, machine->bus, UINT32_C(0x4005411c), UINT32_C(0x10));  /* I2C mode */
    wr(c, machine->bus, UINT32_C(0x400542c4), UINT32_C(0x36));  /* gauge */
    wr(c, machine->bus, UINT32_C(0x40054200), UINT32_C(0x0001)); /* INTEN CMD */
    wr(c, machine->bus, UINT32_C(0x40054120),
       UINT32_C(0x06000212)); /* read gauge reg 6, two bytes */
    wr(c, machine->bus, UINT32_C(0x40004820), UINT32_C(0)); /* counters read */
}

static void probe_235_state(semu_test_context *c, semu_machine *machine,
                           int pending)
{
    int line = -1;

    SEMU_TEST_EQ_U64(c, 0xEu,
        semu_sapporo_rtc_probe(machine->soc->rtc, 0x000u));
    SEMU_TEST_EQ_U64(c, pending ? 1 : 0,
        (uint64_t)semu_sapporo_rtc_probe_pending(machine->soc->rtc, &line));
    rd_eq(c, machine->bus, UINT32_C(0x40004a04), 0u); /* status still clear */
    rd_eq(c, machine->bus, UINT32_C(0x40004830), 0u);
    rd_eq(c, machine->bus, UINT32_C(0x40054000), UINT32_C(0x12345678));
    rd_eq(c, machine->bus, UINT32_C(0x40054020), UINT32_C(0x3200));
    rd_eq(c, machine->bus, UINT32_C(0x40054104), UINT32_C(0x0203));
    rd_eq(c, machine->bus, UINT32_C(0x40054100), UINT32_C(0x1c042000));
    rd_eq(c, machine->bus, UINT32_C(0x40054200), UINT32_C(0x0001));
    rd_eq(c, machine->bus, UINT32_C(0x40054204), UINT32_C(0x0003));
    rd_eq(c, machine->bus, UINT32_C(0x4005412c), UINT32_C(0x00000080));
    rd_eq(c, machine->bus, UINT32_C(0x40054248), UINT32_C(0x00000004));
    rd_eq(c, machine->bus, UINT32_C(0x400542c4), UINT32_C(0x00000036));
}

static void test_machine_roundtrip_and_rearm(semu_test_context *context)
{
    static const size_t buffer_size = 8u * 1024u * 1024u;
    static const uint32_t section_ids[] = {
        SEMU_SNAPSHOT_SECTION_CPU_STATE, SEMU_SNAPSHOT_SECTION_RAM,
        SEMU_SNAPSHOT_SECTION_VIRTUAL_TIME, SEMU_SNAPSHOT_SECTION_STOP_REASON,
        SEMU_SNAPSHOT_SECTION_SCHEDULER, SEMU_SNAPSHOT_SECTION_SOC_STATE,
        SEMU_SNAPSHOT_SECTION_DEVICES, SEMU_SNAPSHOT_SECTION_STORAGE,
        SEMU_SNAPSHOT_SECTION_NEMA, SEMU_SNAPSHOT_SECTION_MACHINE,
        SEMU_SNAPSHOT_SECTION_DISPLAY
    };
    static const uint8_t program[64] = {
        0x00u, 0x01u, 0x00u, 0x10u, 0x21u, 0x00u, 0x00u, 0x00u,
        [0x20] = 0x00u, 0xbfu, [0x22] = 0x00u, 0xbfu,
        [0x24] = 0x00u, 0xbfu, [0x26] = 0x00u, 0xbeu
    };
    char path[128];
    semu_profile profile;
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_machine *first;
    semu_machine *second;
    semu_snapshot *source;
    semu_snapshot *loaded;
    semu_error error;
    uint8_t *buffer;
    uint8_t *second_buffer;
    size_t length;
    size_t second_length;
    size_t index;

    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(path, sizeof(path), "sap235-snapshot.bin"));
    SEMU_TEST_ASSERT(context, write_bytes(path, program, sizeof(program)));
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, make_contract(path, &profile, &firmware, &error));
    memset(&options, 0, sizeof(options));
    options.profile = &profile;
    options.firmware = &firmware;
    first = semu_machine_create(&options, &error);
    second = semu_machine_create(&options, &error);
    source = semu_snapshot_create(&error);
    loaded = semu_snapshot_create(&error);
    buffer = (uint8_t *)malloc(buffer_size);
    second_buffer = (uint8_t *)malloc(buffer_size);
    SEMU_TEST_ASSERT(context, first != NULL && second != NULL &&
                     source != NULL && loaded != NULL && buffer != NULL &&
                     second_buffer != NULL);
    if (first == NULL || second == NULL || source == NULL ||
        loaded == NULL || buffer == NULL || second_buffer == NULL) {
        goto done;
    }
    drive_235_state(context, first);
    probe_235_state(context, first, 1);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_machine_snapshot_save(first, source, &error));
    length = semu_snapshot_serialize(source, buffer, buffer_size);
    SEMU_TEST_ASSERT(context, length > 0u);
    /* Save twice: the image must be byte-identical. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_machine_snapshot_save(first, source, &error));
    second_length = semu_snapshot_serialize(source, second_buffer, buffer_size);
    SEMU_TEST_EQ_U64(context, (uint64_t)length, (uint64_t)second_length);
    SEMU_TEST_ASSERT(context,
                     memcmp(buffer, second_buffer, length) == 0);
    /* Load into the second machine and compare every covered probe. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_snapshot_deserialize(loaded, buffer, length, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_machine_snapshot_load(second, loaded, &error));
    probe_235_state(context, second, 1);
    /* The restored alarm event is rearmed through the restored scheduler:
     * firing at the same whole-second occurrence raises InterruptStatus on
     * both machines identically. */
    {
        uint64_t now = semu_scheduler_now(first->scheduler);
        uint64_t due = UINT64_C(1000000000);
        semu_error local;
        if (due < now) {
            SEMU_TEST_ASSERT(context, 0);
            goto done;
        }
        semu_error_clear(&local);
        SEMU_TEST_ASSERT(context, semu_scheduler_advance(
            first->scheduler, due - now, &local) == SEMU_OK);
        semu_error_clear(&local);
        SEMU_TEST_ASSERT(context, semu_scheduler_advance(
            second->scheduler, due - now, &local) == SEMU_OK);
    }
    rd_eq(context, first->bus, UINT32_C(0x40004a04), 1u);
    rd_eq(context, second->bus, UINT32_C(0x40004a04), 1u);
    /* The one-second cadence continues identically after restore:
     * clear, then the next occurrence latches again on both machines. */
    wr(context, first->bus, UINT32_C(0x40004a08), 1u);
    wr(context, second->bus, UINT32_C(0x40004a08), 1u);
    rd_eq(context, first->bus, UINT32_C(0x40004a04), 0u);
    rd_eq(context, second->bus, UINT32_C(0x40004a04), 0u);
    {
        semu_error local;
        semu_error_clear(&local);
        SEMU_TEST_ASSERT(context, semu_scheduler_advance(
            first->scheduler, UINT64_C(1000000000), &local) == SEMU_OK);
        semu_error_clear(&local);
        SEMU_TEST_ASSERT(context, semu_scheduler_advance(
            second->scheduler, UINT64_C(1000000000), &local) == SEMU_OK);
    }
    rd_eq(context, first->bus, UINT32_C(0x40004a04), 1u);
    rd_eq(context, second->bus, UINT32_C(0x40004a04), 1u);
    /* A profile/identity mismatch refuses. */
    {
        semu_snapshot *rebuilt;
        semu_snapshot *fresh;
        rebuilt = semu_snapshot_create(&error);
        SEMU_TEST_ASSERT(context, rebuilt != NULL);
        if (rebuilt != NULL) {
            for (index = 0u; index < sizeof(section_ids) /
                 sizeof(section_ids[0]); ++index) {
                const uint8_t *data;
                size_t size;
                SEMU_TEST_EQ_U64(context, SEMU_OK,
                    semu_snapshot_read_section(loaded, section_ids[index],
                                                &data, &size));
                SEMU_TEST_EQ_U64(context, SEMU_OK,
                    semu_snapshot_write_section(rebuilt, section_ids[index],
                                                data, size, &error));
            }
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_snapshot_set_identity(rebuilt, "sapporo-2.22.60",
                                           semu_snapshot_firmware_hash(loaded),
                                           &error));
            SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                semu_machine_snapshot_load(second, rebuilt, &error));
            semu_snapshot_destroy(rebuilt);
        }
        fresh = semu_snapshot_create(&error);
        SEMU_TEST_ASSERT(context, fresh != NULL);
        if (fresh != NULL) {
            SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                semu_machine_snapshot_load(second, fresh, &error));
            semu_snapshot_destroy(fresh);
        }
    }
done:
    free(second_buffer);
    free(buffer);
    semu_snapshot_destroy(loaded);
    semu_snapshot_destroy(source);
    semu_machine_destroy(second);
    semu_machine_destroy(first);
}

static void test_section_missing_and_truncated_refuse_atomically(
    semu_test_context *context)
{
    static const size_t buffer_size = 8u * 1024u * 1024u;
    static const uint8_t program[64] = {
        0x00u, 0x01u, 0x00u, 0x10u, 0x21u, 0x00u, 0x00u, 0x00u,
        [0x20] = 0x00u, 0xbfu, [0x22] = 0x00u, 0xbfu,
        [0x24] = 0x00u, 0xbfu, [0x26] = 0x00u, 0xbeu
    };
    char path[128];
    semu_profile profile;
    semu_firmware_manifest firmware;
    semu_machine_options options;
    semu_machine *first;
    semu_machine *second;
    semu_snapshot *source;
    semu_snapshot *loaded;
    semu_error error;
    uint8_t *buffer;
    size_t length;
    const uint8_t *section_data;
    size_t section_size;

    SEMU_TEST_ASSERT(context,
        semu_test_temp_path(path, sizeof(path), "sap235-snap-ref.bin"));
    SEMU_TEST_ASSERT(context, write_bytes(path, program, sizeof(program)));
    semu_error_clear(&error);
    SEMU_TEST_ASSERT(context, make_contract(path, &profile, &firmware, &error));
    memset(&options, 0, sizeof(options));
    options.profile = &profile;
    options.firmware = &firmware;
    first = semu_machine_create(&options, &error);
    second = semu_machine_create(&options, &error);
    source = semu_snapshot_create(&error);
    loaded = semu_snapshot_create(&error);
    buffer = (uint8_t *)malloc(buffer_size);
    SEMU_TEST_ASSERT(context, first != NULL && second != NULL &&
                     source != NULL && loaded != NULL && buffer != NULL);
    if (first == NULL || second == NULL || source == NULL ||
        loaded == NULL || buffer == NULL) {
        goto done;
    }
    drive_235_state(context, first);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_machine_snapshot_save(first, source, &error));
    length = semu_snapshot_serialize(source, buffer, buffer_size);
    SEMU_TEST_ASSERT(context, length > 0u);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_snapshot_deserialize(loaded, buffer, length, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_machine_snapshot_load(second, loaded, &error));
    probe_235_state(context, second, 1);
    /* Drop the SoC section: the load refuses and the machine is untouched. */
    {
        semu_snapshot *rebuilt;
        static const uint32_t keep[] = {
            SEMU_SNAPSHOT_SECTION_CPU_STATE, SEMU_SNAPSHOT_SECTION_RAM,
            SEMU_SNAPSHOT_SECTION_VIRTUAL_TIME,
            SEMU_SNAPSHOT_SECTION_STOP_REASON,
            SEMU_SNAPSHOT_SECTION_SCHEDULER, SEMU_SNAPSHOT_SECTION_DEVICES,
            SEMU_SNAPSHOT_SECTION_STORAGE, SEMU_SNAPSHOT_SECTION_NEMA,
            SEMU_SNAPSHOT_SECTION_MACHINE, SEMU_SNAPSHOT_SECTION_DISPLAY
        };
        size_t index;
        rebuilt = semu_snapshot_create(&error);
        SEMU_TEST_ASSERT(context, rebuilt != NULL);
        if (rebuilt != NULL) {
            for (index = 0u; index < sizeof(keep) / sizeof(keep[0]); ++index) {
                const uint8_t *data;
                size_t size;
                SEMU_TEST_EQ_U64(context, SEMU_OK,
                    semu_snapshot_read_section(loaded, keep[index],
                                                &data, &size));
                SEMU_TEST_EQ_U64(context, SEMU_OK,
                    semu_snapshot_write_section(rebuilt, keep[index],
                                                data, size, &error));
            }
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_snapshot_set_identity(rebuilt,
                    semu_snapshot_profile_id(loaded),
                    semu_snapshot_firmware_hash(loaded), &error));
            SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                semu_machine_snapshot_load(second, rebuilt, &error));
            semu_snapshot_destroy(rebuilt);
        }
    }
    probe_235_state(context, second, 1);
    /* Truncate the SoC section: the live-module bytes are the tail, so the
     * truncated image must refuse and roll back atomically. */
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_snapshot_read_section(loaded, SEMU_SNAPSHOT_SECTION_SOC_STATE,
                                   &section_data, &section_size));
    SEMU_TEST_ASSERT(context, section_size > 8u);
    {
        uint8_t *truncated = (uint8_t *)malloc(section_size - 4u);
        SEMU_TEST_ASSERT(context, truncated != NULL);
        if (truncated != NULL) {
            memcpy(truncated, section_data, section_size - 4u);
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_snapshot_write_section(loaded,
                    SEMU_SNAPSHOT_SECTION_SOC_STATE, truncated,
                    section_size - 4u, &error));
            free(truncated);
        }
    }
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_machine_snapshot_load(second, loaded, &error));
    probe_235_state(context, second, 1);
done:
    free(buffer);
    semu_snapshot_destroy(loaded);
    semu_snapshot_destroy(source);
    semu_machine_destroy(second);
    semu_machine_destroy(first);
}

/* Device-level: the E-SAP-0036 mid-command state (a read command whose
 * size exceeds the RX FIFO) refuses on save; a forged image with the
 * loaded-DMA flag refuses on load. */
static void test_iom4_midcommand_refuses_both_directions(
    semu_test_context *context)
{
    semu_error error;
    semu_bus *bus;
    semu_sapporo_iom4 *iom4;
    semu_snapshot_writer writer;
    semu_snapshot_reader reader;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL);
    if (bus == NULL) return;
    iom4 = semu_sapporo_iom4_create(bus, NULL, NULL, &error);
    SEMU_TEST_ASSERT(context, iom4 != NULL);
    if (iom4 == NULL) {
        semu_bus_destroy(bus);
        return;
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_iom4_write(
        iom4, UINT32_C(0x11c), 4u, UINT32_C(0x10), &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_iom4_write(
        iom4, UINT32_C(0x2c4), 4u, UINT32_C(0x36), &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sapporo_iom4_write(
        iom4, UINT32_C(0x120), 4u, UINT32_C(0x4002), &error));
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
        semu_sapporo_iom4_snapshot_write(iom4, &writer, &error));
    SEMU_TEST_EQ_U64(context, 0u, (uint64_t)writer.size);
    semu_snapshot_writer_destroy(&writer);
    /* Reset clears the command; the same state now saves and loads. */
    semu_sapporo_iom4_reset(iom4);
    semu_snapshot_writer_init(&writer);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_iom4_snapshot_write(iom4, &writer, &error));
    SEMU_TEST_ASSERT(context, writer.size > 11u);
    /* Forged loaded-DMA flag (byte 10 of the device image). */
    writer.data[10u] = 1u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_sapporo_iom4_snapshot_read(iom4, &reader, &error));
    /* Forged FIFO count (byte 3). */
    writer.data[10u] = 0u;
    writer.data[3u] = (uint8_t)FIFO_WORDS;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_sapporo_iom4_snapshot_read(iom4, &reader, &error));
    /* The uncorrupted image loads back. */
    writer.data[3u] = 0u;
    semu_snapshot_reader_init(&reader, writer.data, writer.size);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_iom4_snapshot_read(iom4, &reader, &error));
    SEMU_TEST_ASSERT(context, semu_snapshot_reader_done(&reader));
    semu_snapshot_writer_destroy(&writer);
    semu_sapporo_iom4_destroy(iom4);
    semu_bus_destroy(bus);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_machine_roundtrip_and_rearm),
        SEMU_TEST_CASE(test_section_missing_and_truncated_refuse_atomically),
        SEMU_TEST_CASE(test_iom4_midcommand_refuses_both_directions)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
