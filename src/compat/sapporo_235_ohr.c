#include "sapporo_235_ohr.h"
#include <stdio.h>
#include <string.h>

static const char *const hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "36a14dc5bad7b9cb8a7c8164bfaaedaf68c75a9611bc3a9e6efaa47418a5a38a",
    "f281385acc8bab169976f9e506c230fd25124c44bfdbe2048393763a7d85ae22"
};

/* E-SAP-0041 pins the eight ordered synthetic startup responses.
 * E-SAP-0041-EXT adds the six ordered MAIN-state queries the guest repeats
 * after the Done screen (command 0x0010 sequence 8, 0x0000/9, 0x000e/10,
 * 0x0006/11, 0x0002/12, 0x0004/13). Each of those six request frames was
 * captured byte-exactly by the in-tree OHR2 byte census of the five-layer
 * 2.35.34 setup-walk at the moment the eight-hit budget was exhausted, and
 * each response was reproduced twice byte-identically by the read-only lane
 * endpoint under the same three body laws the lane applies (zero body with
 * command/sequence echoed, "MAIN"/"BSL" identity at body offset 9, request
 * body echoed). Command 0x0004 was admitted by the ticket 710 integrator
 * ruling of 2026-09-23 after the lane answered the captured frame twice
 * (normalised lane transcript sha256
 * 4d6309fccc4b71d897c236832d17dbaaab11bc3d0eb3fb1f0150cd0303d37d14, fresh
 * slice-2 pair /tmp/sap235-710ohr/slice2-lane-ext-{1,2}.norm equal to the
 * slice-1 capture of the same sha256); it is the first post-Done query with
 * a payload byte beyond the pinned padding and pins its payload shape
 * exactly: byte 0x23 at offset 4, zeroes through
 * offset 18, pinned padding from offset 19. Requests past the fourteenth, out
 * of this order, or outside the pinned payload padding still fail closed. */
#define SAP235_OHR_RESPONSES 14u

/* E-SAP-0041: synthetic identities/zero startup data, never measurements.
 * The response budget and order belong to each machine's layer state. */
const semu_layer_descriptor semu_sapporo_235_ohr_layer = {
    .id = "sapporo-2.35-ohr-startup",
    .kind = SEMU_LAYER_DEVICE_FIXTURE,
    .profile_id = "sapporo-2.35.34",
    .evidence = "E-SAP-0041",
    .component_hashes = hashes,
    .component_hash_count = 3u,
    .maximum_hits = SAP235_OHR_RESPONSES
};

static semu_transaction_result refuse(semu_sapporo_235_ohr_context *ctx,
                                       semu_error *error)
{
    semu_error local;
    semu_error_set(&local, SEMU_ERR_UNSUPPORTED,
        "Sapporo 2.35 OHR fixture disabled, exhausted or unexpected request");
    if (ctx != NULL && ctx->state != NULL && ctx->state->enabled) {
        if (ctx->refusal.code == SEMU_OK) ctx->refusal = local;
        local = ctx->refusal;
    }
    if (error != NULL) *error = local;
    return SEMU_TRANSACTION_REFUSE;
}

semu_transaction_result semu_sapporo_235_ohr_body_provider(
    void *context, semu_sapporo_ohr2_command command, uint16_t sequence,
    semu_sapporo_ohr2_state state, const uint8_t request[54],
    uint8_t response[54], semu_error *error)
{
    static const uint16_t commands[SAP235_OHR_RESPONSES] = {
        16u, 0u, 16u, 0u, 13u, 14u, 6u, 2u, 16u, 0u, 14u, 6u, 2u, 4u
    };
    semu_sapporo_235_ohr_context *ctx = context;
    size_t index, first = 4u, pad;
    uint8_t body[54];
    char effect[112];
    if (ctx == NULL || ctx->state == NULL || ctx->logger == NULL ||
        !ctx->state->enabled || ctx->state->descriptor != &semu_sapporo_235_ohr_layer ||
        ctx->state->hits >= SAP235_OHR_RESPONSES || ctx->refusal.code != SEMU_OK ||
        request == NULL || response == NULL) return refuse(ctx, error);
    index = (size_t)ctx->state->hits;
    if ((unsigned)command != commands[index] || sequence != index ||
        state != (index < 2u ? SEMU_SAPPORO_OHR2_BSL : SEMU_SAPPORO_OHR2_MAIN) ||
        request[0] != (uint8_t)command || request[1] != 0u ||
        request[2] != (uint8_t)sequence || request[3] != 0u)
        return refuse(ctx, error);
    if (command == SEMU_SAPPORO_OHR2_COMMAND_BOOT_MODE) {
        if (request[4] != 1u) return refuse(ctx, error);
        first = 5u;
    } else if (command == SEMU_SAPPORO_OHR2_COMMAND_ECHO) {
        first = 14u;
    } else if (command == SEMU_SAPPORO_OHR2_COMMAND_RESULT_4) {
        /* E-SAP-0041-EXT: the captured sequence-13 query shape, byte-exact. */
        if (request[4] != 0x23u) return refuse(ctx, error);
        for (pad = 5u; pad < 19u; ++pad)
            if (request[pad] != 0u) return refuse(ctx, error);
        first = 19u;
    }
    for (; first < 54u; ++first)
        if (request[first] != 0xffu) return refuse(ctx, error);
    memset(body, 0, sizeof(body));
    if (command == SEMU_SAPPORO_OHR2_COMMAND_IDENTITY)
        memcpy(body + 9u, state == SEMU_SAPPORO_OHR2_MAIN ? "MAIN" : "BSL",
               state == SEMU_SAPPORO_OHR2_MAIN ? 4u : 3u);
    if (command == SEMU_SAPPORO_OHR2_COMMAND_ECHO)
        memcpy(body + 4u, request + 4u, 50u);
    (void)snprintf(effect, sizeof(effect),
        "trigger=ohr-startup ordinal=%u command=%04x sequence=%u synthetic-body",
        (unsigned)index + 1u, (unsigned)command, (unsigned)sequence);
    if (semu_layer_hit(ctx->state, ctx->logger, effect, error) != SEMU_OK)
        return refuse(ctx, error);
    memcpy(response, body, sizeof(body));
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}
