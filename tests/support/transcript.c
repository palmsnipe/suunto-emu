#include "transcript.h"

#include <string.h>

static void clear_mismatch(semu_test_transcript_mismatch *mismatch)
{
    if (mismatch != NULL) {
        mismatch->field = SEMU_TEST_TRANSCRIPT_MATCH;
        mismatch->entry_index = 0u;
        mismatch->byte_index = 0u;
    }
}

static semu_status refuse_record(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_RANGE, "transcript record %s", reason);
    return SEMU_ERR_RANGE;
}

void semu_test_transcript_init(semu_test_transcript *transcript)
{
    if (transcript != NULL) {
        memset(transcript, 0, sizeof(*transcript));
    }
}

semu_status semu_test_transcript_record(
    semu_test_transcript *transcript, semu_test_transcript_direction direction,
    uint8_t address, uint8_t chip_select, const uint8_t *bytes, size_t size,
    semu_transaction_result result, semu_error *error)
{
    semu_test_transcript_entry *entry;
    if (transcript == NULL || (unsigned)direction >
                                  (unsigned)SEMU_TEST_TRANSCRIPT_RX ||
        (unsigned)result > (unsigned)SEMU_TRANSACTION_REFUSE) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "invalid transcript record");
        return SEMU_ERR_ARGUMENT;
    }
    if (transcript->count >= SEMU_TEST_TRANSCRIPT_MAX_ENTRIES) {
        return refuse_record(error, "capacity");
    }
    if (size > SEMU_TEST_TRANSCRIPT_MAX_BYTES) {
        return refuse_record(error, "size");
    }
    if (size != 0u && bytes == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "transcript bytes are required");
        return SEMU_ERR_ARGUMENT;
    }
    entry = &transcript->entries[transcript->count];
    entry->direction = direction;
    entry->address = address;
    entry->chip_select = chip_select;
    entry->size = size;
    entry->result = result;
    if (size != 0u) {
        memcpy(entry->bytes, bytes, size);
    }
    ++transcript->count;
    return SEMU_OK;
}

semu_status semu_test_transcript_record_serial(
    semu_test_transcript *transcript, const semu_serial_transaction *transaction,
    semu_transaction_result result, semu_error *error)
{
    semu_status status;
    if (transaction == NULL || (transaction->tx_size != 0u &&
                                transaction->tx == NULL) ||
        (transaction->rx_size != 0u && transaction->rx == NULL)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "invalid serial transaction");
        return SEMU_ERR_ARGUMENT;
    }
    if (transcript == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "transcript is required");
        return SEMU_ERR_ARGUMENT;
    }
    if (transaction->tx_size > SEMU_TEST_TRANSCRIPT_MAX_BYTES ||
        transaction->rx_size > SEMU_TEST_TRANSCRIPT_MAX_BYTES) {
        return refuse_record(error, "size");
    }
    if (transcript->count > SEMU_TEST_TRANSCRIPT_MAX_ENTRIES ||
        SEMU_TEST_TRANSCRIPT_MAX_ENTRIES - transcript->count < 2u) {
        return refuse_record(error, "capacity");
    }
    status = semu_test_transcript_record(
        transcript, SEMU_TEST_TRANSCRIPT_TX, transaction->address,
        transaction->chip_select, transaction->tx, transaction->tx_size,
        result, error);
    if (status != SEMU_OK) {
        return status;
    }
    status = semu_test_transcript_record(
        transcript, SEMU_TEST_TRANSCRIPT_RX, transaction->address,
        transaction->chip_select, transaction->rx, transaction->rx_size,
        result, error);
    if (status != SEMU_OK) {
        --transcript->count;
    }
    return status;
}

static semu_status validate_transcript(const semu_test_transcript *transcript,
                                       semu_error *error)
{
    size_t index;
    if (transcript->count > SEMU_TEST_TRANSCRIPT_MAX_ENTRIES) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "transcript count exceeds capacity");
        return SEMU_ERR_RANGE;
    }
    for (index = 0u; index < transcript->count; ++index) {
        const semu_test_transcript_entry *entry = &transcript->entries[index];
        if ((unsigned)entry->direction >
                (unsigned)SEMU_TEST_TRANSCRIPT_RX ||
            entry->size > SEMU_TEST_TRANSCRIPT_MAX_BYTES ||
            (unsigned)entry->result > (unsigned)SEMU_TRANSACTION_REFUSE) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                           "invalid transcript entry %lu",
                           (unsigned long)index);
            return SEMU_ERR_FORMAT;
        }
    }
    return SEMU_OK;
}

static semu_status mismatch(semu_test_transcript_mismatch *output,
                             semu_test_transcript_mismatch_field field,
                             size_t entry_index, size_t byte_index,
                             semu_error *error, const char *format,
                             unsigned long first, unsigned long second)
{
    if (output != NULL) {
        output->field = field;
        output->entry_index = entry_index;
        output->byte_index = byte_index;
    }
    semu_error_set(error, SEMU_ERR_CONFLICT, format, first, second);
    return SEMU_ERR_CONFLICT;
}

semu_status semu_test_transcript_compare(
    const semu_test_transcript *expected, const semu_test_transcript *actual,
    semu_test_transcript_mismatch *mismatch_output, semu_error *error)
{
    size_t index;
    clear_mismatch(mismatch_output);
    if (expected == NULL || actual == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "transcripts are required");
        return SEMU_ERR_ARGUMENT;
    }
    if (validate_transcript(expected, error) != SEMU_OK ||
        validate_transcript(actual, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_FORMAT;
    }
    if (expected->count != actual->count) {
        return mismatch(mismatch_output, SEMU_TEST_TRANSCRIPT_COUNT,
                         expected->count < actual->count ? expected->count :
                                                            actual->count,
                         0u, error, "transcript count mismatch: %lu != %lu",
                         (unsigned long)expected->count,
                         (unsigned long)actual->count);
    }
    for (index = 0u; index < expected->count; ++index) {
        const semu_test_transcript_entry *left = &expected->entries[index];
        const semu_test_transcript_entry *right = &actual->entries[index];
        size_t byte_index;
        if (left->direction != right->direction) {
            return mismatch(mismatch_output, SEMU_TEST_TRANSCRIPT_DIRECTION,
                             index, 0u, error,
                             "transcript direction mismatch at entry %lu",
                             (unsigned long)index, 0u);
        }
        if (left->address != right->address) {
            return mismatch(mismatch_output, SEMU_TEST_TRANSCRIPT_ADDRESS,
                             index, 0u, error,
                             "transcript address mismatch at entry %lu",
                             (unsigned long)index, 0u);
        }
        if (left->chip_select != right->chip_select) {
            return mismatch(mismatch_output,
                             SEMU_TEST_TRANSCRIPT_CHIP_SELECT, index, 0u,
                             error,
                             "transcript chip-select mismatch at entry %lu",
                             (unsigned long)index, 0u);
        }
        if (left->size != right->size ||
            memcmp(left->bytes, right->bytes,
                   left->size < right->size ? left->size : right->size) != 0) {
            byte_index = 0u;
            while (byte_index < left->size && byte_index < right->size &&
                   left->bytes[byte_index] == right->bytes[byte_index]) {
                ++byte_index;
            }
            return mismatch(mismatch_output, SEMU_TEST_TRANSCRIPT_BYTES,
                             index, byte_index, error,
                             "transcript bytes mismatch at entry %lu byte %lu",
                             (unsigned long)index,
                             (unsigned long)byte_index);
        }
        if (left->result != right->result) {
            return mismatch(mismatch_output, SEMU_TEST_TRANSCRIPT_RESULT,
                             index, 0u, error,
                             "transcript result mismatch at entry %lu",
                             (unsigned long)index, 0u);
        }
    }
    return SEMU_OK;
}
