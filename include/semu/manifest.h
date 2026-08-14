#ifndef SEMU_MANIFEST_H
#define SEMU_MANIFEST_H

#include "semu/types.h"

#define SEMU_MAX_COMPONENTS 8u
#define SEMU_MAX_LAYERS 16u

typedef struct semu_component {
    char id[SEMU_ID_MAX];
    char role[SEMU_ID_MAX];
    char path[SEMU_PATH_MAX];
    uint32_t load_address;
    uint64_t size;
    uint8_t sha256[SEMU_SHA256_SIZE];
} semu_component;

typedef struct semu_firmware_manifest {
    unsigned format;
    char product[SEMU_ID_MAX];
    char version[SEMU_ID_MAX];
    semu_component components[SEMU_MAX_COMPONENTS];
    size_t component_count;
} semu_firmware_manifest;

typedef struct semu_profile {
    unsigned format;
    char id[SEMU_ID_MAX];
    char board[SEMU_ID_MAX];
    char product[SEMU_ID_MAX];
    char version[SEMU_ID_MAX];
    uint32_t vector_table;
    uint32_t display_width;
    uint32_t display_height;
    semu_component required[SEMU_MAX_COMPONENTS];
    size_t required_count;
    char layers[SEMU_MAX_LAYERS][SEMU_ID_MAX];
    size_t layer_count;
} semu_profile;

semu_status semu_manifest_load(const char *path, semu_firmware_manifest *manifest,
                               semu_error *error);
semu_status semu_profile_load(const char *path, semu_profile *profile,
                              semu_error *error);
semu_status semu_manifest_validate(const semu_profile *profile,
                                   const semu_firmware_manifest *manifest,
                                   semu_error *error);

#endif
