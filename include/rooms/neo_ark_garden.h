#ifndef ROOMS_NEO_ARK_GARDEN_H
#define ROOMS_NEO_ARK_GARDEN_H

#include "common.h"

/// The block the garden's ambience task reaches through `Task::spawnArg2`.
/// Only `soundDelay` is read here; what precedes it belongs to whoever owns the
/// block, and the type's true size is not known.
typedef struct NeoArkGardenAmbience {
    u8  pad_0[0x24];
    s16 soundDelay; // Frames left before the view's loops are re-enqueued; set to 4 on every view change
} NeoArkGardenAmbience;

/// Queues one textured quad (tpage 0xAC, clut 0x43C0, 64x64 texels). The
/// unit corners are scaled by 250, rotated by the matrix `Gfx_RotMatrixX`
/// builds from `D_80070F70 << 7`, moved to `pos`, and projected through
/// `Gfx_ViewWorldMtx`. Nothing is queued when the GTE flag word is negative.
void func_neo_ark_garden_8017F42C(SVECTOR* pos);

#endif // ROOMS_NEO_ARK_GARDEN_H
