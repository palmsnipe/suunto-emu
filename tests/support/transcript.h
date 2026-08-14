#ifndef SEMU_TEST_TRANSCRIPT_H
#define SEMU_TEST_TRANSCRIPT_H

#include "semu/peripheral.h"

#define SEMU_TEST_TRANSCRIPT_MAX_ENTRIES 64u
#define SEMU_TEST_TRANSCRIPT_MAX_BYTES 256u

typedef enum semu_test_transcript_direction {
    SEMU_TEST_TRANSCRIPT_TX = 0,
    SEMU_TEST_TRANSCRIPT_RX
} semu_test_transcript_direction;

typedef struct semu_test_transcript_entry {
    semu_test_transcript_direction direction;
    uint8_t address;
    uint8_t chip_select;
    size_t size;
    uint8_t bytes[SEMU_TEST_TRANSCRIPT_MAX_BYTES];
    semu_transaction_result result;
} semu_test_transcript_entry;

typedef struct semu_test_transcript {
    size_t count;
    semu_test_transcript_entry entries[SEMU_TEST_TRANSCRIPT_MAX_ENTRIES];
} semu_test_transcript;

typedef enum semu_test_transcript_mismatch_field {
    SEMU_TEST_TRANSCRIPT_MATCH = 0,
    SEMU_TEST_TRANSCRIPT_COUNT,
    SEMU_TEST_TRANSCRIPT_DIRECTION,
    SEMU_TEST_TRANSCRIPT_ADDRESS,
    SEMU_TEST_TRANSCRIPT_CHIP_SELECT,
    SEMU_TEST_TRANSCRIPT_BYTES,
    SEMU_TEST_TRANSCRIPT_RESULT
} semu_test_transcript_mismatch_field;

typedef struct semu_test_transcript_mismatch {
    semu_test_transcript_mismatch_field field;
    size_t entry_index;
    size_t byte_index;
} semu_test_transcript_mismatch;

void semu_test_transcript_init(semu_test_transcript *transcript);
semu_status semu_test_transcript_record(
    semu_test_transcript *transcript, semu_test_transcript_direction direction,
    uint8_t address, uint8_t chip_select, const uint8_t *bytes, size_t size,
    semu_transaction_result result, semu_error *error);
semu_status semu_test_transcript_record_serial(
    semu_test_transcript *transcript, const semu_serial_transaction *transaction,
    semu_transaction_result result, semu_error *error);
semu_status semu_test_transcript_compare(
    const semu_test_transcript *expected, const semu_test_transcript *actual,
    semu_test_transcript_mismatch *mismatch, semu_error *error);

#endif
