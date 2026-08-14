#include "semu/manifest.h"
#include "semu/hash.h"

#include <string.h>

static const semu_component *find_component(
    const semu_firmware_manifest *manifest, const char *id)
{
    size_t index;
    for (index = 0u; index < manifest->component_count; ++index) {
        if (strcmp(id, manifest->components[index].id) == 0) {
            return &manifest->components[index];
        }
    }
    return NULL;
}

static int metadata_matches(const semu_component *required,
                            const semu_component *component)
{
    return component != NULL && strcmp(required->role, component->role) == 0 &&
           required->load_address == component->load_address &&
           required->size == component->size &&
           memcmp(required->sha256, component->sha256,
                  SEMU_SHA256_SIZE) == 0;
}

semu_status semu_manifest_validate(const semu_profile *profile,
                                   const semu_firmware_manifest *manifest,
                                   semu_error *error)
{
    size_t required_index;
    if (profile == NULL || manifest == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "profile and manifest required");
        return SEMU_ERR_ARGUMENT;
    }
    if (profile->format != 1u || manifest->format != profile->format ||
        strcmp(profile->product, manifest->product) != 0 ||
        strcmp(profile->version, manifest->version) != 0 ||
        profile->required_count != manifest->component_count) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "firmware does not match profile");
        return SEMU_ERR_CONFLICT;
    }
    for (required_index = 0u; required_index < profile->required_count;
         ++required_index) {
        const semu_component *required = &profile->required[required_index];
        const semu_component *component = find_component(manifest, required->id);
        uint8_t digest[SEMU_SHA256_SIZE];
        uint64_t actual_size;
        semu_status status;

        if (!metadata_matches(required, component)) {
            semu_error_set(error, SEMU_ERR_CONFLICT,
                           "component %s does not match profile", required->id);
            return SEMU_ERR_CONFLICT;
        }
        status = semu_sha256_file(component->path, digest, &actual_size, error);
        if (status != SEMU_OK) {
            return status;
        }
        if (actual_size != component->size ||
            memcmp(digest, component->sha256, SEMU_SHA256_SIZE) != 0) {
            semu_error_set(error, SEMU_ERR_CONFLICT,
                           "component %s bytes do not match manifest", component->id);
            return SEMU_ERR_CONFLICT;
        }
    }
    semu_error_clear(error);
    return SEMU_OK;
}
