#ifndef ROOMS_NEO_ARK_GARDEN_H
#define ROOMS_NEO_ARK_GARDEN_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

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

/// Draws a pulsing red marker at the world-space point `pos`; `speed` scales
/// the pulse rate and `size` is the marker's signed half-extent.
void func_neo_ark_garden_8017EFB8(SVECTOR* pos, s16 speed, s32 size);

/// Draws a gouraud ring at the coordinate: one edge, black, at half-extent
/// `r`, the other at `r + width` in the colour `rgb`.
void func_neo_ark_garden_80180190(GsCOORDINATE2* coord, s32 r, s32 width, u8* rgb);

/// Draws a glow disc of half-extent `r` at the coordinate, `rgb` at its
/// centre fading to black at the rim.
void func_neo_ark_garden_801805B4(GsCOORDINATE2* coord, s32 r, u8* rgb);

/// Draws a flickering flat quad of half-extent `size` at the coordinate.
void func_neo_ark_garden_80181020(GsCOORDINATE2* coord, s32 size);

#endif // ROOMS_NEO_ARK_GARDEN_H
