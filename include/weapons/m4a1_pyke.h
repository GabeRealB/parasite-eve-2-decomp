#ifndef M4A1_PYKE_H
#define M4A1_PYKE_H

#include "common.h"
#include <psyq/libgte.h>

#include "gameplay/3A34.h"
#include "main/session.h"

/// 0x38 block the flying dart's spawn state allocates with `Mem_Calloc` and
/// parks in `Task::idMap`. It leads with the `GpObj` list node
/// `WeaponsShared8011e4ac` hands back to `Gp_UnlinkObj` on teardown; `rec` is
/// the single-entry `GpRec18` collision table `obj.field_C` points at, and its
/// `field_0` is set to 2 (the last-element bit) instead of going through
/// `Gp_InitRec18Table`.
typedef struct M4a1PykeBeam {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec[1];
} M4a1PykeBeam;
STATIC_ASSERT_SIZEOF(M4a1PykeBeam, 0x38);

/// Translation of the Pyke's effect coordinate frame inside its parent frame
/// (the muzzle), `(0, 0x200, 0x40)`.
extern SVECTOR D_m4a1_pyke_8011E90C;

/// Handwritten GTE routine. Projects the world-space point `pos` through
/// `GsWSMATRIX` and links one frame of the beam sprite there; `frame` picks the
/// sprite and `brightness` scales its colour.
void func_m4a1_pyke_8011D548(VECTOR3* pos, u16 frame, s32 brightness);

/// Draws one frame of the flying dart at `pos`. `frame` walks the sprite,
/// `width` comes from the dart's flare width and `ang` from its spin angle.
void func_m4a1_pyke_8011DCEC(VECTOR3* pos, u16 frame, u16 width, s16 ang);

/// Spawns the dart's ground splash at `pos`, scaled by `width`.
void func_m4a1_pyke_8011E168(VECTOR3* pos, s16 width);

#endif
