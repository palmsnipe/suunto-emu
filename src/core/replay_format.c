/*
 * Replay formatting (ticket 610).
 * Stable byte-identical text output for versioned input replays.
 */

#include "semu/trace.h"

#include <string.h>

static size_t fmt_str(char *buf, size_t buf_size, size_t offset,
    const char *str)
{
    size_t len = strlen(str);
    size_t i;
    for (i = 0u; i < len && offset + i + 1u < buf_size; ++i) {
        buf[offset + i] = str[i];
    }
    return offset + len;
}

static size_t fmt_u64(char *buf, size_t buf_size, size_t offset,
    uint64_t value)
{
    char tmp[32u];
    size_t len = 0u;
    size_t i;
    if (value == 0u) {
        tmp[len++] = '0';
    } else {
        while (value > 0u && len < sizeof(tmp)) {
            tmp[len++] = (char)('0' + (int)(value % 10u));
            value /= 10u;
        }
    }
    for (i = 0u; i < len; ++i) {
        if (offset + i + 1u < buf_size) {
            buf[offset + i] = tmp[len - 1u - i];
        }
    }
    return offset + len;
}

static size_t fmt_i32(char *buf, size_t buf_size, size_t offset,
    int32_t value)
{
    if (value < 0) {
        offset = fmt_str(buf, buf_size, offset, "-");
        return fmt_u64(buf, buf_size, offset, (uint64_t)(-(int64_t)value));
    }
    return fmt_u64(buf, buf_size, offset, (uint64_t)value);
}

static size_t fmt_kv_str(char *buf, size_t buf_size, size_t offset,
    const char *key, const char *value)
{
    offset = fmt_str(buf, buf_size, offset, key);
    offset = fmt_str(buf, buf_size, offset, "=");
    offset = fmt_str(buf, buf_size, offset, value);
    offset = fmt_str(buf, buf_size, offset, "\n");
    return offset;
}

static size_t fmt_kv_u64(char *buf, size_t buf_size, size_t offset,
    const char *key, uint64_t value)
{
    offset = fmt_str(buf, buf_size, offset, key);
    offset = fmt_str(buf, buf_size, offset, "=");
    offset = fmt_u64(buf, buf_size, offset, value);
    offset = fmt_str(buf, buf_size, offset, "\n");
    return offset;
}

static const char *button_name(uint32_t code)
{
    switch (code) {
    case SEMU_BUTTON_UPPER: return "upper";
    case SEMU_BUTTON_MIDDLE: return "middle";
    case SEMU_BUTTON_LOWER: return "lower";
    default: return "unknown";
    }
}

size_t semu_replay_format(const semu_replay *replay,
    char *buf, size_t buf_size)
{
    size_t offset = 0u;
    size_t i;

    if (buf == NULL || buf_size == 0u) {
        return 0u;
    }
    buf[0] = '\0';
    if (replay == NULL) {
        return 0u;
    }

    offset = fmt_kv_u64(buf, buf_size, offset, "version",
        (uint64_t)replay->version);
    offset = fmt_kv_str(buf, buf_size, offset, "profile",
        replay->profile_id);
    offset = fmt_kv_str(buf, buf_size, offset, "firmware",
        replay->firmware_hash);
    offset = fmt_kv_u64(buf, buf_size, offset, "events",
        (uint64_t)replay->count);

    for (i = 0u; i < replay->count; ++i) {
        const semu_replay_event *ev = &replay->events[i];
        offset = fmt_u64(buf, buf_size, offset, ev->virtual_time_ns);
        switch (ev->kind) {
        case SEMU_INPUT_BUTTON:
            offset = fmt_str(buf, buf_size, offset, " button ");
            offset = fmt_str(buf, buf_size, offset, button_name(ev->code));
            offset = fmt_str(buf, buf_size, offset,
                ev->value == 0 ? " press" : " release");
            break;
        case SEMU_INPUT_CROWN_ROTATE:
            offset = fmt_str(buf, buf_size, offset, " crown rotate ");
            offset = fmt_i32(buf, buf_size, offset, ev->value);
            break;
        case SEMU_INPUT_TOUCH:
            offset = fmt_str(buf, buf_size, offset, " touch ");
            offset = fmt_i32(buf, buf_size, offset, ev->value);
            offset = fmt_str(buf, buf_size, offset, " ");
            offset = fmt_i32(buf, buf_size, offset, ev->x);
            offset = fmt_str(buf, buf_size, offset, " ");
            offset = fmt_i32(buf, buf_size, offset, ev->y);
            break;
        default:
            offset = fmt_str(buf, buf_size, offset, " unknown");
            break;
        }
        offset = fmt_str(buf, buf_size, offset, "\n");
    }

    if (offset < buf_size) {
        buf[offset] = '\0';
    } else {
        buf[buf_size - 1u] = '\0';
        offset = buf_size - 1u;
    }
    return offset;
}
