#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_GARAGE_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_GARAGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_dryfield_night_garage_80183380[2];

extern TaskDesc D_dryfield_night_garage_80181C2C;

extern AreaVariant D_dryfield_night_garage_801874BC[12];

// dryfield_night_garage
extern WorldCoordRoomLighting D_dryfield_night_garage_801833F4[];

extern WorldCollisionRoomResources D_dryfield_night_garage_80183404[];

extern u8* D_dryfield_night_garage_80183434[];

extern ViewCount D_dryfield_night_garage_8018343C[];

extern DirectionWarpEntry D_dryfield_night_garage_80183440[];

extern ViewCamera D_dryfield_night_garage_801843F8[];

extern SpriteView D_dryfield_night_garage_80186258[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_garage_801875B8[];

void func_dryfield_night_garage_80180414(s32 arg0);

/// Draws the night garage's additive grey capsule glows for the current view.
///
/// Gameplay effect bank 6, slot 0x114. Views 3 and 15 draw four strips; views
/// 7 and 14 draw the first strip; view 11 draws two strips with half-turned
/// capsule caps, sharing one strip with views 3 and 15. Other views draw none.
/// The callback ignores `unusedTask` and the spawn arguments. Requires the
/// loaded room overlay, composed view matrix and the current frame's
/// initialized scratch stack, ordering table and GPU packet arena.
void dryfieldNightGarageDrawGlowsTask(Task* unusedTask);

void func_dryfield_night_garage_801803BC(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_GARAGE_H
