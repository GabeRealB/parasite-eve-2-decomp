#ifndef ROOMS_DRYFIELD_TOILET_H
#define ROOMS_DRYFIELD_TOILET_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Per-frame scratch the room's animated billboard task reserves off
/// `G_SCRATCH_HEAD`. `vec` is the task coordinate's world translation, projected
/// through `GsWSMATRIX` into `sxy` with `otz` as the resulting depth. `dx` / `dy`
/// are the rotated half-extents that offset `sxy` into the quad's corners, so
/// the sprite shrinks with distance. The same layout appears in other rooms'
/// billboard tasks; whether they are one type is unsettled.
typedef struct DryfieldToiletSpriteScratch {
    s32     otz;
    s32     dx;
    s32     dy;
    SVECTOR vec;
    DVECTOR sxy;
} DryfieldToiletSpriteScratch;
STATIC_ASSERT_SIZEOF(DryfieldToiletSpriteScratch, 0x18);

/// Queues a semi-transparent textured square centred on the projected world
/// position of `arg0`: half-size `arg2` scaled by depth, animation frame
/// `arg1 & 3`, grey level `arg3`.
void func_dryfield_toilet_8017EE18(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);

/// Queues a gouraud ring of sixteen quads around the projected world position
/// of `arg0`, black at radius `arg1` and shaded `rgb` at `arg1 + arg2`.
void func_dryfield_toilet_8017F09C(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

/// Queues a gouraud disc of eight wedges around the projected world position
/// of `arg0`, shaded `rgb` at the centre and black at radius `arg1`.
void func_dryfield_toilet_8017F4C0(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);

/// Queues a semi-transparent textured quad lying flat at the world position of
/// `arg0`, half-size `arg1`, alternating between two texture frames.
void func_dryfield_toilet_8017FF2C(GsCOORDINATE2* arg0, s32 arg1);

#endif // ROOMS_DRYFIELD_TOILET_H
