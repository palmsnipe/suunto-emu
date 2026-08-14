#ifndef SEMU_FRONTEND_CLI_H
#define SEMU_FRONTEND_CLI_H

#include "semu/frame.h"

int semu_cli_main(int argc, char **argv, semu_frame_callback frame_callback,
                  void *frame_context);

#endif
