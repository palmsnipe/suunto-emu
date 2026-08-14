#include "cpu_guest.h"
#include "semu/hash.h"
#include "test.h"
#include "../../fixtures/synthetic/rtos/guest_image.h"

#include <stdio.h>
#include <string.h>

#define RAM_SIZE 0x10000u
#define MAILBOX 0x8800u
#define TASK_FLAG 0x885cu
#define TCB_A 0x8850u
#define TCB_B 0x8854u
#define SWITCH_COUNT 0x8858u
#define TASK_A_FRAME 0x7000u
#define TASK_B_CONTEXT 0x79a0u
#define TASK_B_FRAME 0x7a00u
#define XPSR_T (1u << 24)
#define IRQ_LOW 0u
#define IRQ_WAKE 2u
#define INSTRUCTION_LIMIT UINT64_C(100000)
#define TIME_LIMIT UINT64_C(1000000)
#define EXPECTED_INSTRUCTIONS UINT64_C(268)
#define EXPECTED_VIRTUAL_TIME UINT64_C(344)
#define EXPECTED_STATE_SHA256 \
    "e4497c731b9ad944edefa89b36e75bf8ab9d6c25601ad27361dbc2cf7da9289e"

typedef struct guest_event {
    semu_cpu *cpu;
    unsigned irq;
} guest_event;

typedef struct guest_record {
    uint32_t mailbox[13];
    uint32_t guards[8];
    uint32_t tcb_a;
    uint32_t tcb_b;
    uint32_t switch_count;
    uint32_t task_flag;
    semu_cpu_state state;
    uint64_t virtual_time;
    semu_stop_reason stop_reason;
} guest_record;

static void inject_irq(void *context, uint64_t now_ns)
{
    guest_event *event = (guest_event *)context;

    (void)now_ns;
    semu_cpu_set_irq(event->cpu, event->irq, 1);
}

static void release_irq(void *context, uint64_t now_ns)
{
    guest_event *event = (guest_event *)context;

    (void)now_ns;
    semu_cpu_set_irq(event->cpu, event->irq, 0);
}

static int write_word(semu_cpu_guest *guest, uint32_t address,
                      uint32_t value)
{
    return semu_cpu_guest_write_u32(guest, address, value);
}

static int read_word(semu_cpu_guest *guest, uint32_t address,
                     uint32_t *value)
{
    return semu_bus_read(guest->bus, address, 4u, value, &guest->error) ==
           SEMU_OK;
}

static int seed_contexts(semu_cpu_guest *guest)
{
    static const uint32_t task_a_frame[8] = {
        0u, 0u, 0u, 0u, 0u, 0x1401u, 0x1400u, XPSR_T
    };
    static const uint32_t task_b_frame[8] = {
        0u, 0u, 0u, 0u, 0u, 0x1501u, 0x1500u, XPSR_T
    };
    static const uint32_t task_b_core[8] = {
        0xb4u, 0xb5u, TASK_FLAG, MAILBOX,
        0xb8u, 0xb9u, 0xbau, 0xbbu
    };
    unsigned index;

    for (index = 0u; index < 8u; ++index) {
        if (!write_word(guest, TASK_A_FRAME + index * 4u,
                        task_a_frame[index]) ||
            !write_word(guest, TASK_B_FRAME + index * 4u,
                        task_b_frame[index]) ||
            !write_word(guest, TASK_B_CONTEXT + 0x40u + index * 4u,
                        task_b_core[index])) {
            return 0;
        }
        if (!write_word(guest, TASK_B_CONTEXT + index * 4u,
                        0xb0u + index)) return 0;
    }
    return write_word(guest, TCB_B, TASK_B_CONTEXT) &&
           write_word(guest, SWITCH_COUNT, 0u) &&
           write_word(guest, TASK_FLAG, 0u) &&
           write_word(guest, 0x8100u, UINT32_C(0x51a7c0de)) &&
           write_word(guest, 0x8104u, UINT32_C(0x51a7c0df)) &&
           write_word(guest, 0x6f00u, UINT32_C(0xa11aa11a)) &&
           write_word(guest, 0x6f04u, UINT32_C(0xa11aa11b)) &&
           write_word(guest, 0x7980u, UINT32_C(0xb22bb22a)) &&
           write_word(guest, 0x7984u, UINT32_C(0xb22bb22b));
}

static int schedule_events(semu_cpu_guest *guest, guest_event events[2])
{
    semu_event_id event_id;

    events[0].cpu = guest->cpu;
    events[0].irq = IRQ_LOW;
    events[1].cpu = guest->cpu;
    events[1].irq = IRQ_WAKE;
    semu_error_clear(&guest->error);
    if (semu_scheduler_schedule(guest->scheduler, 200u, inject_irq,
                                &events[0], &event_id, &guest->error) !=
            SEMU_OK ||
        semu_scheduler_schedule(guest->scheduler, 201u, release_irq,
                                &events[0], &event_id, &guest->error) !=
            SEMU_OK ||
        semu_scheduler_schedule(guest->scheduler, 300u, inject_irq,
                                &events[1], &event_id, &guest->error) !=
            SEMU_OK) {
        return 0;
    }
    semu_cpu_set_irq_priority(guest->cpu, 0u, 0x80u);
    semu_cpu_set_irq_priority(guest->cpu, 1u, 0x20u);
    semu_cpu_set_irq_priority(guest->cpu, 2u, 0x40u);
    return 1;
}

static int capture_record(semu_cpu_guest *guest, guest_record *record)
{
    unsigned index;

    (void)memset(record, 0, sizeof(*record));
    for (index = 0u; index < 13u; ++index) {
        if (!read_word(guest, MAILBOX + index * 4u, &record->mailbox[index]))
            return 0;
    }
    if (!read_word(guest, 0x8100u, &record->guards[0]) ||
        !read_word(guest, 0x8104u, &record->guards[1]) ||
        !read_word(guest, 0x6f00u, &record->guards[2]) ||
        !read_word(guest, 0x6f04u, &record->guards[3]) ||
        !read_word(guest, 0x7980u, &record->guards[4]) ||
        !read_word(guest, 0x7984u, &record->guards[5]) ||
        !read_word(guest, TCB_A, &record->tcb_a) ||
        !read_word(guest, TCB_B, &record->tcb_b) ||
        !read_word(guest, SWITCH_COUNT, &record->switch_count) ||
        !read_word(guest, TASK_FLAG, &record->task_flag)) {
        return 0;
    }
    record->state = *semu_cpu_get_state(guest->cpu);
    record->virtual_time = semu_scheduler_now(guest->scheduler);
    record->stop_reason = semu_cpu_stop_reason(guest->cpu);
    return 1;
}

static int run_once(guest_record *record)
{
    semu_cpu_guest guest;
    guest_event events[2];
    semu_status status;
    int ok;

    if (!semu_cpu_guest_init(&guest, semu_rtos_guest_image,
                             SEMU_RTOS_GUEST_IMAGE_SIZE, RAM_SIZE) ||
        !seed_contexts(&guest) || !schedule_events(&guest, events)) {
        semu_cpu_guest_destroy(&guest);
        return 0;
    }
    status = semu_cpu_guest_run(&guest, INSTRUCTION_LIMIT, TIME_LIMIT);
    ok = status == SEMU_OK && capture_record(&guest, record);
    semu_cpu_guest_destroy(&guest);
    return ok;
}

static int check_record(const guest_record *record)
{
    static const uint32_t guards[6] = {
        UINT32_C(0x51a7c0de), UINT32_C(0x51a7c0df),
        UINT32_C(0xa11aa11a), UINT32_C(0xa11aa11b),
        UINT32_C(0xb22bb22a), UINT32_C(0xb22bb22b)
    };
    unsigned index;

    if (record->stop_reason != SEMU_STOP_HALT || record->tcb_a != 0x6f58u ||
        record->tcb_b != TASK_B_CONTEXT || record->switch_count != 2u ||
        record->task_flag != 2u || record->state.s[0] != 0x40000000u ||
        record->state.psp != 0x7020u ||
        (record->state.xpsr & 0x1ffu) != 0u) return 0;
    for (index = 0u; index < 13u; ++index)
        if (record->mailbox[index] != index + 1u) return 0;
    for (index = 0u; index < 6u; ++index)
        if (record->guards[index] != guards[index]) return 0;
    if (record->state.r[4] != 0xa4u || record->state.r[5] != 0xa5u ||
        record->state.r[6] != TASK_FLAG || record->state.r[7] != MAILBOX ||
        record->state.r[8] != 0xa8u || record->state.r[9] != 0xa9u ||
        record->state.r[10] != 0xaau || record->state.r[11] != 0xabu)
        return 0;
    for (index = 0u; index < 16u; ++index)
        if (record->state.s[16u + index] != 0xa0u + index) return 0;
    return 1;
}

static void append_u32(uint8_t *bytes, size_t *size, uint32_t value)
{
    bytes[(*size)++] = (uint8_t)value;
    bytes[(*size)++] = (uint8_t)(value >> 8);
    bytes[(*size)++] = (uint8_t)(value >> 16);
    bytes[(*size)++] = (uint8_t)(value >> 24);
}

static void append_u64(uint8_t *bytes, size_t *size, uint64_t value)
{
    append_u32(bytes, size, (uint32_t)value);
    append_u32(bytes, size, (uint32_t)(value >> 32));
}

static void record_hash(const guest_record *record, char output[65])
{
    uint8_t bytes[13u * 4u + 6u * 4u + 4u * 4u + 16u * 4u + 32u * 4u +
                  16u + 4u];
    size_t size = 0u;
    unsigned index;
    uint8_t digest[SEMU_SHA256_SIZE];

    for (index = 0u; index < 13u; ++index)
        append_u32(bytes, &size, record->mailbox[index]);
    for (index = 0u; index < 6u; ++index)
        append_u32(bytes, &size, record->guards[index]);
    append_u32(bytes, &size, record->tcb_a);
    append_u32(bytes, &size, record->tcb_b);
    append_u32(bytes, &size, record->switch_count);
    append_u32(bytes, &size, record->task_flag);
    for (index = 0u; index < 16u; ++index)
        append_u32(bytes, &size, record->state.r[index]);
    for (index = 0u; index < 32u; ++index)
        append_u32(bytes, &size, record->state.s[index]);
    append_u64(bytes, &size, record->state.instructions);
    append_u64(bytes, &size, record->virtual_time);
    append_u32(bytes, &size, (uint32_t)record->stop_reason);
    semu_sha256(bytes, size, digest);
    semu_sha256_format(digest, output);
}

static void test_rtos_guest(semu_test_context *context)
{
    guest_record first;
    guest_record second;
    uint8_t digest[SEMU_SHA256_SIZE];
    char image_hash[65];
    char state_hash[65];

    semu_sha256(semu_rtos_guest_image, SEMU_RTOS_GUEST_IMAGE_SIZE, digest);
    semu_sha256_format(digest, image_hash);
    SEMU_TEST_ASSERT(context, strcmp(image_hash, SEMU_RTOS_GUEST_IMAGE_SHA256) ==
                              0);
    SEMU_TEST_ASSERT(context, run_once(&first));
    SEMU_TEST_ASSERT(context, run_once(&second));
    SEMU_TEST_ASSERT(context, check_record(&first));
    SEMU_TEST_ASSERT(context, check_record(&second));
    SEMU_TEST_ASSERT(context, memcmp(&first, &second, sizeof(first)) == 0);
    SEMU_TEST_EQ_U64(context, EXPECTED_INSTRUCTIONS,
                     first.state.instructions);
    SEMU_TEST_EQ_U64(context, EXPECTED_VIRTUAL_TIME, first.virtual_time);
    record_hash(&first, state_hash);
    SEMU_TEST_ASSERT(context,
                     strcmp(state_hash, EXPECTED_STATE_SHA256) == 0);
    (void)fprintf(stdout,
                  "checkpoints: reset,svc-start,task-a,tick,pendsv,task-b,"
                  "irq-low,irq-high,irq-low-return,wfi,wake,fp-restored,done\n"
                  "stop=%s instructions=%llu virtual_time=%llu image_sha256=%s "
                  "state_sha256=%s\n",
                  semu_stop_reason_name(first.stop_reason),
                  (unsigned long long)first.state.instructions,
                  (unsigned long long)first.virtual_time, image_hash,
                  state_hash);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_rtos_guest)
    };

    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
