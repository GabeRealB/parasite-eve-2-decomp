#ifndef INCLUDE_ROOMS_DRYFIELD_R08_H
#define INCLUDE_ROOMS_DRYFIELD_R08_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_r08_80180B88[13];

// dryfield_r08
extern GpRoomObjRec D_dryfield_r08_8017F6DC[];

extern u8* D_dryfield_r08_8017F6FC[];

extern GpViewCountRec D_dryfield_r08_8017F704[];

extern WorldCoordRoomLighting D_dryfield_r08_8017F708[];

extern GpWarpRec D_dryfield_r08_8017F718[];

extern GpViewRec D_dryfield_r08_8017FBBC[];

extern SpriteView D_dryfield_r08_80180918[];

extern WorldCollisionSurfaceProperties* D_dryfield_r08_80180C04[];

void func_dryfield_r08_8017F334(s32 arg0);

void func_dryfield_r08_8017F340(u8 arg0, u8 arg1);

void func_dryfield_r08_8017F438(s16 arg0);

void func_dryfield_r08_8017D5F8(Task* task);

/// Advances Dryfield R08's drifting sprite, with an unbanked redraw during suspension.
///
/// Requires a live coordinate-body task with initialized `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` packs size in bits 0..11, period in
/// 12..14 (1 only when bits 12..15 are all zero), speed in 16..23 (0 means 64),
/// movement kind in 24..27, palette bank in 28..30, and alternate drawer in 31.
/// A nonzero period nibble must encode a nonzero period. Spin uses 4096 units
/// per turn; movement uses coordinate units per running update.
/// Initializes without drawing, then draws and advances through 12 banked or
/// 10 alternate cells. Nonzero effect control freezes updates and redraws the
/// banked sheet with palette zero; control >= 4 releases work and task after
/// that draw. Drawing requires composed coordinates, scratch and packet space.
void dryfieldR08SpriteDriftTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_R08_H
