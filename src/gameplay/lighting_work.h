#ifndef GAMEPLAY_PRIVATE_LIGHTING_WORK_H
#define GAMEPLAY_PRIVATE_LIGHTING_WORK_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// Projected screen position of a target bound to a lock slot.
///
/// The projection writes both pixels as one word. The slot's on-screen number
/// reads the two signed components when it places itself beside the target.
/// The origin is the screen center and Y increases downward. Storing either
/// member replaces the other.
typedef union {
    DVECTOR xy;     // Signed screen pixels from the center (vx right, vy down)
    s32     packed; // Both pixels in the one word the projection stores
} WorldTargetScreenPos;
STATIC_ASSERT_SIZEOF(WorldTargetScreenPos, 4);

/// 0xC slot in the 32-entry table at `Gp_LockSlots`. `Gp_ClearLockSlots` clears
/// `field_0` / `field_4` / `field_6`. `func_800DA6E8` binds `field_0` and
/// bumps `field_4`; `Gp_UpdateLockSlots` treats `field_6` as a countdown and
/// stores projected screen XY at 0x8.
typedef struct _GpSlot70 {
    /* 0x0 */ void*                field_0;
    /* 0x4 */ s16                  field_4;
    /* 0x6 */ s16                  field_6;
    /* 0x8 */ WorldTargetScreenPos screen;
} GpSlot70;
STATIC_ASSERT_SIZEOF(GpSlot70, 0xC);

/// 0x14-byte scratch from the scratch stack used by `Gp_ProjectToSxy`.
/// `vec` is the packed `SVECTOR` fed to RTPS. `p` / `flag` / `otz` hold
/// IR0, FLAG, and `SZ3 >> 2`.
typedef struct _GpPerspScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     p;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     otz;
} GpPerspScratch;
STATIC_ASSERT_SIZEOF(GpPerspScratch, 0x14);

#endif // GAMEPLAY_PRIVATE_LIGHTING_WORK_H
