#include "machine_internal.h"

#include "../core/snapshot_io.h"
#include "semu/hash.h"

semu_status semu_machine_snapshot_manifest_hash(
    const semu_firmware_manifest *firmware,
    char output[SEMU_REPLAY_HASH_HEX_LEN], semu_error *error)
{
    semu_snapshot_writer writer;
    uint8_t digest[SEMU_SHA256_SIZE];
    size_t index;
    semu_status status = SEMU_OK;
#define W(call) do { status = (call); if (status != SEMU_OK) goto done; } while (0)
    if (firmware == NULL || output == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "manifest hash arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    semu_snapshot_writer_init(&writer);
    W(semu_snapshot_writer_u32(&writer, firmware->format, error));
    W(semu_snapshot_writer_bytes(&writer, (const uint8_t *)firmware->product,
                                 sizeof(firmware->product), error));
    W(semu_snapshot_writer_bytes(&writer, (const uint8_t *)firmware->version,
                                 sizeof(firmware->version), error));
    W(semu_snapshot_writer_u32(&writer, (uint32_t)firmware->component_count,
                               error));
    for (index = 0u; index < firmware->component_count; ++index) {
        const semu_component *component = &firmware->components[index];
        W(semu_snapshot_writer_bytes(&writer, (const uint8_t *)component->id,
                                     sizeof(component->id), error));
        W(semu_snapshot_writer_bytes(&writer,
                                     (const uint8_t *)component->role,
                                     sizeof(component->role), error));
        W(semu_snapshot_writer_u32(&writer, component->load_address, error));
        W(semu_snapshot_writer_u64(&writer, component->size, error));
        W(semu_snapshot_writer_bytes(&writer, component->sha256,
                                     sizeof(component->sha256), error));
    }
    semu_sha256(writer.data, writer.size, digest);
    semu_sha256_format(digest, output);
done:
    semu_snapshot_writer_destroy(&writer);
#undef W
    return status;
}
