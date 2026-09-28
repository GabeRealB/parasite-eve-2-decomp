#ifndef GAMEPLAY_PRIVATE_GEOMETRY_H
#define GAMEPLAY_PRIVATE_GEOMETRY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// SVECTOR layout with unsigned X/Z so `Gp_YawToPosXZ` emits `lhu`.
typedef struct _GpPosXZ {
    /* 0x0 */ u16 vx;
    /* 0x2 */ u16 pad_2;
    /* 0x4 */ u16 vz;
    /* 0x6 */ u16 pad_6;
} GpPosXZ;
STATIC_ASSERT_SIZEOF(GpPosXZ, 8);

/// 0x18-byte scratch from `G_SCRATCH_HEAD` used by `Gp_GetObjPan`,
/// `Gp_DebugPanTask` and `Gp_DrawTargetCursor`: one world point projected to
/// the screen, with the depth the projection returned kept beside the screen
/// position. `GpPerspScratch` is the shorter block a projection carves when
/// the screen position is not kept in it.
typedef struct {
    SVECTOR vec;  // the point projected, in the space of the matrix the GTE holds
    s32     dp;   // depth-cue coefficient of the projection (`gte_stdp`)
    s32     flag; // projection status (`gte_stflg`); a negative value leaves no usable position
    s32     otz;  // distance of the point (`gte_stszotz`, `SZ3 >> 2`)
    s16     sx;   // screen X the point landed on (`gte_stsxy`)
    s16     sy;   // screen Y the point landed on
} _GpPanScratch;
STATIC_ASSERT_SIZEOF(_GpPanScratch, 0x18);

#endif // GAMEPLAY_PRIVATE_GEOMETRY_H
