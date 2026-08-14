#include "semu/log.h"

static const char *level_name(semu_log_level level)
{
    switch (level) {
    case SEMU_LOG_TRACE: return "trace";
    case SEMU_LOG_DEBUG: return "debug";
    case SEMU_LOG_INFO: return "info";
    case SEMU_LOG_WARNING: return "warning";
    case SEMU_LOG_ERROR: return "error";
    default: return "unknown";
    }
}

void semu_log_init(semu_logger *logger, FILE *stream, semu_log_level minimum_level)
{
    if (logger == NULL) {
        return;
    }
    logger->stream = stream;
    logger->minimum_level = minimum_level;
    logger->virtual_time_ns = 0u;
}

void semu_log_set_time(semu_logger *logger, uint64_t virtual_time_ns)
{
    if (logger != NULL) {
        logger->virtual_time_ns = virtual_time_ns;
    }
}

void semu_log_vwrite(semu_logger *logger, semu_log_level level,
                     const char *subsystem, const char *event,
                     const char *format, va_list arguments)
{
    if (logger == NULL || logger->stream == NULL ||
        level < logger->minimum_level) {
        return;
    }
    (void)fprintf(logger->stream, "time_ns=%llu level=%s subsystem=%s event=%s",
                  (unsigned long long)logger->virtual_time_ns, level_name(level),
                  subsystem != NULL ? subsystem : "-",
                  event != NULL ? event : "-");
    if (format != NULL && format[0] != '\0') {
        (void)fputc(' ', logger->stream);
        (void)vfprintf(logger->stream, format, arguments);
    }
    (void)fputc('\n', logger->stream);
    (void)fflush(logger->stream);
}

void semu_log_write(semu_logger *logger, semu_log_level level,
                    const char *subsystem, const char *event,
                    const char *format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    semu_log_vwrite(logger, level, subsystem, event, format, arguments);
    va_end(arguments);
}
