#ifndef SEMU_SAPPORO_239_FILES_INTERNAL_H
#define SEMU_SAPPORO_239_FILES_INTERNAL_H

#include "sapporo_239_files.h"

/* File handles start at the first stride so an unopened handle value is
   never a live handle (and never zero); unchanged since E-SAP-COMPAT-
   FILES-239-001, and the storage index space needs the handle pool. */
#define S239_FILE_HANDLE_BASE UINT32_C(0x1015f000)
#define S239_FILE_HANDLE_STRIDE UINT32_C(0x100)
#define S239_FILE_MAX_HANDLES 64u
#define S239_FILE_COUNT 12u
/* E-SAP239-REPO38D123-001: session-local slots for the admitted
   storage/<key>/data.jsn family (REPO persistence writes). File index
   space: 0..11 fixed table; 12.. are storage slots. The slot count is
   S239_FILE_MAX_HANDLES - 1 (63) so the derived file index
   S239_FILE_COUNT + slot stays inside the uint8 handle->file field. */
#define S239_STORAGE_SLOTS 63u
#define S239_STORAGE_NAME_MAX 65u /* Same capture budget as file paths. */
/* Storage per-file capacity. The original 34-byte law
   (E-SAP239-REPO38D123-001) is proven FLAT-SCOPE-ONLY: addendum C
   observed a single nested write requesting 87 bytes at cursor 0,
   and the observe-only sweep in addendum D shows the full session
   burst (nested 89, flat 96 final sizes, universal +2 commit tail)
   completing with zero refusals. Integrator ruling 2026-09-27
   (ticket 796 continuation; owner-approved option (a) WITH this
   derivation documented in place): the admitted capacity is the
   guest's OWN repository buffer bound, repo_read 0x00198b8a
   `movw r5, #0x40d` = 1037 bytes — the native serializer can never
   commit a record larger than its own buffer, so the refusal above
   1037 still means "beyond observed guest semantics" and the
   fail-closed law shape is preserved. A writer-side per-key
   serialization census is an OPTIONAL refinement, not a
   prerequisite (recorded owner decision). */
#define S239_STORAGE_CAPACITY 1037u
/* Fixed-table runtime capacity: partition sizes in the table are
   INITIAL content sizes; the guest appends (observed: zapp/
   storage.sbm grows past 64 and is refused at 64 @1129953745 —
   guest-normal growth). The admission ceiling is the same serializer
   buffer bound 1037 (E-SAP239-REPO38D123-001 addenda C/D, owner
   ruling (a)); larger pinned data files keep their own size as the
   bound. capacity = max(table_size, 1037). */
#define S239_FILE_RUNTIME_CAPACITY 1037u
#define S239_STORAGE_PREFIX "storage/"
#define S239_STORAGE_SUFFIX "/data.jsn"

typedef struct s239_file_slot {
    uint8_t *data;
    size_t size;
    int present;
} s239_file_slot;

typedef struct s239_storage_slot {
    char name[S239_STORAGE_NAME_MAX];
    uint8_t *data;
    size_t size;
    int present;
} s239_storage_slot;

typedef struct s239_handle_slot {
    uint32_t value;
    uint32_t cursor;
    uint8_t file;
    uint8_t mode;
    int active;
} s239_handle_slot;

/* File index space: 0..S239_FILE_COUNT-1 are the fixed table paths;
   index S239_FILE_COUNT+i is storage slot i. */
#define S239_FILE_INDEX_STORAGE UINT32_C(S239_FILE_COUNT)

/* The derived storage index passes through the uint8 handle->file field
   on its way into the uint32 operate_file local, so the slot table must
   stop below an index overflow (63 + 12 = 75). */
typedef char s239_storage_index_fits_uint8_check[
    (S239_FILE_COUNT + S239_STORAGE_SLOTS <= UINT32_C(255)) ? 1 : -1];

struct semu_sapporo_239_files {
    s239_file_slot files[S239_FILE_COUNT];
    s239_handle_slot handles[S239_FILE_MAX_HANDLES];
    s239_storage_slot storage[S239_STORAGE_SLOTS];
    /* Names are immutable once admitted (admit is atomic with its
       intervention gate, so the table is append-only) and bounded by
       the handle pool size. */
    uint32_t storage_names;
    uint32_t next_handle;
};

extern const char *const semu_s239_file_paths[S239_FILE_COUNT];
extern const size_t semu_s239_file_capacities[S239_FILE_COUNT];

int semu_s239_file_index(const char *path);
/* Nonzero when path matches storage/<hex>[/<hex>...]/data.jsn exactly
   (flat single-key or nested hex-segment forms; E-SAP239-REFUSED-PATH-
   001 capture). */
int semu_s239_storage_path(const char *path);
/* File index of an already-present storage slot, or -1. */
int semu_s239_storage_index(const semu_sapporo_239_files *files,
                            const char *path);
/* Admit a shape-valid storage path into a free slot. Returns the file
   index (>= S239_FILE_INDEX_STORAGE), -1 when no slot is free, or
   SEMU_ERR_STATE (as int) when the shape is invalid. */
int semu_s239_storage_admit(semu_sapporo_239_files *files, const char *path);
size_t semu_s239_file_capacity(const semu_sapporo_239_files *files,
                               uint32_t file_index);
int semu_s239_slot_present(const semu_sapporo_239_files *files,
                           uint32_t file_index);
void semu_s239_slot_name(const semu_sapporo_239_files *files,
                         uint32_t file_index, char out[65]);
s239_handle_slot *semu_s239_find_handle(semu_sapporo_239_files *files,
                                        uint32_t value);

#endif
