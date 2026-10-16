#include "sapporo_233_ohr.h"

#include <stdio.h>
#include <string.h>

static const char *const hashes[] = {
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5",
    "ba286a7bce55dd2f7aba81436d3c5152401c28d86bef89b2c9dbbacd04b7e9e5",
    "b06492a982faa188a14d0a458e7165f9e0b13d0917ded3c3e37077f6ef68d5c5"
};

/* The staged-window exchange order (E-SAP-0074 next refusal plus the lane
 * continuation): identity/boot-mode probes and zero-body results, with the
 * reboot switching the state the fixture expects. The budget is this
 * count; nothing beyond it is admitted. */
#define SEMU_SAPPORO_233_OHR_RESPONSES 7u
#define SEMU_SAPPORO_233_OHR_RULE_COUNT SEMU_SAPPORO_233_OHR_RESPONSES

/* The body provider sees the 54-byte payload (frame bytes 1..54): the
 * command/sequence header is payload 0..3, so the first data byte is
 * payload index 4 (frame byte 5). Both boot-mode requests pin 0x01 there;
 * every other non-echo admitted request is ff padding from index 4. */
static const struct {
    uint16_t command;
    semu_sapporo_ohr2_state state;
    uint8_t payload_from;   /* first index whose value the law pins (>= 4) */
    uint8_t payload_value;  /* required value from that index, 0xff = padding */
} rules[SEMU_SAPPORO_233_OHR_RULE_COUNT] = {
    { 0x0010u, SEMU_SAPPORO_OHR2_BSL, 4u, 0x01u },
    { 0x0000u, SEMU_SAPPORO_OHR2_BSL, 4u, 0xffu },
    { 0x0010u, SEMU_SAPPORO_OHR2_MAIN, 4u, 0x01u },
    { 0x0000u, SEMU_SAPPORO_OHR2_MAIN, 4u, 0xffu },
    { 0x000du, SEMU_SAPPORO_OHR2_MAIN, 4u, 0xffu },
    { 0x000eu, SEMU_SAPPORO_OHR2_MAIN, 4u, 0xffu },
    { 0x0006u, SEMU_SAPPORO_OHR2_MAIN, 4u, 0xffu }
};

/* E-SAP-0075 echo law: the ten caller-supplied bytes sit at payload
 * 4..13 with ff padding from 14 (the E-SAP-0041 transport shape). */
static int echo_body_matches(const uint8_t *payload)
{
    size_t i;
    if (payload[4u] == 0xffu) return 0;
    for (i = 14u; i < SEMU_SAPPORO_OHR2_PAYLOAD_SIZE; ++i) {
        if (payload[i] != 0xffu) return 0;
    }
    return 1;
}

const semu_layer_descriptor semu_sapporo_233_ohr_layer = {
    .id = "sapporo-2.33-ohr-startup",
    .kind = SEMU_LAYER_DEVICE_FIXTURE,
    .profile_id = "sapporo-2.33.16",
    .evidence = "E-SAP-0075",
    .component_hashes = hashes,
    .component_hash_count = 3u,
    .maximum_hits = SEMU_SAPPORO_233_OHR_RULE_COUNT
};

static semu_transaction_result refuse(semu_sapporo_233_ohr_context *c,
                                      semu_error *error, const char *reason)
{
    semu_error local;
    semu_error_set(&local, SEMU_ERR_UNSUPPORTED,
                   "2.33 OHR startup fixture refuses %s", reason);
    if (c != NULL && c->state != NULL && c->state->enabled &&
        c->refusal.code == SEMU_OK) {
        c->refusal = local;
    }
    if (error != NULL) *error = local;
    return SEMU_TRANSACTION_REFUSE;
}

void semu_sapporo_233_ohr_reset(semu_sapporo_233_ohr_context *context)
{
    if (context != NULL) {
        semu_error_clear(&context->refusal);
    }
}

/* The device-transport body provider: the existing sapporo OHR2 endpoint
 * keeps its own framing/CRC/ready law; this fixture only supplies bodies.
 * The layer state gates every response; hits count staged-window rounds. */
semu_transaction_result semu_sapporo_233_ohr_body_provider(
    void *context, semu_sapporo_ohr2_command command, uint16_t sequence,
    semu_sapporo_ohr2_state state,
    const uint8_t request_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    uint8_t response_payload[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE],
    semu_error *error)
{
    semu_sapporo_233_ohr_context *c = context;
    char effect[96];
    size_t index;
    semu_error failure;
    if (c == NULL || c->state == NULL || c->logger == NULL ||
        !c->state->enabled ||
        c->state->descriptor != &semu_sapporo_233_ohr_layer) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "2.33 OHR fixture is disabled or unbound");
        return SEMU_TRANSACTION_REFUSE;
    }
    if (c->refusal.code != SEMU_OK) {
        if (error != NULL) *error = c->refusal;
        return SEMU_TRANSACTION_REFUSE;
    }
    if (sequence != c->state->hits ||
        sequence >= SEMU_SAPPORO_233_OHR_RULE_COUNT ||
        command != rules[sequence].command ||
        state != rules[sequence].state) {
        return refuse(c, error, "round order, command or state");
    }
    if (command == SEMU_SAPPORO_OHR2_COMMAND_ECHO) {
        if (!echo_body_matches(request_payload)) {
            return refuse(c, error, "echo body");
        }
    } else {
        if (request_payload[rules[sequence].payload_from] !=
                rules[sequence].payload_value) {
            return refuse(c, error, "request payload");
        }
        for (index = rules[sequence].payload_from + 1u;
             index < SEMU_SAPPORO_OHR2_PAYLOAD_SIZE; ++index) {
            if (request_payload[index] != 0xffu) {
                return refuse(c, error, "request body padding");
            }
        }
    }
    (void)snprintf(effect, sizeof(effect),
        "trigger=ohr-startup ordinal=%u command=%04x sequence=%u synthetic-body",
        (unsigned)sequence + 1u, (unsigned)command, (unsigned)sequence);
    if (semu_layer_hit(c->state, c->logger, effect, &failure) != SEMU_OK) {
        if (error != NULL) *error = failure;
        return SEMU_TRANSACTION_REFUSE;
    }
    memset(response_payload, 0, SEMU_SAPPORO_OHR2_PAYLOAD_SIZE);
    if (command == SEMU_SAPPORO_OHR2_COMMAND_IDENTITY) {
        if (state == SEMU_SAPPORO_OHR2_MAIN) {
            memcpy(response_payload + 9u, "MAIN", 4u);
        } else {
            memcpy(response_payload + 9u, "BSL", 3u);
        }
    } else if (command == SEMU_SAPPORO_OHR2_COMMAND_ECHO) {
        memcpy(response_payload + 4u, request_payload + 4u,
               SEMU_SAPPORO_OHR2_PAYLOAD_SIZE - 4u);
    }
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}
