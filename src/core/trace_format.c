/*
 * Trace formatting (ticket 600).
 * Stable byte-identical output for identical event sequences.
 */

#include "semu/trace.h"

#include <stdio.h>
#include <string.h>

static const char *kind_name(uint32_t kind)
{
    switch (kind) {
    case SEMU_TRACE_KIND_INSTRUCTION: return "instruction";
    case SEMU_TRACE_KIND_MMIO_READ:    return "mmio-read";
    case SEMU_TRACE_KIND_MMIO_WRITE:   return "mmio-write";
    case SEMU_TRACE_KIND_DEVICE:       return "device";
    case SEMU_TRACE_KIND_COMPAT:       return "compat";
    case SEMU_TRACE_KIND_INPUT:        return "input";
    case SEMU_TRACE_KIND_FRAME:        return "frame";
    default:                            return "unknown";
    }
}

static size_t append_str(char *buf, size_t buf_size, size_t offset,
    const char *str)
{
    size_t len = strlen(str);
    size_t i;
    for (i = 0u; i < len && offset + i + 1u < buf_size; ++i) {
        buf[offset + i] = str[i];
    }
    return offset + len;
}

static size_t append_u64(char *buf, size_t buf_size, size_t offset,
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

size_t semu_trace_format(const semu_trace *trace,
    char *buf, size_t buf_size)
{
    size_t offset = 0u;
    size_t i;
    size_t count;

    if (buf == NULL || buf_size == 0u) {
        return 0u;
    }
    buf[0] = '\0';
    if (trace == NULL) {
        return 0u;
    }
    count = semu_trace_count(trace);
    for (i = 0u; i < count; ++i) {
        const semu_trace_record *rec = semu_trace_get(trace, i);
        if (rec == NULL) {
            break;
        }
        offset = append_str(buf, buf_size, offset, "v=");
        offset = append_u64(buf, buf_size, offset, rec->schema_version);
        offset = append_str(buf, buf_size, offset, " kind=");
        offset = append_str(buf, buf_size, offset, kind_name(rec->kind));
        offset = append_str(buf, buf_size, offset, " time=");
        offset = append_u64(buf, buf_size, offset, rec->virtual_time_ns);
        offset = append_str(buf, buf_size, offset, " seq=");
        offset = append_u64(buf, buf_size, offset, rec->sequence);
        offset = append_str(buf, buf_size, offset, " pc=0x");
        offset = append_u64(buf, buf_size, offset, rec->pc);
        offset = append_str(buf, buf_size, offset, " addr=0x");
        offset = append_u64(buf, buf_size, offset, rec->addr);
        offset = append_str(buf, buf_size, offset, " value=0x");
        offset = append_u64(buf, buf_size, offset, rec->value);
        offset = append_str(buf, buf_size, offset, " event=");
        offset = append_u64(buf, buf_size, offset, rec->event_code);
        offset = append_str(buf, buf_size, offset, " width=");
        offset = append_u64(buf, buf_size, offset, rec->width);
        offset = append_str(buf, buf_size, offset, "\n");
    }
    if (offset < buf_size) {
        buf[offset] = '\0';
    } else {
        buf[buf_size - 1u] = '\0';
        offset = buf_size - 1u;
    }
    return offset;
}
