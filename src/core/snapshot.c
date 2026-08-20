/*
 * Versioned machine snapshots (ticket 615).
 * Atomic serialize/restore with magic, version, identity binding,
 * and named binary sections.  Explicit little-endian, checked sizes.
 * No pointer serialization, compression, or private firmware bytes.
 */

#include "semu/trace.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t id;
    size_t size;
    uint8_t *data;
} semu_snapshot_section;

struct semu_snapshot {
    char profile_id[SEMU_ID_MAX];
    char firmware_hash[SEMU_REPLAY_HASH_HEX_LEN];
    semu_snapshot_section sections[SEMU_SNAPSHOT_MAX_SECTIONS];
    size_t section_count;
};

semu_snapshot *semu_snapshot_create(semu_error *error)
{
    semu_snapshot *s = (semu_snapshot *)calloc(1u, sizeof(*s));
    if (s == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "snapshot: cannot allocate");
        return NULL;
    }
    return s;
}

void semu_snapshot_destroy(semu_snapshot *snap)
{
    if (snap != NULL) {
        size_t i;
        for (i = 0u; i < snap->section_count; ++i) {
            free(snap->sections[i].data);
        }
        free(snap);
    }
}

void semu_snapshot_reset(semu_snapshot *snap)
{
    size_t i;
    if (snap == NULL) {
        return;
    }
    for (i = 0u; i < snap->section_count; ++i) {
        free(snap->sections[i].data);
        snap->sections[i].data = NULL;
        snap->sections[i].size = 0u;
        snap->sections[i].id = 0u;
    }
    snap->section_count = 0u;
    snap->profile_id[0] = '\0';
    snap->firmware_hash[0] = '\0';
}

size_t semu_snapshot_section_count(const semu_snapshot *snap)
{
    return snap != NULL ? snap->section_count : 0u;
}

const char *semu_snapshot_profile_id(const semu_snapshot *snap)
{
    return snap != NULL ? snap->profile_id : NULL;
}

const char *semu_snapshot_firmware_hash(const semu_snapshot *snap)
{
    return snap != NULL ? snap->firmware_hash : NULL;
}

uint32_t semu_snapshot_version(const semu_snapshot *snap)
{
    (void)snap;
    return SEMU_SNAPSHOT_VERSION;
}

static int is_lower_hex_hash(const char *value, size_t length)
{
    size_t index;

    if (value == NULL || length != SEMU_SHA256_SIZE * 2u) {
        return 0;
    }
    for (index = 0u; index < length; ++index) {
        char digit = value[index];
        if (!((digit >= '0' && digit <= '9') ||
              (digit >= 'a' && digit <= 'f'))) {
            return 0;
        }
    }
    return 1;
}

semu_status semu_snapshot_set_identity(semu_snapshot *snap,
    const char *profile_id, const char *firmware_hash,
    semu_error *error)
{
    size_t plen, flen;
    if (snap == NULL || profile_id == NULL || firmware_hash == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "snapshot: null identity");
        return SEMU_ERR_ARGUMENT;
    }
    plen = strlen(profile_id);
    flen = strlen(firmware_hash);
    if (plen == 0u || plen >= SEMU_ID_MAX) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "snapshot: bad profile id");
        return SEMU_ERR_ARGUMENT;
    }
    if (flen != SEMU_SHA256_SIZE * 2u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
            "snapshot: bad firmware hash length");
        return SEMU_ERR_ARGUMENT;
    }
    if (!is_lower_hex_hash(firmware_hash, flen)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
            "snapshot: firmware hash not lowercase hex");
        return SEMU_ERR_ARGUMENT;
    }
    memcpy(snap->profile_id, profile_id, plen);
    snap->profile_id[plen] = '\0';
    memcpy(snap->firmware_hash, firmware_hash, flen);
    snap->firmware_hash[flen] = '\0';
    return SEMU_OK;
}

static semu_snapshot_section *find_section(semu_snapshot *snap,
    uint32_t id)
{
    size_t i;
    for (i = 0u; i < snap->section_count; ++i) {
        if (snap->sections[i].id == id) {
            return &snap->sections[i];
        }
    }
    return NULL;
}

semu_status semu_snapshot_write_section(semu_snapshot *snap,
    uint32_t section_id, const uint8_t *data, size_t size,
    semu_error *error)
{
    semu_snapshot_section *sec;
    uint8_t *copy;

    if (snap == NULL || (data == NULL && size > 0u)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "snapshot: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    if (size > SEMU_SNAPSHOT_MAX_SECTION_SIZE) {
        semu_error_set(error, SEMU_ERR_RANGE, "snapshot: section too large");
        return SEMU_ERR_RANGE;
    }
    sec = find_section(snap, section_id);
    if (sec == NULL) {
        if (snap->section_count >= SEMU_SNAPSHOT_MAX_SECTIONS) {
            semu_error_set(error, SEMU_ERR_RANGE,
                "snapshot: too many sections");
            return SEMU_ERR_RANGE;
        }
        sec = &snap->sections[snap->section_count];
        sec->id = section_id;
        sec->data = NULL;
        sec->size = 0u;
    } else {
        free(sec->data);
        sec->data = NULL;
        sec->size = 0u;
    }
    if (size > 0u) {
        copy = (uint8_t *)malloc(size);
        if (copy == NULL) {
            semu_error_set(error, SEMU_ERR_NOMEM,
                "snapshot: cannot allocate section");
            return SEMU_ERR_NOMEM;
        }
        memcpy(copy, data, size);
    } else {
        copy = NULL;
    }
    sec->data = copy;
    sec->size = size;
    if (sec == &snap->sections[snap->section_count]) {
        ++snap->section_count;
    }
    return SEMU_OK;
}

semu_status semu_snapshot_read_section(const semu_snapshot *snap,
    uint32_t section_id, const uint8_t **out_data, size_t *out_size)
{
    size_t i;
    if (snap == NULL) {
        return SEMU_ERR_ARGUMENT;
    }
    for (i = 0u; i < snap->section_count; ++i) {
        if (snap->sections[i].id == section_id) {
            if (out_data != NULL) {
                *out_data = snap->sections[i].data;
            }
            if (out_size != NULL) {
                *out_size = snap->sections[i].size;
            }
            return SEMU_OK;
        }
    }
    return SEMU_ERR_RANGE;
}

static void put_u32le(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)val;
    buf[1] = (uint8_t)(val >> 8);
    buf[2] = (uint8_t)(val >> 16);
    buf[3] = (uint8_t)(val >> 24);
}

static void put_u64le(uint8_t *buf, uint64_t val)
{
    put_u32le(buf, (uint32_t)val);
    put_u32le(buf + 4u, (uint32_t)(val >> 32));
}

static uint32_t get_u32le(const uint8_t *buf)
{
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

static uint64_t get_u64le(const uint8_t *buf)
{
    return (uint64_t)get_u32le(buf) | ((uint64_t)get_u32le(buf + 4u) << 32);
}

size_t semu_snapshot_serialize(const semu_snapshot *snap,
    uint8_t *buf, size_t buf_size)
{
    size_t offset = 0u;
    size_t i;
    size_t profile_len;
    size_t hash_len;

    if (buf == NULL || buf_size == 0u || snap == NULL) {
        return 0u;
    }
    profile_len = strlen(snap->profile_id);
    hash_len = strlen(snap->firmware_hash);

    if (offset + 4u + 4u + SEMU_ID_MAX + SEMU_REPLAY_HASH_HEX_LEN +
        4u > buf_size) {
        return 0u;
    }
    put_u32le(buf + offset, SEMU_SNAPSHOT_MAGIC);
    offset += 4u;
    put_u32le(buf + offset, SEMU_SNAPSHOT_VERSION);
    offset += 4u;
    memset(buf + offset, 0, SEMU_ID_MAX);
    memcpy(buf + offset, snap->profile_id, profile_len);
    offset += SEMU_ID_MAX;
    memset(buf + offset, 0, SEMU_REPLAY_HASH_HEX_LEN);
    memcpy(buf + offset, snap->firmware_hash, hash_len);
    offset += SEMU_REPLAY_HASH_HEX_LEN;
    put_u32le(buf + offset, (uint32_t)snap->section_count);
    offset += 4u;

    for (i = 0u; i < snap->section_count; ++i) {
        const semu_snapshot_section *sec = &snap->sections[i];
        if (offset + 4u + 8u + sec->size > buf_size) {
            return 0u;
        }
        put_u32le(buf + offset, sec->id);
        offset += 4u;
        put_u64le(buf + offset, (uint64_t)sec->size);
        offset += 8u;
        if (sec->size > 0u) {
            memcpy(buf + offset, sec->data, sec->size);
            offset += sec->size;
        }
    }
    return offset;
}

semu_status semu_snapshot_deserialize(semu_snapshot *snap,
    const uint8_t *buf, size_t buf_size, semu_error *error)
{
    size_t offset = 0u;
    uint32_t magic;
    uint32_t version;
    uint32_t scount;
    size_t i;
    semu_snapshot_section temp_sections[SEMU_SNAPSHOT_MAX_SECTIONS];
    char temp_profile[SEMU_ID_MAX];
    char temp_hash[SEMU_REPLAY_HASH_HEX_LEN];
    size_t temp_count = 0u;
    semu_status failure_status = SEMU_ERR_FORMAT;

    if (snap == NULL || buf == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "snapshot: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    memset(temp_sections, 0, sizeof(temp_sections));
    memset(temp_profile, 0, sizeof(temp_profile));
    memset(temp_hash, 0, sizeof(temp_hash));

    /* Minimum header size: magic + version + profile + hash + count */
    if (buf_size < 4u + 4u + SEMU_ID_MAX + SEMU_REPLAY_HASH_HEX_LEN + 4u) {
        semu_error_set(error, SEMU_ERR_FORMAT, "snapshot: truncated header");
        return SEMU_ERR_FORMAT;
    }
    magic = get_u32le(buf + offset);
    offset += 4u;
    if (magic != SEMU_SNAPSHOT_MAGIC) {
        semu_error_set(error, SEMU_ERR_FORMAT, "snapshot: bad magic");
        return SEMU_ERR_FORMAT;
    }
    version = get_u32le(buf + offset);
    offset += 4u;
    if (version != SEMU_SNAPSHOT_VERSION) {
        semu_error_set(error, SEMU_ERR_FORMAT,
            "snapshot: unsupported version %u", version);
        return SEMU_ERR_FORMAT;
    }
    memcpy(temp_profile, buf + offset, SEMU_ID_MAX);
    temp_profile[SEMU_ID_MAX - 1u] = '\0';
    offset += SEMU_ID_MAX;
    memcpy(temp_hash, buf + offset, SEMU_REPLAY_HASH_HEX_LEN);
    temp_hash[SEMU_REPLAY_HASH_HEX_LEN - 1u] = '\0';
    offset += SEMU_REPLAY_HASH_HEX_LEN;
    if (!is_lower_hex_hash(temp_hash, SEMU_SHA256_SIZE * 2u)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
            "snapshot: firmware hash not lowercase hex");
        return SEMU_ERR_FORMAT;
    }
    scount = get_u32le(buf + offset);
    offset += 4u;
    if (scount > SEMU_SNAPSHOT_MAX_SECTIONS) {
        semu_error_set(error, SEMU_ERR_FORMAT,
            "snapshot: too many sections %u", scount);
        return SEMU_ERR_FORMAT;
    }

    for (i = 0u; i < scount; ++i) {
        uint32_t sid;
        uint64_t slen;
        if (offset > buf_size || 4u + 8u > buf_size - offset) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "snapshot: truncated section header");
            goto fail;
        }
        sid = get_u32le(buf + offset);
        offset += 4u;
        slen = get_u64le(buf + offset);
        offset += 8u;
        if (slen > SEMU_SNAPSHOT_MAX_SECTION_SIZE) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "snapshot: section too large");
            goto fail;
        }
        if (slen > (uint64_t)SIZE_MAX ||
            (size_t)slen > buf_size - offset) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "snapshot: truncated section data");
            goto fail;
        }
        for (size_t previous = 0u; previous < temp_count; ++previous) {
            if (temp_sections[previous].id == sid) {
                semu_error_set(error, SEMU_ERR_FORMAT,
                               "snapshot: duplicate section %u", sid);
                goto fail;
            }
        }
        temp_sections[temp_count].id = sid;
        temp_sections[temp_count].size = (size_t)slen;
        if (slen > 0u) {
            temp_sections[temp_count].data =
                (uint8_t *)malloc((size_t)slen);
            if (temp_sections[temp_count].data == NULL) {
                semu_error_set(error, SEMU_ERR_NOMEM,
                    "snapshot: cannot allocate section");
                failure_status = SEMU_ERR_NOMEM;
                goto fail;
            }
            memcpy(temp_sections[temp_count].data, buf + offset,
                   (size_t)slen);
        } else {
            temp_sections[temp_count].data = NULL;
        }
        ++temp_count;
        offset += (size_t)slen;
    }
    if (offset != buf_size) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "snapshot: trailing image data");
        goto fail;
    }

    /* Entire image validated; now commit atomically. */
    semu_snapshot_reset(snap);
    memcpy(snap->profile_id, temp_profile, sizeof(temp_profile));
    memcpy(snap->firmware_hash, temp_hash, sizeof(temp_hash));
    for (i = 0u; i < temp_count; ++i) {
        snap->sections[i] = temp_sections[i];
    }
    snap->section_count = temp_count;
    return SEMU_OK;

fail:
    for (i = 0u; i < temp_count; ++i) {
        free(temp_sections[i].data);
    }
    return failure_status;
}
