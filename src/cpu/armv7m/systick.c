#include "armv7m_internal.h"

#define SYSTICK_CTRL 0x010u
#define SYSTICK_RELOAD 0x014u
#define SYSTICK_CURRENT 0x018u
#define SYSTICK_CALIB 0x01cu
#define SYSTICK_ENABLE (1u << 0)
#define SYSTICK_TICKINT (1u << 1)
#define SYSTICK_CLKSOURCE (1u << 2)
#define SYSTICK_COUNTFLAG (1u << 16)

static semu_status refuse(uint32_t offset, semu_error *error)
{
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "armv7m SysTick refuses offset 0x%03x", offset);
    return SEMU_ERR_UNSUPPORTED;
}

static int access_valid(uint32_t offset, unsigned width)
{
    return (width == 1u || width == 2u || width == 4u) &&
           (offset & (width - 1u)) == 0u &&
           (offset & 3u) + width <= 4u;
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

static uint32_t access_mask(uint32_t offset, unsigned width)
{
    uint32_t mask = width == 4u ? UINT32_MAX : (1u << (width * 8u)) - 1u;
    return mask << ((offset & 3u) * 8u);
}

static void update_current(semu_cpu *cpu)
{
    uint64_t now;
    uint64_t elapsed;

    if (cpu->systick_event_valid == 0u || cpu->scheduler == NULL) return;
    now = semu_scheduler_now(cpu->scheduler);
    if (now < cpu->systick_last_time) {
        cpu->systick_last_time = now;
        return;
    }
    elapsed = now - cpu->systick_last_time;
    if (elapsed < cpu->systick_current)
        cpu->systick_current -= (uint32_t)elapsed;
    else
        cpu->systick_current = 0u;
    cpu->systick_last_time = now;
}

static void systick_callback(void *context, uint64_t now_ns)
{
    semu_cpu *cpu = (semu_cpu *)context;

    (void)now_ns;
    cpu->systick_event_valid = 0u;
    if ((cpu->systick_control & (SYSTICK_ENABLE | SYSTICK_CLKSOURCE)) !=
        (SYSTICK_ENABLE | SYSTICK_CLKSOURCE)) {
        return;
    }
    cpu->systick_current = cpu->systick_reload;
    cpu->systick_last_time = now_ns;
    cpu->systick_countflag = 1u;
    if ((cpu->systick_control & SYSTICK_TICKINT) != 0u)
        armv7m_set_system_pending(cpu, 15u);
    armv7m_systick_reschedule(cpu);
}

static semu_status schedule_timer(semu_cpu *cpu, semu_error *error)
{
    uint64_t delay;

    if ((cpu->systick_control & (SYSTICK_ENABLE | SYSTICK_CLKSOURCE)) !=
        (SYSTICK_ENABLE | SYSTICK_CLKSOURCE) || cpu->scheduler == NULL) {
        return SEMU_OK;
    }
    if (cpu->systick_current == 0u && cpu->systick_reload == 0u)
        return SEMU_OK;
    delay = cpu->systick_current == 0u ? 1u : cpu->systick_current;
    return semu_scheduler_schedule(cpu->scheduler, delay, systick_callback,
                                   cpu, &cpu->systick_event, error) == SEMU_OK
               ? (cpu->systick_event_valid = 1u, SEMU_OK)
               : error != NULL ? error->code : SEMU_ERR_STATE;
}

void armv7m_systick_reschedule(semu_cpu *cpu)
{
    semu_error error;

    if (cpu == NULL) return;
    if (cpu->systick_event_valid != 0u) {
        (void)semu_scheduler_cancel(cpu->scheduler, cpu->systick_event);
        cpu->systick_event_valid = 0u;
    }
    semu_error_clear(&error);
    if (schedule_timer(cpu, &error) != SEMU_OK) {
        cpu->state.halted = 1;
        cpu->stop_reason = SEMU_STOP_DEVICE_REFUSED;
    }
}

void armv7m_systick_reset(semu_cpu *cpu)
{
    if (cpu == NULL) return;
    if (cpu->systick_event_valid != 0u)
        (void)semu_scheduler_cancel(cpu->scheduler, cpu->systick_event);
    cpu->systick_control = 0u;
    cpu->systick_reload = 0u;
    cpu->systick_current = 0u;
    cpu->systick_calibration = 0u;
    cpu->systick_last_time = 0u;
    cpu->systick_countflag = 0u;
    cpu->systick_event = 0u;
    cpu->systick_event_valid = 0u;
}

semu_status armv7m_systick_read(semu_cpu *cpu, uint32_t offset,
                                unsigned width, uint32_t *value,
                                semu_error *error)
{
    uint32_t raw;

    if (cpu == NULL || value == NULL || !access_valid(offset, width) ||
        offset < SYSTICK_CTRL || offset > SYSTICK_CALIB)
        return refuse(offset, error);
    switch (offset & ~3u) {
    case SYSTICK_CTRL:
        raw = cpu->systick_control |
              (cpu->systick_countflag != 0u ? SYSTICK_COUNTFLAG : 0u);
        *value = read_lane(raw, offset, width);
        cpu->systick_countflag = 0u;
        semu_error_clear(error);
        return SEMU_OK;
    case SYSTICK_RELOAD: raw = cpu->systick_reload; break;
    case SYSTICK_CURRENT: raw = cpu->systick_current; break;
    case SYSTICK_CALIB: raw = cpu->systick_calibration; break;
    default: return refuse(offset, error);
    }
    if ((offset & ~3u) == SYSTICK_CURRENT) {
        update_current(cpu);
        raw = cpu->systick_current;
    }
    *value = read_lane(raw, offset, width);
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status armv7m_systick_write(semu_cpu *cpu, uint32_t offset,
                                 unsigned width, uint32_t value,
                                 semu_error *error)
{
    uint32_t bits;
    uint32_t mask;
    uint32_t old_control;
    uint32_t old_reload;
    uint32_t old_current;
    uint64_t old_last_time;
    uint8_t old_countflag;
    semu_status status;

    if (cpu == NULL || !access_valid(offset, width) ||
        offset < SYSTICK_CTRL || offset > SYSTICK_CALIB)
        return refuse(offset, error);
    bits = write_lane(value, offset);
    mask = access_mask(offset, width);
    old_control = cpu->systick_control;
    old_reload = cpu->systick_reload;
    update_current(cpu);
    old_current = cpu->systick_current;
    old_last_time = cpu->systick_last_time;
    old_countflag = cpu->systick_countflag;
    if ((offset & ~3u) == SYSTICK_CALIB)
        return refuse(offset, error);
    if ((offset & ~3u) == SYSTICK_CTRL) {
        uint32_t candidate = (cpu->systick_control & ~mask) |
                             (bits & mask);
        candidate &= SYSTICK_ENABLE | SYSTICK_TICKINT | SYSTICK_CLKSOURCE;
        if ((candidate & SYSTICK_ENABLE) != 0u &&
            (candidate & SYSTICK_CLKSOURCE) == 0u) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "SysTick external clock source is unsupported");
            return SEMU_ERR_UNSUPPORTED;
        }
    }
    if (cpu->systick_event_valid != 0u) {
        (void)semu_scheduler_cancel(cpu->scheduler, cpu->systick_event);
        cpu->systick_event_valid = 0u;
    }
    cpu->systick_last_time = semu_scheduler_now(cpu->scheduler);
    switch (offset & ~3u) {
    case SYSTICK_CTRL:
        cpu->systick_control = (cpu->systick_control & ~mask) |
                               (bits & mask);
        cpu->systick_control &= SYSTICK_ENABLE | SYSTICK_TICKINT |
                                SYSTICK_CLKSOURCE;
        if ((old_control & SYSTICK_ENABLE) == 0u &&
            (cpu->systick_control & SYSTICK_ENABLE) != 0u) {
            cpu->systick_current = cpu->systick_reload;
            cpu->systick_last_time = semu_scheduler_now(cpu->scheduler);
        }
        break;
    case SYSTICK_RELOAD:
        cpu->systick_reload = ((cpu->systick_reload & ~mask) |
                               (bits & mask)) & 0x00ffffffu;
        break;
    case SYSTICK_CURRENT:
        cpu->systick_current = 0u;
        cpu->systick_last_time = semu_scheduler_now(cpu->scheduler);
        cpu->systick_countflag = 0u;
        break;
    case SYSTICK_CALIB: return refuse(offset, error);
    default:
        return refuse(offset, error);
    }
    semu_error_clear(error);
    status = schedule_timer(cpu, error);
    if (status != SEMU_OK) {
        cpu->systick_control = old_control;
        cpu->systick_reload = old_reload;
        cpu->systick_current = old_current;
        cpu->systick_last_time = old_last_time;
        cpu->systick_countflag = old_countflag;
        (void)schedule_timer(cpu, error);
        return status;
    }
    if (cpu->systick_event_valid == 0u)
        cpu->systick_event = 0u;
    return SEMU_OK;
}
