#include "ulsan_sdio.h"

#include "semu/hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDIO_BASE UINT32_C(0x40070000)
#define SDIO_SIZE UINT32_C(0x1000)
#define BLOCK_SIZE 512u
#define MEDIA_SIZE (256u * 1024u * 1024u)

enum {
    BLOCK = 0x04u, ARGUMENT = 0x08u, COMMAND = 0x0cu, RESPONSE = 0x10u,
    DATA = 0x20u, PRESENT = 0x24u, HOST = 0x28u, CLOCK = 0x2cu,
    STATUS = 0x30u, STATUS_ENABLE = 0x34u, SIGNAL_ENABLE = 0x38u,
    CAPABILITIES = 0x40u, VERSION = 0xfcu
};

#define CARD_INSERTED 0x10000u
#define READ_READY 0x800u
#define WRITE_READY 0x400u
#define COMMAND_COMPLETE 1u
#define TRANSFER_COMPLETE 2u
#define COMMAND_TIMEOUT 0x10000u
#define HOST_RESET_MASK 0x07000000u
#define WRITE_MULTIPLE 0x193a0022u
#define STOP_TRANSMISSION 0x0c1a0000u
#define SEND_STATUS 0x0d1a0000u

static semu_status refuse(semu_error *error, const char *reason)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED, "Ulsan SDIO %s", reason);
    return SEMU_ERR_UNSUPPORTED;
}

static void end_transfer(semu_ulsan_sdio *s)
{
    s->transfer_size = 0u;
    s->transferred = 0u;
    s->registers[PRESENT / 4u] &= ~(READ_READY | WRITE_READY);
}

/* E-ULS-0079. Admit the complete command and media span before changing the
 * response or the transfer state. This host has no wired interrupt. */
static semu_status issue_command(semu_ulsan_sdio *s, uint32_t command,
                                 semu_error *error)
{
    static const uint32_t cid[] = {
        0x15010046u, 0x4134454du, 0x4d303031u, 0x25010000u
    };
    static const uint32_t csd[] = {
        0x00180000u, 0xffc00380u, 0x32000900u, 0x00000e00u
    };
    const uint32_t argument = s->registers[ARGUMENT / 4u];
    const uint32_t block = s->registers[BLOCK / 4u];
    uint32_t response[4] = {0};
    uint32_t completion = COMMAND_COMPLETE;
    size_t count = 0u;
    uint64_t start = (uint64_t)argument * BLOCK_SIZE;
    int ready = s->ready, selected = s->selected, writing = 0;

    if (s->transfer_size != 0u)
        return refuse(error, "command during active transfer");
    if (s->multi_write_stop && command != STOP_TRANSMISSION && command != SEND_STATUS)
        return refuse(error, "multi-block write needs CMD12");
    switch (command) {
    case 0u: /* CMD0 */
        if (argument != 0u) return refuse(error, "CMD0 argument");
        ready = selected = 0;
        break;
    case 0x08020000u: /* SD-only probe */
        if (argument != 0x1a5u || ready) return refuse(error, "CMD8 state");
        completion = COMMAND_TIMEOUT;
        break;
    case 0x01020000u: /* CMD1 */
        if (argument != 0u || selected) return refuse(error, "CMD1 state");
        ready = 1;
        response[0] = 0xc0ff8080u;
        break;
    case 0x02090000u: /* CMD2 */
        if (argument != 0u || !ready) return refuse(error, "CMD2 state");
        memcpy(response, cid, sizeof(response));
        break;
    case 0x09090000u: /* CMD9 */
        if (argument != 0x10000u || !ready) return refuse(error, "CMD9 state");
        memcpy(response, csd, sizeof(response));
        break;
    case 0x071b0000u: /* CMD7 */
    case 0x031a0000u: /* CMD3 */
    case SEND_STATUS: /* CMD13 */
        if (argument != 0x10000u || !ready) return refuse(error, "card address");
        if (command == 0x071b0000u) selected = 1;
        response[0] = 0x900u;
        break;
    case 0x101a0000u: /* CMD16 */
        if (argument != BLOCK_SIZE || !selected)
            return refuse(error, "CMD16 state or length");
        response[0] = 0x900u;
        break;
    case 0x183a0002u: /* CMD24 */
    case WRITE_MULTIPLE: /* CMD25, E-ULS-0087 */
        writing = 1;
        /* fall through */
    case 0x113a0012u: /* CMD17 */
    case 0x123a0036u: /* CMD18 */
        count = (command == 0x123a0036u || command == WRITE_MULTIPLE) ? 2u : 1u;
        if (!selected || block != (count << 16u) + BLOCK_SIZE)
            return refuse(error, "block command state or size");
        if (start > s->media_size ||
            count * BLOCK_SIZE > s->media_size - start) {
            semu_error_set(error, SEMU_ERR_RANGE, "Ulsan SDIO sector out of range");
            return SEMU_ERR_RANGE;
        }
        break;
    case STOP_TRANSMISSION: /* CMD12, after the two data blocks */
        if (!s->multi_write_stop || argument != 0u ||
            block != (2u << 16u) + BLOCK_SIZE)
            return refuse(error, "CMD12 state or argument");
        s->multi_write_stop = 0;
        break;
    default:
        return refuse(error, "command word is unsupported");
    }
    s->registers[COMMAND / 4u] = command;
    s->registers[STATUS / 4u] |= completion;
    memcpy(&s->registers[RESPONSE / 4u], response, sizeof(response));
    s->ready = ready;
    s->selected = selected;
    if (count != 0u) {
        s->transfer_start = (size_t)start;
        s->transfer_size = count * BLOCK_SIZE;
        s->writing = writing;
        s->registers[PRESENT / 4u] |= writing ? WRITE_READY : READ_READY;
    }
    return SEMU_OK;
}

static semu_status sdio_read(void *context, uint32_t offset, unsigned width,
                             uint32_t *value, semu_error *error)
{
    semu_ulsan_sdio *s = context;
    const uint8_t *p;

    if (width != 4u || value == NULL)
        return refuse(error, "read width is unsupported");
    switch (offset) {
    case BLOCK: case RESPONSE: case RESPONSE + 4u: case RESPONSE + 8u:
    case RESPONSE + 12u: case PRESENT: case HOST: case CLOCK: case STATUS:
    case STATUS_ENABLE: case CAPABILITIES: case VERSION:
        *value = s->registers[offset / 4u];
        return SEMU_OK;
    case DATA:
        if (s->transfer_size == 0u || s->writing)
            return refuse(error, "data read outside a read transfer");
        p = s->media + s->transfer_start + s->transferred;
        *value = (uint32_t)p[0] | ((uint32_t)p[1] << 8u) |
                 ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
        s->transferred += 4u;
        if (s->transferred == s->transfer_size) {
            end_transfer(s);
            s->registers[STATUS / 4u] |= TRANSFER_COMPLETE;
        }
        return SEMU_OK;
    default:
        return refuse(error, "read offset is unsupported");
    }
}

static semu_status sdio_write(void *context, uint32_t offset, unsigned width,
                              uint32_t value, semu_error *error)
{
    semu_ulsan_sdio *s = context;
    unsigned i;

    if (width != 4u)
        return refuse(error, "write width is unsupported");
    switch (offset) {
    case BLOCK: case ARGUMENT: case HOST: case STATUS_ENABLE: case SIGNAL_ENABLE:
        s->registers[offset / 4u] = value;
        return SEMU_OK;
    case CLOCK:
        s->registers[CLOCK / 4u] = (value & ~HOST_RESET_MASK) |
                                    ((value & 1u) != 0u ? 2u : 0u);
        if ((value & HOST_RESET_MASK) != 0u) {
            end_transfer(s);
            s->multi_write_stop = 0;
        }
        return SEMU_OK;
    case STATUS:
        s->registers[STATUS / 4u] &= ~value;
        return SEMU_OK;
    case COMMAND:
        return issue_command(s, value, error);
    case DATA:
        if (s->transfer_size == 0u || !s->writing)
            return refuse(error, "data write outside a write transfer");
        for (i = 0u; i < 4u; ++i)
            s->pending_write[s->transferred + i] = (uint8_t)(value >> (8u * i));
        s->transferred += 4u;
        if (s->transferred == s->transfer_size) {
            memcpy(s->media + s->transfer_start, s->pending_write,
                   s->transfer_size);
            s->multi_write_stop = s->registers[COMMAND / 4u] == WRITE_MULTIPLE;
            end_transfer(s);
            s->registers[STATUS / 4u] |= TRANSFER_COMPLETE;
        }
        return SEMU_OK;
    default:
        return refuse(error, "write offset is unsupported");
    }
}

static void sdio_reset(void *context)
{
    semu_ulsan_sdio *s = context;
    memset(s->registers, 0, sizeof(s->registers));
    s->registers[PRESENT / 4u] = CARD_INSERTED;
    s->selected = s->ready = 0;
    s->multi_write_stop = 0;
    end_transfer(s);
}

semu_status semu_ulsan_sdio_map(semu_bus *bus, semu_ulsan_sdio *state,
    uint8_t *media, size_t media_size, semu_error *error)
{
    static const semu_bus_device_ops ops = {sdio_read, sdio_write, sdio_reset};
    semu_status status;

    if (state == NULL || media == NULL || media_size == 0u ||
        media_size % BLOCK_SIZE != 0u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Ulsan SDIO requires sector media");
        return SEMU_ERR_ARGUMENT;
    }
    status = semu_bus_map_device(bus, "ulsan.sdio", SDIO_BASE, SDIO_SIZE,
                                 &ops, state, error);
    if (status == SEMU_OK) {
        state->media = media;
        state->media_size = media_size;
        sdio_reset(state);
    }
    return status;
}

uint8_t *semu_ulsan_sdio_load_media(const char *path, size_t *size,
                                    semu_error *error)
{
    static const char expected[] =
        "62b711d026fa42cbcade59e5de6b1f9fb14397dba828a392e993991298c4d962";
    uint8_t digest[SEMU_SHA256_SIZE];
    char hash[65];
    FILE *file;
    uint8_t *media;
    int exact;

    if (path == NULL || size == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Ulsan eMMC path and size are required");
        return NULL;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        semu_error_set(error, SEMU_ERR_IO, "cannot open Ulsan eMMC fixture");
        return NULL;
    }
    media = malloc(MEDIA_SIZE);
    if (media == NULL) {
        fclose(file);
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate Ulsan eMMC session");
        return NULL;
    }
    exact = fread(media, 1u, MEDIA_SIZE, file) == MEDIA_SIZE &&
            fgetc(file) == EOF && !ferror(file);
    if (fclose(file) != 0 || !exact) {
        free(media);
        semu_error_set(error, SEMU_ERR_CONFLICT, "Ulsan eMMC fixture size mismatch");
        return NULL;
    }
    semu_sha256(media, MEDIA_SIZE, digest);
    semu_sha256_format(digest, hash);
    if (strcmp(hash, expected) != 0) {
        free(media);
        semu_error_set(error, SEMU_ERR_CONFLICT, "Ulsan eMMC fixture hash mismatch");
        return NULL;
    }
    *size = MEDIA_SIZE;
    return media;
}
