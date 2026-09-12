/*
 * Ulsan 2.35.36 NVM OTP INFO1 observed reads (ticket 730, E-ULS-0012).
 *
 * See ulsan_otpinfo.h: the lane has no device in the NVM_OTP/INFO1 tag
 * range and its sysbus log lines record the two boot reads at
 * 0x42003240 and 0x42003310 as returning 0x00000000 (tag
 * NVM_OTP/INFO1, reader PC 0x000c056c). These are the only NVM_OTP
 * accesses observed anywhere in the boot-phase scratch trace; serving
 * anything else would be a guess, so this device answers exactly these
 * two words with the logged zero and refuses everything else, including
 * writes (the lane log shows no OTP write attempts).
 */

#include "ulsan_otpinfo.h"

#include "semu/types.h"

enum {
    OFFSET_INFO1_3240 = 0x240u,
    OFFSET_INFO1_3310 = 0x310u
};

static semu_status otpinfo_read(void *context, uint32_t offset,
                                unsigned width, uint32_t *value,
                                semu_error *error)
{
    (void)context;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan OTP info read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Ulsan OTP info read at 0x%08x width %u is "
                       "unsupported", offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    if (offset == OFFSET_INFO1_3240 || offset == OFFSET_INFO1_3310) {
        *value = 0u; /* the value the lane sysbus log records */
        return SEMU_OK;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan OTP info read at 0x%08x is unsupported", offset);
    return SEMU_ERR_UNSUPPORTED;
}

static semu_status otpinfo_write(void *context, uint32_t offset,
                                 unsigned width, uint32_t value,
                                 semu_error *error)
{
    (void)context;
    (void)value;
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Ulsan OTP info write at 0x%08x width %u is unsupported",
                   offset, width);
    return SEMU_ERR_UNSUPPORTED;
}

static void otpinfo_reset(void *context)
{
    (void)context;
}

static const semu_bus_device_ops otpinfo_ops = {
    otpinfo_read,
    otpinfo_write,
    otpinfo_reset
};

static int otpinfo_context;

semu_status semu_ulsan_otpinfo_map(semu_bus *bus, semu_error *error)
{
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Ulsan OTP info needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    return semu_bus_map_device(bus, "ulsan.otp_info1", 0x42003000u, 0x400u,
                               &otpinfo_ops, &otpinfo_context, error);
}
