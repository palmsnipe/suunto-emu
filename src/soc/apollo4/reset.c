#include "reset.h"

#include <stdlib.h>

/*
* E-A4-RST-001 verified trace (sapporo-apollo4-coldboot-clock-reset-unpatched):
*   Two complete SYSRESETREQ cycles via AIRCR 0x05FA0004.
*   Reset re-entry at handoff 0x001A2430 / reset-body 0x001A2438 is deterministic.
* The reset controller coordinates ordered callbacks; it is not an MMIO device.
* Bus read/write always refuse; reset runs callbacks in registration order and
* stops on the first failure without continuing.
*/

typedef struct reset_callback_entry {
    semu_apollo4_reset_callback callback;
    void *context;
} reset_callback_entry;

struct semu_apollo4_reset_controller {
    semu_bus *bus;
    semu_scheduler *scheduler;
    reset_callback_entry callbacks[SEMU_APOLLO4_RESET_MAX_CALLBACKS];
    size_t callback_count;
};

static const semu_bus_device_ops reset_ops = {
    semu_apollo4_reset_controller_read,
    semu_apollo4_reset_controller_write,
    semu_apollo4_reset_controller_reset
};

semu_apollo4_reset_controller *semu_apollo4_reset_controller_create(
    semu_bus *bus, semu_scheduler *scheduler, semu_error *error)
{
    semu_apollo4_reset_controller *controller;

    if (bus == NULL || scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 reset controller requires bus and scheduler");
        return NULL;
    }
    controller = (semu_apollo4_reset_controller *)
        calloc(1u, sizeof(*controller));
    if (controller == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate Apollo4 reset controller");
        return NULL;
    }
    controller->bus = bus;
    controller->scheduler = scheduler;
    semu_error_clear(error);
    return controller;
}

void semu_apollo4_reset_controller_destroy(
    semu_apollo4_reset_controller *controller)
{
    free(controller);
}

semu_status semu_apollo4_reset_controller_register(
    semu_apollo4_reset_controller *controller,
    semu_apollo4_reset_callback callback, void *callback_context,
    semu_error *error)
{
    if (controller == NULL || callback == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "reset callback registration requires controller and callback");
        return SEMU_ERR_ARGUMENT;
    }
    if (controller->callback_count >= SEMU_APOLLO4_RESET_MAX_CALLBACKS) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "Apollo4 reset callback table is full");
        return SEMU_ERR_RANGE;
    }
    controller->callbacks[controller->callback_count].callback = callback;
    controller->callbacks[controller->callback_count].context = callback_context;
    ++controller->callback_count;
    semu_error_clear(error);
    return SEMU_OK;
}

void semu_apollo4_reset_controller_reset(void *context)
{
    semu_apollo4_reset_controller *controller =
        (semu_apollo4_reset_controller *)context;
    size_t i;

    if (controller == NULL) {
        return;
    }
    for (i = 0u; i < controller->callback_count; ++i) {
        semu_error local_error;
        semu_status status;
        semu_error_clear(&local_error);
        status = controller->callbacks[i].callback(
            controller->callbacks[i].context, &local_error);
        if (status != SEMU_OK) {
            return;
        }
    }
}

semu_status semu_apollo4_reset_controller_read(void *context, uint32_t offset,
                                                unsigned width, uint32_t *value,
                                                semu_error *error)
{
    (void)context;
    (void)offset;
    (void)width;
    (void)value;
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Apollo4 reset controller has no readable registers");
    return SEMU_ERR_UNSUPPORTED;
}

semu_status semu_apollo4_reset_controller_write(void *context, uint32_t offset,
                                                 unsigned width, uint32_t value,
                                                 semu_error *error)
{
    (void)context;
    (void)offset;
    (void)width;
    (void)value;
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Apollo4 reset controller has no writable registers");
    return SEMU_ERR_UNSUPPORTED;
}

const semu_bus_device_ops *semu_apollo4_reset_controller_bus_ops(void)
{
    return &reset_ops;
}
