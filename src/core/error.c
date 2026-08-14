#include "semu/types.h"

#include <stdarg.h>
#include <stdio.h>

void semu_error_clear(semu_error *error)
{
    if (error == NULL) {
        return;
    }
    error->code = SEMU_OK;
    error->text[0] = '\0';
}

void semu_error_set(semu_error *error, semu_status code, const char *format, ...)
{
    va_list arguments;

    if (error == NULL) {
        return;
    }
    error->code = code;
    if (format == NULL) {
        error->text[0] = '\0';
        return;
    }
    va_start(arguments, format);
    (void)vsnprintf(error->text, sizeof(error->text), format, arguments);
    va_end(arguments);
}

const char *semu_status_name(semu_status status)
{
    switch (status) {
    case SEMU_OK: return "ok";
    case SEMU_ERR_ARGUMENT: return "argument";
    case SEMU_ERR_IO: return "io";
    case SEMU_ERR_FORMAT: return "format";
    case SEMU_ERR_RANGE: return "range";
    case SEMU_ERR_CONFLICT: return "conflict";
    case SEMU_ERR_UNSUPPORTED: return "unsupported";
    case SEMU_ERR_STATE: return "state";
    case SEMU_ERR_NOMEM: return "nomem";
    default: return "unknown";
    }
}

const char *semu_stop_reason_name(semu_stop_reason reason)
{
    switch (reason) {
    case SEMU_STOP_NONE: return "none";
    case SEMU_STOP_HALT: return "halt";
    case SEMU_STOP_BUDGET: return "budget";
    case SEMU_STOP_WFI_DEADLOCK: return "wfi-deadlock";
    case SEMU_STOP_UNMAPPED_ACCESS: return "unmapped-access";
    case SEMU_STOP_UNSUPPORTED_INSTRUCTION: return "unsupported-instruction";
    case SEMU_STOP_DEVICE_REFUSED: return "device-refused";
    case SEMU_STOP_FIRMWARE_ASSERT: return "firmware-assert";
    case SEMU_STOP_COMPAT_REFUSED: return "compat-refused";
    case SEMU_STOP_USER: return "user";
    default: return "unknown";
    }
}
