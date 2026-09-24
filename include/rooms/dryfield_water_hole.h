#ifndef ROOMS_DRYFIELD_WATER_HOLE_H
#define ROOMS_DRYFIELD_WATER_HOLE_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// One rectangle of water surface drawn by `func_dryfield_water_hole_8017D898`,
/// in world coordinates: it spans `width` along X from `x` and `depth` along Z
/// from `z`, at height `y`. The table ends at the first entry whose `y` word is
/// -1; the drawing code reads only its low half as the height.
typedef struct DryfieldWaterHoleSurface {
    s16 x;
    s16 z;
    s16 width;
    u16 depth;
    s32 y;
} DryfieldWaterHoleSurface;

/// The room's water surfaces, terminated by an entry with `y == -1`.
extern DryfieldWaterHoleSurface D_dryfield_water_hole_8017FC98[];

/// Cursor into the primitive area the room's water surface is written to,
/// reset each frame to the half of that area belonging to the ordering table
/// being built.
extern u8* D_dryfield_water_hole_801828CC;

/// Frame counter the water surface's wave is phased by.
extern s16 D_dryfield_water_hole_801828D0;

/// The room task's three states, run from a stack copy by
/// `func_dryfield_water_hole_8017D840`: the entry tick, the idle state, then
/// `taskKill`.
extern const TaskFuncTable3 D_dryfield_water_hole_8017D5C4;

/// The water task's drawing state: draws the room's water surfaces into the
/// current ordering table.
void func_dryfield_water_hole_8017D898(Task* task);

/// The water task's first state.
void func_dryfield_water_hole_8017E000(Task* arg0);

/// Draws a glowing beam between two points of a coordinate's local space.
void func_dryfield_water_hole_8017E410(GsCOORDINATE2* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3);

/// Draws the flat textured flash quad of the room's flash effect.
void func_dryfield_water_hole_8017EDE4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);

/// Draw a spinning and an upright sprite of the room's particle effect.
void func_dryfield_water_hole_8017F5D4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_dryfield_water_hole_8017F9C0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);

#endif
