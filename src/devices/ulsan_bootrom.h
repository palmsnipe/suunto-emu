#ifndef SEMU_DEVICES_ULSAN_BOOTROM_H
#define SEMU_DEVICES_ULSAN_BOOTROM_H

#include "semu/bus.h"

/*
 * Ulsan 2.35.36 bootrom stub block (ticket 730, E-ULS-0009).
 *
 * The reference lane maps Memory.MappedMemory at 0x08000000 size 0x1000
 * whose entire content is the WriteWord/WriteDoubleWord list declared in
 * the platform description (stub bootrom functions: delay at 0x9C,
 * read_word at 0x74, program_main2 at 0x6C/0x200/0x220, unimplemented-
 * function handler at 0x30 whose logger_address literal is 0x07FFFFFC);
 * every other byte of the block is zero-initialized in the lane. The
 * BootromLogger sits at 0x07FFFFFC; the handler writes the caller address
 * there and jumps to it, which the lane treats as a fatal abort. This
 * device serves the declared bytes read-only and fails closed on writes
 * and on any BootromLogger access, which is the tree's abort-equivalent.
 */

semu_status semu_ulsan_bootrom_map(semu_bus *bus, semu_error *error);

#endif
