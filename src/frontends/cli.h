#ifndef SEMU_FRONTEND_CLI_H
#define SEMU_FRONTEND_CLI_H

#include "semu/frame.h"
#include "semu/machine.h"

int semu_cli_main(int argc, char **argv, semu_frame_callback frame_callback,
                  void *frame_context,
                  semu_machine_input_poll_fn input_poll,
                  void *input_poll_context);

#endif
