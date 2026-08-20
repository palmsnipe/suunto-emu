#ifndef SEMU_SAPPORO_NEMA_GPU_INTERNAL_H
#define SEMU_SAPPORO_NEMA_GPU_INTERNAL_H

#include "sapporo_nema_gpu.h"
#include "../display/nema_completion.h"

const nema_completion *semu_nema_gpu_snapshot_completion(
    const semu_nema_gpu *gpu);

#endif
