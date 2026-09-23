#include "test.h"
#include "sapporo_flash.h"
#include "../../src/soc/apollo4/mspi.h"
#include "semu/hash.h"
#include <stdio.h>
#include <string.h>

#define CAPACITY UINT32_C(0x02000000)
#define BLOCK UINT32_C(0x10000)

typedef struct {
    char path[128];
    semu_storage *storage;
    semu_sapporo_flash *flash;
    semu_serial_endpoint endpoint;
    semu_bus *bus;
    semu_apollo4_mspi *mspi;
    semu_error error;
} fixture;

static int start(fixture *f)
{
    FILE *out;
    uint8_t page[4096];
    unsigned i;
    memset(f, 0, sizeof(*f));
    memset(page, 0xa5, sizeof(page));
    if (!semu_test_temp_path(f->path, sizeof(f->path), "block-erase.bin")) return 0;
    out = fopen(f->path, "wb");
    if (!out) return 0;
    for (i = 0u; i < 48u; ++i) {
        if (fwrite(page, 1u, sizeof(page), out) != sizeof(page)) {
            fclose(out); return 0;
        }
    }
    if (fclose(out) != 0) return 0;
    f->storage = semu_storage_open(f->path, CAPACITY, 0xffu, &f->error);
    f->flash = semu_sapporo_flash_create(f->storage, CAPACITY, 4096u, 256u, &f->error);
    f->bus = semu_bus_create(&f->error);
    f->mspi = semu_apollo4_mspi_create(f->bus, SEMU_APOLLO4_MSPI2_BASE,
        SEMU_APOLLO4_MSPI2_IRQ, NULL, NULL, NULL, NULL, &f->error);
    f->endpoint = semu_sapporo_flash_endpoint(f->flash);
    return f->storage && f->flash && f->bus && f->mspi &&
        semu_apollo4_mspi_attach_endpoint(f->mspi, &f->endpoint, &f->error) == SEMU_OK;
}

static void finish(fixture *f)
{
    semu_apollo4_mspi_destroy(f->mspi); semu_bus_destroy(f->bus);
    semu_sapporo_flash_destroy(f->flash); semu_storage_destroy(f->storage);
    remove(f->path);
}

static semu_status command(fixture *f, uint8_t opcode, uint32_t address)
{
    semu_status status = semu_apollo4_mspi_write(f->mspi,
        SEMU_APOLLO4_MSPI2_DATA, 4u, opcode, &f->error);
    if (status != SEMU_OK) return status;
    status = semu_apollo4_mspi_write(f->mspi,
        SEMU_APOLLO4_MSPI2_ADDRESS, 4u, address, &f->error);
    if (status != SEMU_OK) return status;
    return semu_apollo4_mspi_write(f->mspi,
        SEMU_APOLLO4_MSPI2_COMMAND, 4u, 0xc1u, &f->error);
}

static void test_block_erase_extent_and_address(semu_test_context *context)
{
    fixture f;
    uint8_t bytes[BLOCK], hash[32], after[32], zero = 0u, byte;
    uint64_t size;
    const uint32_t bases[] = { BLOCK, UINT32_C(0x01010000), CAPACITY - BLOCK };
    size_t i, j;
    SEMU_TEST_ASSERT(context, start(&f));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sha256_file(f.path, hash, &size, &f.error));
    for (i = 0u; i < SEMU_ARRAY_LEN(bases); ++i) {
        uint32_t base = bases[i];
        if (i != 0u) {
            memset(bytes, 0, sizeof(bytes));
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_storage_program(f.storage, base, bytes, sizeof(bytes), &f.error));
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_storage_program(f.storage, base - 1u, &zero, 1u, &f.error));
            if (base + BLOCK < CAPACITY)
                SEMU_TEST_EQ_U64(context, SEMU_OK,
                    semu_storage_program(f.storage, base + BLOCK, &zero, 1u, &f.error));
        }
        SEMU_TEST_EQ_U64(context, SEMU_OK, command(&f, 6u, 0u));
        SEMU_TEST_EQ_U64(context, SEMU_OK, command(&f, 0xdcu, base + BLOCK - 1u));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_storage_read(f.storage, base, bytes, sizeof(bytes), &f.error));
        for (j = 0u; j < sizeof(bytes); ++j) SEMU_TEST_EQ_U64(context, 0xffu, bytes[j]);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_storage_read(f.storage, base - 1u, &byte, 1u, &f.error));
        SEMU_TEST_EQ_U64(context, i == 0u ? 0xa5u : 0u, byte);
        if (base + BLOCK < CAPACITY) {
            SEMU_TEST_EQ_U64(context, SEMU_OK,
                semu_storage_read(f.storage, base + BLOCK, &byte, 1u, &f.error));
            SEMU_TEST_EQ_U64(context, i == 0u ? 0xa5u : 0u, byte);
        }
        SEMU_TEST_ASSERT(context, command(&f, 0xdcu, base) != SEMU_OK);
    }
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_sha256_file(f.path, after, &size, &f.error));
    SEMU_TEST_ASSERT(context, memcmp(hash, after, sizeof(hash)) == 0);
    finish(&f);
}

static void test_block_erase_refusals(semu_test_context *context)
{
    fixture f;
    uint8_t tx[] = {0xdcu, 1u, 0u, 0u}, byte = 0u;
    semu_serial_transaction request = {0};
    unsigned variant;
    SEMU_TEST_ASSERT(context, start(&f));
    request.tx = tx; request.tx_size = sizeof(tx);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        f.endpoint.transfer(f.endpoint.context, &request, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, command(&f, 6u, 0u));
    for (variant = 0u; variant < 4u; ++variant) {
        request.tx_size = variant == 0u ? 3u : sizeof(tx);
        request.rx_size = variant == 1u ? 1u : 0u;
        request.rx = &byte;
        request.address = variant >= 2u ? (variant == 2u ? 2u : 255u) : 0u;
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
            f.endpoint.transfer(f.endpoint.context, &request, &f.error));
        SEMU_TEST_EQ_U64(context, 0u, semu_storage_dirty_pages(f.storage));
    }
    /* Invalid requests preserve WREN, so a valid operation still succeeds. */
    request.address = 0u; request.rx_size = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        f.endpoint.transfer(f.endpoint.context, &request, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, command(&f, 6u, 0u));
    semu_sapporo_flash_reset(f.flash);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        f.endpoint.transfer(f.endpoint.context, &request, &f.error));
    finish(&f);
}

static void test_block_erase_unknown_controller_command(semu_test_context *context)
{
    fixture f;
    uint32_t value;
    SEMU_TEST_ASSERT(context, start(&f));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, command(&f, 0xdeu, BLOCK));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_mspi_read(f.mspi,
        SEMU_APOLLO4_MSPI_INTSTAT, 4u, &value, &f.error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_mspi_read(f.mspi,
        SEMU_APOLLO4_MSPI2_COMMAND, 4u, &value, &f.error));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, 0u, semu_storage_dirty_pages(f.storage));
    finish(&f);
}

static void test_block_erase_power_command_completion(semu_test_context *context)
{
    fixture f;
    uint32_t value;
    const uint8_t commands[] = { 0xb9u, 0xabu };
    size_t i;
    SEMU_TEST_ASSERT(context, start(&f));
    SEMU_TEST_EQ_U64(context, SEMU_OK, command(&f, 6u, 0u));
    for (i = 0u; i < SEMU_ARRAY_LEN(commands); ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_mspi_write(f.mspi,
            SEMU_APOLLO4_MSPI_INTCLR, 4u, UINT32_MAX, &f.error));
        SEMU_TEST_EQ_U64(context, SEMU_OK, command(&f, commands[i], 0u));
        SEMU_TEST_EQ_U64(context, SEMU_OK, semu_apollo4_mspi_read(f.mspi,
            SEMU_APOLLO4_MSPI_INTSTAT, 4u, &value, &f.error));
        SEMU_TEST_EQ_U64(context, 1u, value);
        SEMU_TEST_EQ_U64(context, 0u, semu_storage_dirty_pages(f.storage));
    }
    /* Lane completion does not consume the endpoint's write-enable latch. */
    SEMU_TEST_EQ_U64(context, SEMU_OK, command(&f, 0xdcu, BLOCK));
    finish(&f);
}

int main(void)
{
    const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_block_erase_extent_and_address),
        SEMU_TEST_CASE(test_block_erase_refusals),
        SEMU_TEST_CASE(test_block_erase_unknown_controller_command),
        SEMU_TEST_CASE(test_block_erase_power_command_completion)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
