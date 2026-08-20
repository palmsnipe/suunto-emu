#include "sapporo_ohr2.h"

#include "semu/hash.h"

#include <stdlib.h>
#include <string.h>

#define OHR2_DIAGNOSTIC_LIMIT 64u

struct semu_sapporo_ohr2 {
    semu_peripheral_signal_fn ready_callback;
    void *ready_context;
    semu_sapporo_ohr2_body_provider_fn body_provider;
    void *body_context;
    semu_sapporo_ohr2_state state;
    uint8_t queued_response[SEMU_SAPPORO_OHR2_RESPONSE_SIZE];
    uint16_t queued_sequence;
    int response_queued;
    int selector_armed;
    int ready;
    uint16_t expected_sequence;
    int sequence_initialized;
    semu_logger *logger;
    unsigned diagnostic_count;
};

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8u));
}

static void write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
}

static uint32_t read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
}

static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

uint32_t semu_sapporo_ohr2_crc32(const uint8_t *payload, size_t size)
{
    return semu_crc32(0u, payload, size);
}

static semu_transaction_result refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "OHR2 refuses %s", reason);
    return SEMU_TRANSACTION_REFUSE;
}

static const char *state_name(semu_sapporo_ohr2_state state)
{
    return state == SEMU_SAPPORO_OHR2_MAIN ? "MAIN" : "BSL";
}

static const char *result_name(semu_transaction_result result)
{
    switch (result) {
    case SEMU_TRANSACTION_OK: return "ok";
    case SEMU_TRANSACTION_WAIT: return "wait";
    case SEMU_TRANSACTION_REFUSE: return "refuse";
    default: return "unknown";
    }
}

static void log_transaction(semu_sapporo_ohr2 *device, const char *kind,
                             uint16_t command, uint16_t sequence,
                             semu_sapporo_ohr2_state state,
                             semu_transaction_result result)
{
    if (device == NULL || device->logger == NULL ||
        device->diagnostic_count >= OHR2_DIAGNOSTIC_LIMIT) {
        return;
    }
    ++device->diagnostic_count;
    semu_log_write(device->logger, SEMU_LOG_INFO, "ohr2", "ohr-transaction",
                   "kind=%s command=0x%04x sequence=%u state=%s status=%s "
                   "ready=%u",
                   kind != NULL ? kind : "unknown", (unsigned)command,
                   (unsigned)sequence, state_name(state),
                   result_name(result), (unsigned)(device->ready != 0));
}

static void log_ready(semu_sapporo_ohr2 *device, int level)
{
    if (device == NULL || device->logger == NULL ||
        device->diagnostic_count >= OHR2_DIAGNOSTIC_LIMIT) {
        return;
    }
    ++device->diagnostic_count;
    semu_log_write(device->logger, SEMU_LOG_INFO, "ohr2", "ohr-ready",
                   "state=%s level=%u", state_name(device->state),
                   (unsigned)(level != 0));
}

static void set_ready(semu_sapporo_ohr2 *device, int level)
{
    if (device->ready == level) {
        return;
    }
    device->ready = level;
    log_ready(device, level);
    if (device->ready_callback != NULL) {
        device->ready_callback(device->ready_context,
                               SEMU_SAPPORO_OHR2_READY_SIGNAL, level);
    }
}

static int valid_command_state(const semu_sapporo_ohr2 *device,
                               semu_sapporo_ohr2_command command)
{
    if (command == SEMU_SAPPORO_OHR2_COMMAND_IDENTITY) {
        return 1;
    }
    if (device->state == SEMU_SAPPORO_OHR2_BSL) {
        return command == SEMU_SAPPORO_OHR2_COMMAND_CONFIGURE ||
               command == SEMU_SAPPORO_OHR2_COMMAND_REBOOT;
    }
    return command == SEMU_SAPPORO_OHR2_COMMAND_ECHO ||
           command == SEMU_SAPPORO_OHR2_COMMAND_RESULT_13 ||
           command == SEMU_SAPPORO_OHR2_COMMAND_RESULT_14;
}

static int known_command(uint16_t command)
{
    return command == SEMU_SAPPORO_OHR2_COMMAND_IDENTITY ||
           command == SEMU_SAPPORO_OHR2_COMMAND_CONFIGURE ||
           command == SEMU_SAPPORO_OHR2_COMMAND_REBOOT ||
           command == SEMU_SAPPORO_OHR2_COMMAND_ECHO ||
           command == SEMU_SAPPORO_OHR2_COMMAND_RESULT_13 ||
           command == SEMU_SAPPORO_OHR2_COMMAND_RESULT_14;
}

static int sequence_allowed(const semu_sapporo_ohr2 *device,
                            uint16_t command, uint16_t sequence)
{
    /* E-SAP-OHR2-001: the Renode transport validates framing and CRC, then
       echoes the firmware's command/sequence fields.  It does not impose a
       monotonic sequence policy; startup legitimately repeats identity
       probes with sequence zero. */
    (void)device;
    (void)command;
    (void)sequence;
    return 1;
}

static semu_transaction_result queue_response(
    semu_sapporo_ohr2 *device, const uint8_t *request, uint16_t command,
    uint16_t sequence, semu_error *error)
{
    uint8_t body[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE];
    semu_transaction_result result;

    if (sequence == UINT16_MAX) {
        return refuse(error, "sequence counter overflow");
    }
    if (device->body_provider == NULL) {
        return refuse(error, "missing response body fixture");
    }
    result = device->body_provider(
        device->body_context, (semu_sapporo_ohr2_command)command, sequence,
        device->state, request, body, error);
    if (result != SEMU_TRANSACTION_OK) {
        return result;
    }
    write_u16(body, command);
    write_u16(body + 2u, sequence);
    (void)memcpy(device->queued_response, body, sizeof(body));
    write_u32(device->queued_response + SEMU_SAPPORO_OHR2_PAYLOAD_SIZE,
              semu_sapporo_ohr2_crc32(body, sizeof(body)));
    device->queued_sequence = sequence;
    device->expected_sequence = sequence;
    device->sequence_initialized = 1;
    device->response_queued = 1;
    device->selector_armed = 0;
    set_ready(device, 1);
    return SEMU_TRANSACTION_OK;
}

static semu_transaction_result accept_request(
    semu_sapporo_ohr2 *device, semu_serial_transaction *transaction,
    semu_error *error)
{
    const uint8_t *request = transaction->tx;
    uint16_t command;
    uint16_t sequence;

    if (transaction->tx_size != SEMU_SAPPORO_OHR2_REQUEST_SIZE ||
        transaction->rx_size != 0u) {
        return refuse(error, "request header or shape");
    }
    if (request[0] != 0u) {
        return refuse(error, "request header or shape");
    }
    if (read_u32(request + 55u) !=
        semu_sapporo_ohr2_crc32(request + 1u,
                                 SEMU_SAPPORO_OHR2_PAYLOAD_SIZE)) {
        return refuse(error, "request CRC");
    }
    command = read_u16(request + 1u);
    sequence = read_u16(request + 3u);
    if (sequence == UINT16_MAX) {
        return refuse(error, "sequence counter overflow");
    }
    if (!known_command(command)) {
        return refuse(error, "unknown command");
    }
    if (!sequence_allowed(device, command, sequence)) {
        return refuse(error, "request sequence");
    }
    if (!valid_command_state(device, (semu_sapporo_ohr2_command)command)) {
        return refuse(error, "command in current state");
    }
    if (device->response_queued) {
        return refuse(error, "queued response not consumed");
    }
    if (command == SEMU_SAPPORO_OHR2_COMMAND_REBOOT) {
        device->state = SEMU_SAPPORO_OHR2_MAIN;
        device->selector_armed = 0;
        set_ready(device, 0);
        return SEMU_TRANSACTION_OK;
    }
    return queue_response(device, request + 1u, command, sequence, error);
}

static semu_transaction_result consume_response(
    semu_sapporo_ohr2 *device, uint8_t *output, semu_error *error)
{
    if (!device->response_queued || !device->selector_armed) {
        return refuse(error, "read without selected queued response");
    }
    if (!device->sequence_initialized ||
        read_u16(device->queued_response + 2u) != device->expected_sequence ||
        device->queued_sequence != device->expected_sequence) {
        return refuse(error, "queued response sequence");
    }
    if (device->expected_sequence == UINT16_MAX) {
        return refuse(error, "sequence counter overflow");
    }
    (void)memcpy(output, device->queued_response,
                 SEMU_SAPPORO_OHR2_RESPONSE_SIZE);
    device->response_queued = 0;
    device->selector_armed = 0;
    ++device->expected_sequence;
    set_ready(device, 0);
    return SEMU_TRANSACTION_OK;
}

static semu_transaction_result transfer(void *context,
                                        semu_serial_transaction *transaction,
                                        semu_error *error)
{
    semu_sapporo_ohr2 *device = (semu_sapporo_ohr2 *)context;
    uint16_t command = UINT16_MAX;
    uint16_t sequence = UINT16_MAX;
    semu_transaction_result result;
    if (device == NULL || transaction == NULL ||
        transaction->address != SEMU_SAPPORO_OHR2_ADDRESS ||
        (transaction->tx == NULL && transaction->tx_size != 0u) ||
        (transaction->rx == NULL && transaction->rx_size != 0u)) {
        log_transaction(device, "unknown", command, sequence,
                        device != NULL ? device->state : SEMU_SAPPORO_OHR2_BSL,
                        SEMU_TRANSACTION_REFUSE);
        return refuse(error, "address or null buffer");
    }
    if (transaction->tx_size == SEMU_SAPPORO_OHR2_REQUEST_SIZE) {
        command = read_u16(transaction->tx + 1u);
        sequence = read_u16(transaction->tx + 3u);
        result = accept_request(device, transaction, error);
        log_transaction(device, "request", command, sequence, device->state,
                        result);
        return result;
    }
    if (transaction->tx_size == 1u &&
        transaction->tx[0] == SEMU_SAPPORO_OHR2_RESPONSE_SELECTOR) {
        if (device->response_queued) {
            command = read_u16(device->queued_response);
            sequence = read_u16(device->queued_response + 2u);
        }
        if (!device->response_queued) {
            result = refuse(error, "response selector without queued response");
            log_transaction(device, "selector", command, sequence,
                            device->state, result);
            return result;
        }
        if (transaction->rx_size == 0u) {
            device->selector_armed = 1;
            result = SEMU_TRANSACTION_OK;
            log_transaction(device, "selector", command, sequence,
                            device->state, result);
            return result;
        }
        if (transaction->rx_size != SEMU_SAPPORO_OHR2_RESPONSE_SIZE) {
            result = refuse(error, "response selector length");
            log_transaction(device, "selector", command, sequence,
                            device->state, result);
            return result;
        }
        device->selector_armed = 1;
        result = consume_response(device, transaction->rx, error);
        log_transaction(device, "response", command, sequence, device->state,
                        result);
        return result;
    }
    if (transaction->tx_size == 0u &&
        transaction->rx_size == SEMU_SAPPORO_OHR2_RESPONSE_SIZE) {
        if (device->response_queued) {
            command = read_u16(device->queued_response);
            sequence = read_u16(device->queued_response + 2u);
        }
        result = consume_response(device, transaction->rx, error);
        log_transaction(device, "response", command, sequence, device->state,
                        result);
        return result;
    }
    log_transaction(device, "unknown", command, sequence, device->state,
                    SEMU_TRANSACTION_REFUSE);
    return refuse(error, "unknown transaction state");
}

semu_sapporo_ohr2 *semu_sapporo_ohr2_create(
    semu_peripheral_signal_fn ready_callback, void *ready_context,
    semu_sapporo_ohr2_body_provider_fn body_provider, void *body_context,
    semu_error *error)
{
    semu_sapporo_ohr2 *device = (semu_sapporo_ohr2 *)calloc(1u,
                                                              sizeof(*device));
    if (device == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate OHR2");
        return NULL;
    }
    device->ready_callback = ready_callback;
    device->ready_context = ready_context;
    device->body_provider = body_provider;
    device->body_context = body_context;
    device->state = SEMU_SAPPORO_OHR2_BSL;
    semu_error_clear(error);
    return device;
}

void semu_sapporo_ohr2_destroy(semu_sapporo_ohr2 *device)
{
    free(device);
}

void semu_sapporo_ohr2_reset(semu_sapporo_ohr2 *device)
{
    if (device == NULL) {
        return;
    }
    device->state = SEMU_SAPPORO_OHR2_BSL;
    device->response_queued = 0;
    device->selector_armed = 0;
    device->queued_sequence = 0u;
    device->expected_sequence = 0u;
    device->sequence_initialized = 0;
    set_ready(device, 0);
}

void semu_sapporo_ohr2_set_logger(semu_sapporo_ohr2 *device,
                                  semu_logger *logger)
{
    if (device != NULL) {
        device->logger = logger;
    }
}

semu_serial_endpoint semu_sapporo_ohr2_endpoint(
    semu_sapporo_ohr2 *device)
{
    semu_serial_endpoint endpoint;
    endpoint.name = "OHR2";
    endpoint.transfer = transfer;
    endpoint.context = device;
    return endpoint;
}

semu_status semu_sapporo_ohr2_snapshot_write(
    const semu_sapporo_ohr2 *device, semu_snapshot_writer *writer,
    semu_error *error)
{
    if (device == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "OHR2 snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    if (semu_snapshot_writer_u8(writer, (uint8_t)device->state, error) != SEMU_OK ||
        semu_snapshot_writer_bytes(writer, device->queued_response,
                                   sizeof(device->queued_response), error) != SEMU_OK ||
        semu_snapshot_writer_u16(writer, device->queued_sequence, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)(device->response_queued != 0), error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)(device->selector_armed != 0), error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)(device->ready != 0), error) != SEMU_OK ||
        semu_snapshot_writer_u16(writer, device->expected_sequence, error) != SEMU_OK ||
        semu_snapshot_writer_u8(writer, (uint8_t)(device->sequence_initialized != 0), error) != SEMU_OK)
        return error->code;
    return SEMU_OK;
}

semu_status semu_sapporo_ohr2_snapshot_read(
    semu_sapporo_ohr2 *device, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_sapporo_ohr2 candidate;
    uint8_t state, response_queued, selector_armed, ready, initialized;
    if (device == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "OHR2 snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *device;
    if (semu_snapshot_reader_u8(reader, &state, error) != SEMU_OK ||
        semu_snapshot_reader_bytes(reader, candidate.queued_response,
                                   sizeof(candidate.queued_response), error) != SEMU_OK ||
        semu_snapshot_reader_u16(reader, &candidate.queued_sequence, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &response_queued, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &selector_armed, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &ready, error) != SEMU_OK ||
        semu_snapshot_reader_u16(reader, &candidate.expected_sequence, error) != SEMU_OK ||
        semu_snapshot_reader_u8(reader, &initialized, error) != SEMU_OK)
        return error->code;
    if (state > (uint8_t)SEMU_SAPPORO_OHR2_MAIN || response_queued > 1u ||
        selector_armed > 1u || ready > 1u || initialized > 1u) {
        semu_error_set(error, SEMU_ERR_FORMAT, "invalid OHR2 snapshot state");
        return SEMU_ERR_FORMAT;
    }
    candidate.state = (semu_sapporo_ohr2_state)state;
    candidate.response_queued = response_queued;
    candidate.selector_armed = selector_armed;
    candidate.ready = ready;
    candidate.sequence_initialized = initialized;
    *device = candidate;
    return SEMU_OK;
}
