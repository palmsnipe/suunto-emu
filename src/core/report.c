/*
 * Fault report formatting (ticket 605).
 * Stable text output for CPU faults with optional trace history.
 * No host paths, wall time, or raw firmware bytes.
 */

#include "semu/trace.h"

#include <string.h>

static size_t rep_str(char *buf, size_t buf_size, size_t offset,
    const char *str)
{
    size_t len = strlen(str);
    size_t i;
    for (i = 0u; i < len && offset + i + 1u < buf_size; ++i) {
        buf[offset + i] = str[i];
    }
    return offset + len;
}

static size_t rep_u64(char *buf, size_t buf_size, size_t offset,
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

static size_t rep_hex32(char *buf, size_t buf_size, size_t offset,
    uint32_t value)
{
    static const char digits[] = "0123456789abcdef";
    char tmp[8u];
    size_t i;
    for (i = 0u; i < 8u; ++i) {
        tmp[7u - i] = digits[value & 0xfu];
        value >>= 4;
    }
    for (i = 0u; i < 8u; ++i) {
        if (offset + i + 1u < buf_size) {
            buf[offset + i] = tmp[i];
        }
    }
    return offset + 8u;
}

static size_t rep_field_str(char *buf, size_t buf_size, size_t offset,
    const char *key, const char *value)
{
    offset = rep_str(buf, buf_size, offset, key);
    offset = rep_str(buf, buf_size, offset, "=");
    offset = rep_str(buf, buf_size, offset, value);
    offset = rep_str(buf, buf_size, offset, "\n");
    return offset;
}

static size_t rep_field_hex(char *buf, size_t buf_size, size_t offset,
    const char *key, uint32_t value)
{
    offset = rep_str(buf, buf_size, offset, key);
    offset = rep_str(buf, buf_size, offset, "=0x");
    offset = rep_hex32(buf, buf_size, offset, value);
    offset = rep_str(buf, buf_size, offset, "\n");
    return offset;
}

static size_t rep_field_u64(char *buf, size_t buf_size, size_t offset,
    const char *key, uint64_t value)
{
    offset = rep_str(buf, buf_size, offset, key);
    offset = rep_str(buf, buf_size, offset, "=");
    offset = rep_u64(buf, buf_size, offset, value);
    offset = rep_str(buf, buf_size, offset, "\n");
    return offset;
}

static const char *stop_name(semu_stop_reason reason)
{
    switch (reason) {
    case SEMU_STOP_NONE:                   return "none";
    case SEMU_STOP_HALT:                   return "halt";
    case SEMU_STOP_BUDGET:                 return "budget";
    case SEMU_STOP_WFI_DEADLOCK:           return "wfi-deadlock";
    case SEMU_STOP_UNMAPPED_ACCESS:        return "unmapped-access";
    case SEMU_STOP_UNSUPPORTED_INSTRUCTION: return "unsupported-instruction";
    case SEMU_STOP_DEVICE_REFUSED:         return "device-refused";
    case SEMU_STOP_FIRMWARE_ASSERT:        return "firmware-assert";
    case SEMU_STOP_COMPAT_REFUSED:         return "compat-refused";
    case SEMU_STOP_USER:                   return "user";
    default:                                return "unknown";
    }
}

size_t semu_report_fault_format(const semu_report_fault *report,
    const semu_trace *trace, char *buf, size_t buf_size)
{
    size_t offset = 0u;
    uint32_t i;

    if (buf == NULL || buf_size == 0u || report == NULL) {
        if (buf != NULL && buf_size > 0u) {
            buf[0] = '\0';
        }
        return 0u;
    }
    buf[0] = '\0';

    offset = rep_field_str(buf, buf_size, offset, "stop",
                           stop_name(report->stop_reason));
    offset = rep_field_hex(buf, buf_size, offset, "pc", report->r[15]);
    offset = rep_field_hex(buf, buf_size, offset, "fault_instruction",
                           report->fault_instruction);
    if (report->has_fault_address) {
        offset = rep_field_hex(buf, buf_size, offset, "fault_address",
                               report->fault_address);
    } else {
        offset = rep_field_str(buf, buf_size, offset, "fault_address",
                               "none");
    }
    for (i = 0u; i < 16u; ++i) {
        char key[4u];
        key[0] = 'r';
        if (i >= 10u) {
            key[1] = '1';
            key[2] = (char)('0' + (int)(i - 10u));
            key[3] = '\0';
        } else {
            key[1] = (char)('0' + (int)i);
            key[2] = '\0';
        }
        offset = rep_field_hex(buf, buf_size, offset, key, report->r[i]);
    }
    offset = rep_field_hex(buf, buf_size, offset, "xpsr", report->xpsr);
    offset = rep_field_hex(buf, buf_size, offset, "primask",
                           report->primask);
    offset = rep_field_hex(buf, buf_size, offset, "basepri",
                           report->basepri);
    offset = rep_field_hex(buf, buf_size, offset, "faultmask",
                           report->faultmask);
    offset = rep_field_hex(buf, buf_size, offset, "control",
                           report->control);
    offset = rep_field_hex(buf, buf_size, offset, "fpscr", report->fpscr);
    offset = rep_field_u64(buf, buf_size, offset, "instructions",
                           report->instructions);

    if (trace != NULL && semu_trace_count(trace) > 0u) {
        char trace_buf[8192u];
        size_t trace_len;
        offset = rep_str(buf, buf_size, offset, "--- trace ---\n");
        trace_len = semu_trace_format(trace, trace_buf, sizeof(trace_buf));
        if (offset + trace_len + 1u < buf_size) {
            memcpy(buf + offset, trace_buf, trace_len);
            offset += trace_len;
        } else {
            size_t avail = buf_size - 1u - offset;
            size_t copy = trace_len < avail ? trace_len : avail;
            memcpy(buf + offset, trace_buf, copy);
            offset += copy;
        }
    }

    if (offset < buf_size) {
        buf[offset] = '\0';
    } else {
        buf[buf_size - 1u] = '\0';
        offset = buf_size - 1u;
    }
    return offset;
}
