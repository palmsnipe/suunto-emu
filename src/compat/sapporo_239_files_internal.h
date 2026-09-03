#ifndef SEMU_SAPPORO_239_FILES_INTERNAL_H
#define SEMU_SAPPORO_239_FILES_INTERNAL_H

#include "sapporo_239_files.h"

#define S239_FILE_HANDLE_BASE UINT32_C(0x1015f000)
#define S239_FILE_HANDLE_STRIDE UINT32_C(0x100)
#define S239_FILE_MAX_HANDLES 64u

typedef struct s239_file_slot {
    uint8_t *data;
    size_t size;
    int present;
} s239_file_slot;

typedef struct s239_handle_slot {
    uint32_t value;
    uint32_t cursor;
    uint8_t file;
    uint8_t mode;
    int active;
} s239_handle_slot;

struct semu_sapporo_239_files {
    s239_file_slot files[11];
    s239_handle_slot handles[S239_FILE_MAX_HANDLES];
    uint32_t next_handle;
};

extern const char *const semu_s239_file_paths[11];
extern const size_t semu_s239_file_capacities[11];

int semu_s239_file_index(const char *path);
s239_handle_slot *semu_s239_find_handle(semu_sapporo_239_files *files,
                                        uint32_t value);

#endif
