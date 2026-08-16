#include "armv7m_internal.h"

#include <string.h>

#define SCB_CPUID 0xd00u
#define SCB_ICSR 0xd04u
#define SCB_VTOR 0xd08u
#define SCB_AIRCR 0xd0cu
#define SCB_SCR 0xd10u
#define SCB_CCR 0xd14u
#define SCB_SHPR1 0xd18u
#define SCB_SHPR2 0xd1cu
#define SCB_SHPR3 0xd20u
#define SCB_SHCSR 0xd24u
#define SCB_CFSR 0xd28u
#define SCB_HFSR 0xd2cu
#define SCB_MMFAR 0xd34u
#define SCB_BFAR 0xd38u
#define SCB_CPACR 0xd88u
#define SCB_FPCCR 0xf34u
#define SCB_FPCAR 0xf38u
#define SCB_FPDSCR 0xf3cu

#define FPCAR_ADDRESS_MASK 0xfffffff8u
#define FPDSCR_MODEL_MASK 0x07c00000u

#define ICSR_PENDNMISET (1u << 31)
#define ICSR_PENDSVSET (1u << 28)
#define ICSR_PENDSVCLR (1u << 27)
#define ICSR_PENDSTSET (1u << 26)
#define ICSR_PENDSTCLR (1u << 25)
#define ICSR_ISRPENDING (1u << 22)
#define ICSR_RETTOBASE (1u << 11)
#define AIRCR_SYSRESETREQ (1u << 2)

#define SHCSR_MEMFAULTPENDED (1u << 13)
#define SHCSR_BUSFAULTPENDED (1u << 14)
#define SHCSR_USGFAULTPENDED (1u << 12)
#define SHCSR_SVCALLPENDED (1u << 15)
#define SHCSR_SVCALLACT (1u << 7)
#define SHCSR_MEMFAULTACT (1u << 0)
#define SHCSR_BUSFAULTACT (1u << 1)
#define SHCSR_USGFAULTACT (1u << 3)
#define SHCSR_PENDSVACT (1u << 10)
#define SHCSR_SYSTICKACT (1u << 11)
#define SHCSR_MEMFAULTENA (1u << 16)
#define SHCSR_BUSFAULTENA (1u << 17)
#define SHCSR_USGFAULTENA (1u << 18)

static semu_status refuse(uint32_t offset, semu_error *error)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "armv7m SCS refuses offset 0x%03x", offset);
    return SEMU_ERR_UNSUPPORTED;
}

static int privileged(const semu_cpu *cpu)
{
    return (cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK) != 0u ||
           (cpu->state.control & 1u) == 0u;
}

static int access_valid(uint32_t offset, unsigned width)
{
    return (width == 1u || width == 2u || width == 4u) &&
           (offset & (width - 1u)) == 0u &&
           (offset & 3u) + width <= 4u;
}

static uint32_t access_bits(unsigned width, unsigned shift)
{
    uint32_t mask = width == 4u ? UINT32_MAX : (1u << (width * 8u)) - 1u;
    return mask << shift;
}

static uint32_t read_lane(uint32_t value, uint32_t offset, unsigned width)
{
    value >>= (offset & 3u) * 8u;
    return width == 4u ? value : value & ((1u << (width * 8u)) - 1u);
}

static uint32_t write_lane(uint32_t value, uint32_t offset)
{
    return value << ((offset & 3u) * 8u);
}

static int system_priority_byte(uint32_t offset, unsigned *exception)
{
    unsigned byte = offset & 3u;

    if ((offset & ~3u) == SCB_SHPR1 && byte < 3u) {
        *exception = 4u + byte;
        return 1;
    }
    if ((offset & ~3u) == SCB_SHPR2 && byte == 3u) {
        *exception = 11u;
        return 1;
    }
    if ((offset & ~3u) == SCB_SHPR3 && byte >= 2u) {
        *exception = byte == 2u ? 14u : 15u;
        return 1;
    }
    return 0;
}

static uint32_t shpr_value(const semu_cpu *cpu, uint32_t offset)
{
    uint32_t value = 0u;
    unsigned byte;

    for (byte = 0u; byte < 4u; ++byte) {
        unsigned exception;
        uint32_t address = offset + byte;
        if (system_priority_byte(address, &exception)) {
            value |= (uint32_t)cpu->system_priority[exception] << (byte * 8u);
        }
    }
    return value;
}

static void write_shpr(semu_cpu *cpu, uint32_t offset, unsigned width,
                       uint32_t value)
{
    unsigned byte;

    for (byte = 0u; byte < width; ++byte) {
        unsigned exception;
        uint32_t address = offset + byte;
        if (system_priority_byte(address, &exception)) {
            cpu->system_priority[exception] =
                (uint8_t)(value >> (byte * 8u));
        }
    }
}

static uint32_t shcsr_value(const semu_cpu *cpu)
{
    uint32_t value = cpu->shcsr & (SHCSR_MEMFAULTENA |
                                   SHCSR_BUSFAULTENA |
                                   SHCSR_USGFAULTENA);

    if (cpu->system_active[4u] != 0u) value |= SHCSR_MEMFAULTACT;
    if (cpu->system_active[5u] != 0u) value |= SHCSR_BUSFAULTACT;
    if (cpu->system_active[6u] != 0u) value |= SHCSR_USGFAULTACT;
    if (cpu->system_active[11u] != 0u) value |= SHCSR_SVCALLACT;
    if (cpu->system_active[14u] != 0u) value |= SHCSR_PENDSVACT;
    if (cpu->system_active[15u] != 0u) value |= SHCSR_SYSTICKACT;
    if (cpu->system_pending[4u] != 0u) value |= SHCSR_MEMFAULTPENDED;
    if (cpu->system_pending[5u] != 0u) value |= SHCSR_BUSFAULTPENDED;
    if (cpu->system_pending[6u] != 0u) value |= SHCSR_USGFAULTPENDED;
    if (cpu->system_pending[11u] != 0u) value |= SHCSR_SVCALLPENDED;
    return value;
}

static int any_external_pending(const semu_cpu *cpu)
{
    unsigned irq;
    for (irq = 0u; irq < ARMV7M_IMPLEMENTED_IRQ_COUNT; ++irq) {
        if (cpu->irq_level[irq] != 0u || cpu->irq_pending[irq] != 0u)
            return 1;
    }
    return 0;
}

static uint32_t icsr_value(const semu_cpu *cpu)
{
    uint32_t value = cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK;
    int pending = armv7m_pending_exception_for_icsr(cpu);

    if (cpu->exception_depth <= 1u) value |= ICSR_RETTOBASE;
    if (pending >= 0) value |= (uint32_t)pending << 12;
    if (any_external_pending(cpu)) value |= ICSR_ISRPENDING;
    if (cpu->system_pending[2u] != 0u) value |= ICSR_PENDNMISET;
    if (cpu->system_pending[14u] != 0u) value |= ICSR_PENDSVSET;
    if (cpu->system_pending[15u] != 0u) value |= ICSR_PENDSTSET;
    return value;
}

static semu_status read_scb(semu_cpu *cpu, uint32_t offset, unsigned width,
                            uint32_t *value, semu_error *error)
{
    uint32_t raw;

    if (!access_valid(offset, width)) return refuse(offset, error);
    if (offset >= SCB_SHPR1 && offset < SCB_SHPR3 + 4u) {
        raw = shpr_value(cpu, offset & ~3u);
    } else {
        switch (offset & ~3u) {
        case SCB_CPUID: raw = UINT32_C(0x410fc241); break;
        case SCB_ICSR: raw = icsr_value(cpu); break;
        case SCB_VTOR: raw = cpu->vector_table; break;
        case SCB_AIRCR: raw = 0xfa05u << 16 | (uint32_t)cpu->prigroup << 8;
            break;
        case SCB_SCR: raw = cpu->scr; break;
        case SCB_CCR: raw = cpu->ccr; break;
        case SCB_SHCSR: raw = shcsr_value(cpu); break;
        case SCB_CFSR: raw = cpu->cfsr; break;
        case SCB_HFSR: raw = cpu->hfsr; break;
        case SCB_MMFAR: raw = (cpu->cfsr & (1u << 7)) != 0u ?
                                  cpu->mmfar : 0u; break;
        case SCB_BFAR: raw = (cpu->cfsr & (1u << 15)) != 0u ?
                                  cpu->bfar : 0u; break;
        case SCB_CPACR: raw = cpu->cpacr; break;
        case SCB_FPCCR: raw = cpu->fpccr & ARMV7M_FPCCR_READ_MASK; break;
        case SCB_FPCAR: raw = cpu->fpcar & FPCAR_ADDRESS_MASK; break;
        case SCB_FPDSCR: raw = cpu->fpdscr & FPDSCR_MODEL_MASK; break;
        default: return refuse(offset, error);
        }
    }
    *value = read_lane(raw, offset, width);
    return SEMU_OK;
}

static semu_status write_scb(semu_cpu *cpu, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error)
{
    uint32_t bits;
    uint32_t mask;

    if (!access_valid(offset, width)) return refuse(offset, error);
    if (offset >= SCB_SHPR1 && offset < SCB_SHPR3 + 4u) {
        write_shpr(cpu, offset, width, value);
        return SEMU_OK;
    }
    bits = write_lane(value, offset);
    mask = access_bits(width, (offset & 3u) * 8u);
    switch (offset & ~3u) {
    case SCB_ICSR:
        if ((bits & ICSR_PENDNMISET) != 0u)
            armv7m_set_system_pending(cpu, 2u);
        if ((bits & ICSR_PENDSVSET) != 0u)
            armv7m_set_system_pending(cpu, 14u);
        if ((bits & ICSR_PENDSVCLR) != 0u) cpu->system_pending[14u] = 0u;
        if ((bits & ICSR_PENDSTSET) != 0u)
            armv7m_set_system_pending(cpu, 15u);
        if ((bits & ICSR_PENDSTCLR) != 0u) cpu->system_pending[15u] = 0u;
        return SEMU_OK;
    case SCB_VTOR:
        cpu->vector_table = (cpu->vector_table & ~mask) | (bits & mask);
        cpu->vector_table &= ~0x7fu;
        return SEMU_OK;
    case SCB_AIRCR:
        if ((bits >> 16) == 0x5fau) {
            cpu->prigroup = (uint8_t)((bits >> 8) & 7u);
            if ((bits & AIRCR_SYSRESETREQ) != 0u) {
                cpu->reset_requested = 1u;
            }
        }
        return SEMU_OK;
    case SCB_SCR:
        cpu->scr = (cpu->scr & ~mask) | (bits & mask);
        cpu->scr &= (1u << 4) | (1u << 2) | (1u << 1);
        return SEMU_OK;
    case SCB_CCR:
        cpu->ccr = (cpu->ccr & ~mask) | (bits & mask);
        cpu->ccr &= (1u << 9) | (1u << 8) | (1u << 4) | (1u << 3);
        cpu->stack_align = (uint8_t)((cpu->ccr >> 9) & 1u);
        return SEMU_OK;
    case SCB_SHCSR:
        cpu->shcsr = (cpu->shcsr &
                      ~(mask & (SHCSR_MEMFAULTENA | SHCSR_BUSFAULTENA |
                                SHCSR_USGFAULTENA))) |
                     (bits & mask & (SHCSR_MEMFAULTENA | SHCSR_BUSFAULTENA |
                                     SHCSR_USGFAULTENA));
        if ((mask & SHCSR_MEMFAULTACT) != 0u)
            cpu->system_active[4u] = (bits & SHCSR_MEMFAULTACT) != 0u;
        if ((mask & SHCSR_BUSFAULTACT) != 0u)
            cpu->system_active[5u] = (bits & SHCSR_BUSFAULTACT) != 0u;
        if ((mask & SHCSR_USGFAULTACT) != 0u)
            cpu->system_active[6u] = (bits & SHCSR_USGFAULTACT) != 0u;
        if ((mask & SHCSR_SVCALLACT) != 0u)
            cpu->system_active[11u] = (bits & SHCSR_SVCALLACT) != 0u;
        if ((mask & SHCSR_PENDSVACT) != 0u)
            cpu->system_active[14u] = (bits & SHCSR_PENDSVACT) != 0u;
        if ((mask & SHCSR_SYSTICKACT) != 0u)
            cpu->system_active[15u] = (bits & SHCSR_SYSTICKACT) != 0u;
        if ((mask & SHCSR_MEMFAULTPENDED) != 0u) {
            if ((bits & SHCSR_MEMFAULTPENDED) != 0u)
                armv7m_set_system_pending(cpu, 4u);
            else cpu->system_pending[4u] = 0u;
        }
        if ((mask & SHCSR_BUSFAULTPENDED) != 0u) {
            if ((bits & SHCSR_BUSFAULTPENDED) != 0u)
                armv7m_set_system_pending(cpu, 5u);
            else cpu->system_pending[5u] = 0u;
        }
        if ((mask & SHCSR_USGFAULTPENDED) != 0u) {
            if ((bits & SHCSR_USGFAULTPENDED) != 0u)
                armv7m_set_system_pending(cpu, 6u);
            else cpu->system_pending[6u] = 0u;
        }
        if ((mask & SHCSR_SVCALLPENDED) != 0u) {
            if ((bits & SHCSR_SVCALLPENDED) != 0u)
                armv7m_set_system_pending(cpu, 11u);
            else cpu->system_pending[11u] = 0u;
        }
        return SEMU_OK;
    case SCB_CFSR:
        cpu->cfsr &= ~bits;
        return SEMU_OK;
    case SCB_HFSR:
        cpu->hfsr &= ~bits;
        return SEMU_OK;
    case SCB_MMFAR:
        cpu->mmfar = (cpu->mmfar & ~mask) | (bits & mask);
        return SEMU_OK;
    case SCB_BFAR:
        cpu->bfar = (cpu->bfar & ~mask) | (bits & mask);
        return SEMU_OK;
    case SCB_CPACR:
        cpu->cpacr = (cpu->cpacr & ~mask) | (bits & mask);
        cpu->cpacr &= 0x00f00000u;
        return SEMU_OK;
    case SCB_FPCCR:
        cpu->fpccr = (cpu->fpccr & ~(mask & ARMV7M_FPCCR_CONTROL_MASK)) |
                     (bits & mask & ARMV7M_FPCCR_CONTROL_MASK);
        cpu->fpccr &= ARMV7M_FPCCR_READ_MASK;
        return SEMU_OK;
    case SCB_FPCAR:
        cpu->fpcar = (cpu->fpcar & ~mask) | (bits & mask);
        cpu->fpcar &= FPCAR_ADDRESS_MASK;
        return SEMU_OK;
    case SCB_FPDSCR:
        cpu->fpdscr = (cpu->fpdscr & ~(mask & FPDSCR_MODEL_MASK)) |
                      (bits & mask & FPDSCR_MODEL_MASK);
        cpu->fpdscr &= FPDSCR_MODEL_MASK;
        return SEMU_OK;
    default:
        return refuse(offset, error);
    }
}

semu_status armv7m_request_fault(semu_cpu *cpu, unsigned exception,
                                 uint32_t status_bits, uint32_t address,
                                 int address_valid, semu_error *error)
{
    unsigned enable_bit = exception == 4u ? SHCSR_MEMFAULTENA :
                           exception == 5u ? SHCSR_BUSFAULTENA :
                           exception == 6u ? SHCSR_USGFAULTENA : 0u;
    unsigned current = cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK;
    int escalate = 0;

    cpu->cfsr |= status_bits;
    if (address_valid) {
        cpu->fault_address = address;
        cpu->has_fault_address = 1u;
    }
    if (exception == 4u && address_valid) cpu->mmfar = address;
    if (exception == 5u && address_valid) cpu->bfar = address;
    if (current == 2u || current == 3u) {
        cpu->state.halted = 1;
        cpu->stop_reason = SEMU_STOP_FIRMWARE_ASSERT;
        semu_error_set(error, SEMU_ERR_STATE,
                       "fault during NMI or HardFault enters lockup");
        return SEMU_ERR_STATE;
    }
    if (exception >= 4u && exception <= 6u) {
        escalate = (cpu->shcsr & enable_bit) == 0u;
        if (current != 0u && !armv7m_exception_can_preempt(cpu, exception))
            escalate = 1;
    }
    if (escalate) {
        cpu->hfsr |= 1u << 30;
        exception = 3u;
    }
    return armv7m_take_exception(cpu, exception, error);
}

semu_status armv7m_scs_read(void *context, uint32_t offset, unsigned width,
                            uint32_t *value, semu_error *error)
{
    semu_cpu *cpu = (semu_cpu *)context;
    semu_status status;

    if (cpu == NULL || !privileged(cpu)) return refuse(offset, error);
    status = armv7m_nvic_read(cpu, offset, width, value, error);
    if (status == SEMU_OK) return status;
    status = read_scb(cpu, offset, width, value, error);
    if (status == SEMU_OK) return status;
    if (offset >= 0x010u && offset < 0x020u)
        return armv7m_systick_read(cpu, offset, width, value, error);
    status = semu_bus_read_below(cpu->bus, ARMV7M_SCS_BASE + offset,
                                 width, value, error);
    return status == SEMU_OK ? status : refuse(offset, error);
}

semu_status armv7m_scs_write(void *context, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error)
{
    semu_cpu *cpu = (semu_cpu *)context;
    semu_status status;

    if (cpu == NULL || !privileged(cpu)) return refuse(offset, error);
    status = armv7m_nvic_write(cpu, offset, width, value, error);
    if (status == SEMU_OK) return status;
    status = write_scb(cpu, offset, width, value, error);
    if (status == SEMU_OK) return status;
    if (offset >= 0x010u && offset < 0x020u)
        return armv7m_systick_write(cpu, offset, width, value, error);
    status = semu_bus_write_below(cpu->bus, ARMV7M_SCS_BASE + offset,
                                  width, value, error);
    return status == SEMU_OK ? status : refuse(offset, error);
}

void armv7m_scs_reset(void *context)
{
    semu_cpu *cpu = (semu_cpu *)context;

    if (cpu == NULL) return;
    memset(cpu->irq_enabled, 0, sizeof(cpu->irq_enabled));
    memset(cpu->irq_level, 0, sizeof(cpu->irq_level));
    memset(cpu->irq_pending, 0, sizeof(cpu->irq_pending));
    memset(cpu->irq_active, 0, sizeof(cpu->irq_active));
    memset(cpu->irq_priority, 0, sizeof(cpu->irq_priority));
    memset(cpu->system_priority, 0, sizeof(cpu->system_priority));
    memset(cpu->system_pending, 0, sizeof(cpu->system_pending));
    memset(cpu->system_active, 0, sizeof(cpu->system_active));
    cpu->exception_depth = 0u;
    cpu->prigroup = 0u;
    cpu->vector_table = 0u;
    cpu->scr = 0u;
    cpu->ccr = 1u << 9;
    cpu->shcsr = 0u;
    cpu->cfsr = 0u;
    cpu->hfsr = 0u;
    cpu->mmfar = 0u;
    cpu->bfar = 0u;
    cpu->cpacr = 0u;
    cpu->fpccr = ARMV7M_FPCCR_CONTROL_MASK;
    cpu->fpcar = 0u;
    cpu->fpdscr = 0u;
    cpu->fpca = 0u;
    cpu->fp_context_fault = 0u;
    cpu->stack_fault_active = 0u;
    cpu->bus_fault_active = 0u;
    cpu->reset_requested = 0u;
    armv7m_systick_reset(cpu);
    cpu->stack_align = 1u;
}
