#include "armv7m_internal.h"

static semu_status refuse(uint32_t offset, semu_error *error)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "armv7m NVIC refuses offset 0x%03x", offset);
    return SEMU_ERR_UNSUPPORTED;
}

static int access_valid(uint32_t offset, unsigned width)
{
    return (width == 1u || width == 2u || width == 4u) &&
           (offset & (width - 1u)) == 0u &&
           (offset & 3u) + width <= 4u;
}

int armv7m_exception_priority(const semu_cpu *cpu, unsigned exception)
{
    if (exception == 2u) return -2;
    if (exception == 3u) return -1;
    if (exception >= 16u && exception < 16u + ARMV7M_IRQ_COUNT) {
        return cpu->irq_priority[exception - 16u];
    }
    if (exception < 16u) {
        return cpu->system_priority[exception];
    }
    return 0x100;
}

static int exception_masked(const semu_cpu *cpu, unsigned exception,
                            int ignore_primask)
{
    int priority;

    if (exception == 2u) return 0;
    if (exception >= 16u && exception >= 16u + ARMV7M_IMPLEMENTED_IRQ_COUNT)
        return 1;
    if (cpu->state.faultmask != 0u) return 1;
    if (exception == 3u) return 0;
    if (!ignore_primask && cpu->state.primask != 0u) return 1;
    priority = armv7m_exception_priority(cpu, exception);
    return cpu->state.basepri != 0u && priority >= (int)cpu->state.basepri;
}

int armv7m_exception_masked(const semu_cpu *cpu, unsigned exception)
{
    return exception_masked(cpu, exception, 0);
}

static int configurable_fault_enabled(const semu_cpu *cpu,
                                      unsigned exception)
{
    unsigned bit = exception == 4u ? 16u :
                   exception == 5u ? 17u :
                   exception == 6u ? 18u : 0u;
    return bit != 0u && (cpu->shcsr & (1u << bit)) != 0u;
}

static int can_preempt(const semu_cpu *cpu, unsigned exception)
{
    unsigned current = cpu->state.xpsr & ARMV7M_XPSR_IPSR_MASK;
    int candidate_priority;
    int current_priority;

    if (current == 0u) return 1;
    if (exception == 2u) return current != 2u;
    if (current == 2u) return 0;
    candidate_priority = armv7m_exception_priority(cpu, exception);
    current_priority = armv7m_exception_priority(cpu, current);
    if (candidate_priority < 0 || current_priority < 0)
        return candidate_priority < current_priority;
    return (candidate_priority >> (cpu->prigroup + 1u)) <
           (current_priority >> (cpu->prigroup + 1u));
}

int armv7m_exception_can_preempt(const semu_cpu *cpu, unsigned exception)
{
    return can_preempt(cpu, exception);
}

static int pending_exception(const semu_cpu *cpu, int ignore_primask,
                             int require_preemption)
{
    int selected = -1;
    int selected_priority = 0x101;
    unsigned selected_source = 0xffffffffu;
    unsigned exception;

    for (exception = 2u; exception < 16u; ++exception) {
        unsigned candidate = exception;
        int priority;
        if (cpu->system_pending[exception] == 0u ||
            (exception >= 4u && exception <= 6u &&
             !configurable_fault_enabled(cpu, exception)) ||
            exception_masked(cpu, candidate, ignore_primask) ||
            (require_preemption && !can_preempt(cpu, candidate))) {
            continue;
        }
        priority = armv7m_exception_priority(cpu, candidate);
        if (selected < 0 || priority < selected_priority ||
            (priority == selected_priority && exception < selected_source)) {
            selected = (int)candidate;
            selected_priority = priority;
            selected_source = exception;
        }
    }
    for (exception = 16u;
         exception < 16u + ARMV7M_IMPLEMENTED_IRQ_COUNT; ++exception) {
        unsigned irq = exception - 16u;
        unsigned candidate;
        int priority;
        if (cpu->irq_enabled[irq] == 0u ||
            (cpu->irq_level[irq] == 0u && cpu->irq_pending[irq] == 0u) ||
            (candidate = exception,
             exception_masked(cpu, candidate, ignore_primask)) ||
            (require_preemption && !can_preempt(cpu, candidate))) {
            continue;
        }
        priority = armv7m_exception_priority(cpu, candidate);
        if (selected < 0 || priority < selected_priority ||
            (priority == selected_priority && exception < selected_source)) {
            selected = (int)candidate;
            selected_priority = priority;
            selected_source = exception;
        }
    }
    return selected;
}

int armv7m_pending_exception(const semu_cpu *cpu)
{
    return pending_exception(cpu, 0, 1);
}

int armv7m_pending_exception_for_icsr(const semu_cpu *cpu)
{
    return pending_exception(cpu, 1, 0);
}

void armv7m_signal_pending_event(semu_cpu *cpu, unsigned exception)
{
    if (cpu != NULL && exception < 16u + ARMV7M_IMPLEMENTED_IRQ_COUNT &&
        (cpu->scr & (1u << 4)) != 0u)
        armv7m_sleep_event(cpu);
}

void armv7m_set_system_pending(semu_cpu *cpu, unsigned exception)
{
    if (cpu == NULL || exception >= 16u ||
        cpu->system_pending[exception] != 0u)
        return;
    cpu->system_pending[exception] = 1u;
    if (cpu->pending_source_count != UINT16_MAX) {
        ++cpu->pending_source_count;
    }
    armv7m_signal_pending_event(cpu, exception);
}

void armv7m_clear_system_pending(semu_cpu *cpu, unsigned exception)
{
    if (cpu == NULL || exception >= 16u ||
        cpu->system_pending[exception] == 0u)
        return;
    cpu->system_pending[exception] = 0u;
    if (cpu->pending_source_count != 0u) {
        --cpu->pending_source_count;
    }
}

void armv7m_set_irq_pending(semu_cpu *cpu, unsigned irq)
{
    if (cpu == NULL || irq >= ARMV7M_IMPLEMENTED_IRQ_COUNT ||
        cpu->irq_pending[irq] != 0u)
        return;
    cpu->irq_pending[irq] = 1u;
    if (cpu->pending_source_count != UINT16_MAX) {
        ++cpu->pending_source_count;
    }
    armv7m_signal_pending_event(cpu, 16u + irq);
}

void armv7m_clear_irq_pending(semu_cpu *cpu, unsigned irq)
{
    if (cpu == NULL || irq >= ARMV7M_IMPLEMENTED_IRQ_COUNT ||
        cpu->irq_pending[irq] == 0u)
        return;
    cpu->irq_pending[irq] = 0u;
    if (cpu->irq_level[irq] == 0u && cpu->pending_source_count != 0u) {
        --cpu->pending_source_count;
    }
}

int armv7m_pending_wake(const semu_cpu *cpu)
{
    unsigned exception;

    for (exception = 2u; exception < 16u; ++exception) {
        if (cpu->system_pending[exception] != 0u) return 1;
    }
    for (exception = 0u; exception < ARMV7M_IMPLEMENTED_IRQ_COUNT;
         ++exception) {
        if (cpu->irq_pending[exception] != 0u ||
            cpu->irq_level[exception] != 0u)
            return 1;
    }
    return 0;
}

void armv7m_exception_entered(semu_cpu *cpu, unsigned exception)
{
    if (exception >= 16u && exception < 16u + ARMV7M_IRQ_COUNT) {
        unsigned irq = exception - 16u;
        armv7m_clear_irq_pending(cpu, irq);
        cpu->irq_active[irq] = 1u;
    } else if (exception < 16u) {
        armv7m_clear_system_pending(cpu, exception);
        cpu->system_active[exception] = 1u;
    }
    if (cpu->exception_depth != UINT8_MAX) ++cpu->exception_depth;
}

void armv7m_exception_returned(semu_cpu *cpu, unsigned exception)
{
    if (exception >= 16u && exception < 16u + ARMV7M_IRQ_COUNT) {
        cpu->irq_active[exception - 16u] = 0u;
    } else if (exception < 16u) {
        cpu->system_active[exception] = 0u;
    }
    if (cpu->exception_depth != 0u) --cpu->exception_depth;
}

static uint32_t irq_bits(const semu_cpu *cpu, unsigned word, unsigned kind)
{
    uint32_t value = 0u;
    unsigned bit;

    for (bit = 0u; bit < 32u; ++bit) {
        unsigned irq = word * 32u + bit;
        int set = 0;
        if (irq >= ARMV7M_IMPLEMENTED_IRQ_COUNT) continue;
        if (kind == 0u) set = cpu->irq_enabled[irq] != 0u;
        else if (kind == 1u) set = cpu->irq_level[irq] != 0u ||
                                        cpu->irq_pending[irq] != 0u;
        else set = cpu->irq_active[irq] != 0u;
        if (set) value |= 1u << bit;
    }
    return value;
}

static void update_irq_bits(semu_cpu *cpu, unsigned word, uint32_t value,
                            unsigned kind, int set)
{
    unsigned bit;

    for (bit = 0u; bit < 32u; ++bit) {
        unsigned irq = word * 32u + bit;
        if (irq >= ARMV7M_IMPLEMENTED_IRQ_COUNT ||
            (value & (1u << bit)) == 0u) continue;
        if (kind == 0u) cpu->irq_enabled[irq] = set != 0 ? 1u : 0u;
        else if (kind == 1u) {
            if (set != 0) armv7m_set_irq_pending(cpu, irq);
            else armv7m_clear_irq_pending(cpu, irq);
        }
    }
}

static int in_range(uint32_t offset, uint32_t base, uint32_t size)
{
    return offset >= base && offset - base < size;
}

static int bit_register(uint32_t offset, unsigned *base, unsigned *kind)
{
    static const unsigned bases[] = {0x100u, 0x180u, 0x200u, 0x280u};
    unsigned index;

    for (index = 0u; index < sizeof(bases) / sizeof(bases[0]); ++index) {
        if (offset >= bases[index] && offset - bases[index] < 0x20u) {
            *base = bases[index];
            *kind = index;
            return 1;
        }
    }
    return 0;
}

semu_status armv7m_nvic_read(semu_cpu *cpu, uint32_t offset, unsigned width,
                             uint32_t *value, semu_error *error)
{
    unsigned word;
    unsigned index;

    if (bit_register(offset, &index, &word)) {
        if (!access_valid(offset, width)) return refuse(offset, error);
        *value = irq_bits(cpu, (offset - index) / 4u,
                          word < 2u ? 0u : 1u) >>
                 ((offset & 3u) * 8u);
        if (width != 4u) *value &= (1u << (width * 8u)) - 1u;
        return SEMU_OK;
    }
    if (in_range(offset, 0x300u, 0x20u)) {
        if (!access_valid(offset, width)) return refuse(offset, error);
        word = (offset - 0x300u) / 4u;
        *value = irq_bits(cpu, word, 2u) >> ((offset & 3u) * 8u);
        if (width != 4u) *value &= (1u << (width * 8u)) - 1u;
        return SEMU_OK;
    }
    if (in_range(offset, 0x400u, 0xf0u)) {
        if (!access_valid(offset, width)) return refuse(offset, error);
        *value = 0u;
        for (index = 0u; index < width; ++index) {
            unsigned irq = offset - 0x400u + index;
            if (irq < ARMV7M_IMPLEMENTED_IRQ_COUNT)
                *value |= (uint32_t)(cpu->irq_priority[irq] &
                                     ARMV7M_NVIC_PRIORITY_MASK) <<
                          (index * 8u);
        }
        return SEMU_OK;
    }
    return refuse(offset, error);
}

semu_status armv7m_nvic_write(semu_cpu *cpu, uint32_t offset, unsigned width,
                              uint32_t value, semu_error *error)
{
    unsigned word;
    unsigned base;
    unsigned index;
    uint32_t shifted;

    if (bit_register(offset, &base, &index)) {
        if (!access_valid(offset, width)) return refuse(offset, error);
        word = (offset - base) / 4u;
        shifted = value << ((offset & 3u) * 8u);
        update_irq_bits(cpu, word, shifted,
                        index < 2u ? 0u : 1u, index == 0u || index == 2u);
        return SEMU_OK;
    }
    if (in_range(offset, 0x400u, 0xf0u)) {
        if (!access_valid(offset, width)) return refuse(offset, error);
        for (index = 0u; index < width; ++index) {
            unsigned irq = offset - 0x400u + index;
            if (irq < ARMV7M_IMPLEMENTED_IRQ_COUNT)
                cpu->irq_priority[irq] = (uint8_t)(value >> (index * 8u)) &
                                         ARMV7M_NVIC_PRIORITY_MASK;
        }
        return SEMU_OK;
    }
    return refuse(offset, error);
}
