#ifndef SEMU_SAPPORO_OHR2_H
#define SEMU_SAPPORO_OHR2_H

#include "semu/peripheral.h"
#include "../core/snapshot_io.h"

#define SEMU_SAPPORO_OHR2_ADDRESS 0x10u
#define SEMU_SAPPORO_OHR2_REQUEST_SIZE 59u
#define SEMU_SAPPORO_OHR2_RESPONSE_SIZE 58u
#define SEMU_SAPPORO_OHR2_PAYLOAD_SIZE 54u
#define SEMU_SAPPORO_OHR2_CRC_SIZE 4u
#define SEMU_SAPPORO_OHR2_RESPONSE_SELECTOR 0x3cu
#define SEMU_SAPPORO_OHR2_READY_SIGNAL 0u

typedef enum semu_sapporo_ohr2_command {
    SEMU_SAPPORO_OHR2_COMMAND_IDENTITY = 0u,
    SEMU_SAPPORO_OHR2_COMMAND_CONFIGURE = 1u,
    SEMU_SAPPORO_OHR2_COMMAND_REBOOT = 3u,
    SEMU_SAPPORO_OHR2_COMMAND_ECHO = 6u,
    SEMU_SAPPORO_OHR2_COMMAND_RESULT_13 = 13u,
    SEMU_SAPPORO_OHR2_COMMAND_RESULT_14 = 14u
} semu_sapporo_ohr2_command;

typedef enum semu_sapporo_ohr2_state {
    SEMU_SAPPORO_OHR2_BSL = 0,
    SEMU_SAPPORO_OHR2_MAIN
} semu_sapporo_ohr2_state;

typedef struct semu_sapporo_ohr2 semu_sapporo_ohr2;

/* The provider owns every response-body byte except command and sequence,
 * which the transport overwrites with the validated request header. */
typedef semu_transaction_result (*semu_sapporo_ohr2_body_provider_fn)(
    void *context, semu_sapporo_ohr2_command command, uint16_t sequence,
    semu_sapporo_ohr2_state state,
    const uint8_t request_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    uint8_t response_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    semu_error *error);

semu_sapporo_ohr2 *semu_sapporo_ohr2_create(
    semu_peripheral_signal_fn ready_callback, void *ready_context,
    semu_sapporo_ohr2_body_provider_fn body_provider, void *body_context,
    semu_error *error);
void semu_sapporo_ohr2_destroy(semu_sapporo_ohr2 *device);
void semu_sapporo_ohr2_reset(semu_sapporo_ohr2 *device);
semu_serial_endpoint semu_sapporo_ohr2_endpoint(
    semu_sapporo_ohr2 *device);
uint32_t semu_sapporo_ohr2_crc32(const uint8_t *payload, size_t size);
semu_status semu_sapporo_ohr2_snapshot_write(
    const semu_sapporo_ohr2 *device, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_sapporo_ohr2_snapshot_read(
    semu_sapporo_ohr2 *device, semu_snapshot_reader *reader,
    semu_error *error);

#endif
