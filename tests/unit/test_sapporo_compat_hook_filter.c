#include "test.h"

#include "../../src/devices/sapporo_devices.h"

static void test_exact_hook_pc_filter(semu_test_context *context)
{
    static const uint32_t hook_pcs[] = {
        UINT32_C(0x001145be), UINT32_C(0x0009d166),
        UINT32_C(0x001145e8), UINT32_C(0x0011469c),
        UINT32_C(0x0011470a), UINT32_C(0x0010f6d8),
        UINT32_C(0x0010f4fc), UINT32_C(0x0010f610),
        UINT32_C(0x0010f7c2), UINT32_C(0x0010f7b8),
        UINT32_C(0x0009aaec), UINT32_C(0x0009a3b8),
        UINT32_C(0x0010fbde)
    };
    size_t index;

    for (index = 0u; index < SEMU_ARRAY_LEN(hook_pcs); ++index) {
        SEMU_TEST_EQ_U64(context, 1u,
            semu_sapporo_devices_compat_hook_pc(hook_pcs[index]));
        SEMU_TEST_EQ_U64(context, 0u,
            semu_sapporo_devices_compat_hook_pc(hook_pcs[index] - 2u));
        SEMU_TEST_EQ_U64(context, 0u,
            semu_sapporo_devices_compat_hook_pc(hook_pcs[index] + 2u));
    }
    SEMU_TEST_EQ_U64(context, 0u,
        semu_sapporo_devices_compat_hook_pc(0u));
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_exact_hook_pc_filter)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
