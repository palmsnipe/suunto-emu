#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

#include <string.h>

static void put16(uint8_t *bytes, uint16_t instruction)
{
    bytes[0] = (uint8_t)instruction;
    bytes[1] = (uint8_t)(instruction >> 8);
}

static uint16_t reg_transfer(unsigned op, unsigned rm, unsigned rn,
                             unsigned rt)
{
    return (uint16_t)(0x5000u | (op << 9u) | (rm << 6u) |
                      (rn << 3u) | rt);
}

static uint16_t imm_transfer(unsigned width, unsigned load, unsigned imm,
                             unsigned rn, unsigned rt)
{
    uint16_t base = width == 4u ? 0x6000u :
                    width == 1u ? 0x7000u : 0x8000u;
    return (uint16_t)(base | (load << 11u) | (imm << 6u) |
                      (rn << 3u) | rt);
}

static int run_transfer(unsigned op, uint32_t address, uint32_t input,
                        uint32_t expected, unsigned width, int load)
{
    uint8_t program[4] = {0};
    uint8_t bytes[4] = {(uint8_t)input, (uint8_t)(input >> 8),
                        (uint8_t)(input >> 16), (uint8_t)(input >> 24)};
    semu_cpu_fixture first = {0};
    semu_cpu_fixture second = {0};
    semu_cpu_fixture *fixture;
    semu_cpu_state *state;
    uint32_t value;
    unsigned pass;
    int result = 0;

    put16(program, reg_transfer(op, 2u, 1u, 0u));
    put16(program + 2u, 0xbe00u);
    for (pass = 0u; pass < 2u; ++pass) {
        fixture = pass == 0u ? &first : &second;
        if (!semu_cpu_fixture_init(fixture, program, sizeof(program))) {
            goto done;
        }
        state = semu_cpu_get_state_mutable(fixture->cpu);
        state->r[0] = load ? 0x13579bdfu : input;
        state->r[1] = address;
        if (load && semu_bus_load(fixture->bus, address, bytes, width,
                                  &fixture->error) != SEMU_OK) {
            goto done;
        }
        if (semu_cpu_fixture_step(fixture) != SEMU_OK ||
            state->r[0] != expected || state->r[15] != 0x102u ||
            state->instructions != 1u ||
            semu_cpu_stop_reason(fixture->cpu) != SEMU_STOP_NONE ||
            semu_bus_read(fixture->bus, address, width, &value,
                          &fixture->error) != SEMU_OK) {
            goto done;
        }
        if (!load && value != (input & (width == 4u ? UINT32_MAX :
                                        width == 2u ? 0xffffu : 0xffu))) {
            goto done;
        }
    }
    result = memcmp(semu_cpu_get_state(first.cpu),
                    semu_cpu_get_state(second.cpu),
                    sizeof(semu_cpu_state)) == 0;
done:
    semu_cpu_fixture_destroy(&second);
    semu_cpu_fixture_destroy(&first);
    return result;
}

static void test_register_transfers(semu_test_context *context)
{
    SEMU_TEST_ASSERT(context, run_transfer(0u, 0x600u, 0xa55aa55au,
                                           0xa55aa55au, 4u, 0));
    SEMU_TEST_ASSERT(context, run_transfer(1u, 0x602u, 0x1234u,
                                           0x1234u, 2u, 0));
    SEMU_TEST_ASSERT(context, run_transfer(2u, 0x603u, 0xefu, 0xefu, 1u, 0));
    SEMU_TEST_ASSERT(context, run_transfer(3u, 0x600u, 0x80u,
                                           0xffffff80u, 1u, 1));
    SEMU_TEST_ASSERT(context, run_transfer(4u, 0x600u, 0xa55aa55au,
                                           0xa55aa55au, 4u, 1));
    SEMU_TEST_ASSERT(context, run_transfer(5u, 0x602u, 0x1234u,
                                           0x1234u, 2u, 1));
    SEMU_TEST_ASSERT(context, run_transfer(6u, 0x603u, 0xefu, 0xefu, 1u, 1));
    SEMU_TEST_ASSERT(context, run_transfer(7u, 0x604u, 0x8001u,
                                           0xffff8001u, 2u, 1));
}

static int run_immediate(unsigned width, uint32_t base, unsigned imm,
                         uint32_t input)
{
    uint8_t program[4];
    semu_cpu_fixture fixture = {0};
    semu_cpu_state *state;
    uint32_t address = base + imm * width;
    uint32_t value;
    int result;

    put16(program, imm_transfer(width, 0u, imm, 1u, 0u));
    put16(program + 2u, imm_transfer(width, 1u, imm, 1u, 2u));
    if (!semu_cpu_fixture_init(&fixture, program, sizeof(program))) {
        return 0;
    }
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = input;
    state->r[1] = base;
    result = semu_cpu_fixture_run(&fixture, 2u) == SEMU_OK &&
             state->r[2] == (input & (width == 4u ? UINT32_MAX :
                                      width == 2u ? 0xffffu : 0xffu)) &&
             state->r[15] == 0x104u && state->instructions == 2u &&
             semu_bus_read(fixture.bus, address, width, &value,
                           &fixture.error) == SEMU_OK &&
             value == (input & (width == 4u ? UINT32_MAX :
                                width == 2u ? 0xffffu : 0xffu));
    semu_cpu_fixture_destroy(&fixture);
    return result;
}

static void test_immediate_literal_sp(semu_test_context *context)
{
    static const uint8_t program[] = {
        0x40u, 0x48u, 0x01u, 0x90u, 0x01u, 0x98u, 0x00u, 0xbeu
    };
    semu_cpu_fixture fixture = {0};
    semu_cpu_state *state;
    uint32_t value;

    SEMU_TEST_ASSERT(context, run_immediate(4u, 0xff8u, 1u, 0xa55aa55au));
    SEMU_TEST_ASSERT(context, run_immediate(2u, 0xffcu, 1u, 0x1234u));
    SEMU_TEST_ASSERT(context, run_immediate(1u, 0xffeu, 1u, 0xefu));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, program,
                                                    sizeof(program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[13] = state->msp = 0x600u;
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x204u,
                                                        0x12345678u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_run(&fixture, 3u));
    SEMU_TEST_EQ_U64(context, 0x12345678u, state->r[0]);
    SEMU_TEST_ASSERT(context, semu_bus_read(fixture.bus, 0x604u, 4u, &value,
                                             &fixture.error) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 0x12345678u, value);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_adr_sp_and_stack(semu_test_context *context)
{
    static const uint8_t address_program[] = {
        0x20u, 0xa3u, 0x02u, 0xb0u, 0x81u, 0xb0u, 0x00u, 0xbeu
    };
    static const uint8_t stack_program[] = {
        0x0fu, 0xb5u, 0xf0u, 0xbdu, 0x00u, 0xbeu
    };
    semu_cpu_fixture fixture = {0};
    semu_cpu_state *state;

    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, address_program,
                                                    sizeof(address_program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_run(&fixture, 3u));
    SEMU_TEST_EQ_U64(context, 0x184u, state->r[3]);
    SEMU_TEST_EQ_U64(context, 0x804u, state->r[13]);
    SEMU_TEST_EQ_U64(context, 0x106u, state->r[15]);
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, stack_program,
                                                    sizeof(stack_program)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = 0x11111111u; state->r[1] = 0x22222222u;
    state->r[2] = 0x33333333u; state->r[3] = 0x44444444u;
    state->r[14] = 0x301u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_run(&fixture, 2u));
    SEMU_TEST_EQ_U64(context, 0x11111111u, state->r[4]);
    SEMU_TEST_EQ_U64(context, 0x22222222u, state->r[5]);
    SEMU_TEST_EQ_U64(context, 0x33333333u, state->r[6]);
    SEMU_TEST_EQ_U64(context, 0x44444444u, state->r[7]);
    SEMU_TEST_EQ_U64(context, 0x800u, state->r[13]);
    SEMU_TEST_EQ_U64(context, 0x300u, state->r[15]);
    semu_cpu_fixture_destroy(&fixture);
}

static void test_multiple_transfers(semu_test_context *context)
{
    static const uint8_t store[] = {0x03u, 0xc2u, 0x00u, 0xbeu};
    static const uint8_t load[] = {0x38u, 0xcau, 0x00u, 0xbeu};
    semu_cpu_fixture fixture = {0};
    semu_cpu_state *state;
    uint32_t value;

    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, store,
                                                    sizeof(store)));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = 0x11111111u; state->r[1] = 0x22222222u; state->r[2] = 0x600u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0x608u, state->r[2]);
    SEMU_TEST_ASSERT(context, semu_bus_read(fixture.bus, 0x600u, 4u, &value,
                                             &fixture.error) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 0x11111111u, value);
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture, load,
                                                    sizeof(load)));
    state = semu_cpu_get_state_mutable(fixture.cpu); state->r[2] = 0x600u;
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x600u,
                                                        0x33333333u));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x604u,
                                                        0x44444444u));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x608u,
                                                        0x55555555u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0x33333333u, state->r[3]);
    SEMU_TEST_EQ_U64(context, 0x44444444u, state->r[4]);
    SEMU_TEST_EQ_U64(context, 0x55555555u, state->r[5]);
    SEMU_TEST_EQ_U64(context, 0x60cu, state->r[2]);
    semu_cpu_fixture_destroy(&fixture);
}

static int refusal(uint16_t instruction, uint32_t base, uint32_t offset,
                   int fault)
{
    uint8_t program[4]; semu_cpu_fixture fixture = {0};
    semu_cpu_state *state; uint32_t address; int result;

    put16(program, instruction); put16(program + 2u, 0xbe00u);
    if (!semu_cpu_fixture_init(&fixture, program, sizeof(program))) return 0;
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = 0x13579bdfu; state->r[1] = base; state->r[2] = offset;
    result = semu_cpu_fixture_step(&fixture) ==
                 (fault ? SEMU_ERR_RANGE : SEMU_ERR_UNSUPPORTED) &&
             semu_cpu_stop_reason(fixture.cpu) ==
                 (fault ? SEMU_STOP_UNMAPPED_ACCESS :
                          SEMU_STOP_UNSUPPORTED_INSTRUCTION) &&
             state->r[0] == 0x13579bdfu && state->r[15] == 0x100u &&
             state->instructions == 0u;
    if (fault) result = result && semu_cpu_fault_address(fixture.cpu, &address) &&
                                  address == 0x1000u;
    else result = result && !semu_cpu_fault_address(fixture.cpu, NULL);
    semu_cpu_fixture_destroy(&fixture); return result;
}

static void test_refusals_and_partial_fault(semu_test_context *context)
{
    semu_cpu_fixture fixture = {0}; semu_cpu_state *state; uint32_t value;

    SEMU_TEST_ASSERT(context, refusal(0x6008u, 0x601u, 0u, 0));
    SEMU_TEST_ASSERT(context, refusal(reg_transfer(6u, 2u, 1u, 0u),
                                      0xffffffffu, 1u, 0));
    SEMU_TEST_ASSERT(context, refusal(0x6808u, 0x1000u, 0u, 1));
    SEMU_TEST_ASSERT(context, refusal(0xb400u, 0u, 0u, 0));
    SEMU_TEST_ASSERT(context, refusal(0xbc00u, 0u, 0u, 0));
    SEMU_TEST_ASSERT(context, refusal(0xc800u, 0u, 0u, 0));
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture,
                                                    (const uint8_t[]){0x03u,
                                                        0xc2u, 0x00u, 0xbeu}, 4u));
    state = semu_cpu_get_state_mutable(fixture.cpu);
    state->r[0] = 0xaaaaaaaau; state->r[1] = 0xbbbbbbbbu; state->r[2] = 0xffcu;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0xaaaaaaaau, state->r[0]);
    SEMU_TEST_EQ_U64(context, 0xffcu, state->r[2]);
    SEMU_TEST_ASSERT(context, semu_cpu_fault_address(fixture.cpu, &value));
    SEMU_TEST_EQ_U64(context, 0x1000u, value);
    SEMU_TEST_ASSERT(context, semu_bus_read(fixture.bus, 0xffcu, 4u, &value,
                                             &fixture.error) == SEMU_OK);
    SEMU_TEST_EQ_U64(context, 0xaaaaaaaau, value);
    semu_cpu_fixture_destroy(&fixture);

    SEMU_TEST_ASSERT(context, semu_cpu_fixture_init(&fixture,
                                                    (const uint8_t[]){0x00u,
                                                        0xbdu, 0x00u, 0xbeu}, 4u));
    state = semu_cpu_get_state_mutable(fixture.cpu); state->r[13] = state->msp = 0x600u;
    SEMU_TEST_ASSERT(context, semu_cpu_fixture_load_u32(&fixture, 0x600u, 0x300u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, semu_cpu_fixture_step(&fixture));
    SEMU_TEST_EQ_U64(context, 0x600u, state->r[13]);
    SEMU_TEST_EQ_U64(context, 0x100u, state->r[15]);
    semu_cpu_fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_register_transfers),
        SEMU_TEST_CASE(test_immediate_literal_sp),
        SEMU_TEST_CASE(test_adr_sp_and_stack),
        SEMU_TEST_CASE(test_multiple_transfers),
        SEMU_TEST_CASE(test_refusals_and_partial_fault)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
