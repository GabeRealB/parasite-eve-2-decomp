#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_G_R_KITCHEN_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_G_R_KITCHEN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_g_r_kitchen_8017EB88[12];

// dryfield_night_g_r_kitchen
extern WorldCoordRoomLighting D_dryfield_night_g_r_kitchen_8017E2BC[];

extern WorldCollisionRoomResources D_dryfield_night_g_r_kitchen_8017E2C4[];

extern u8* D_dryfield_night_g_r_kitchen_8017E2D4[];

extern ViewCount D_dryfield_night_g_r_kitchen_8017E2D8[];

extern DirectionWarpEntry D_dryfield_night_g_r_kitchen_8017E2DC[];

extern ViewCamera D_dryfield_night_g_r_kitchen_8017E578[];

extern SpriteView D_dryfield_night_g_r_kitchen_8017E6A8[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_g_r_kitchen_8017EC04[];

/// Draws the nighttime kitchen's two grey light shafts for view 2 or 3 each frame.
///
/// Other views emit no packets. The task argument is unused. Requires the room
/// overlay to remain loaded, a composed view, an initialized scratch stack and
/// space in the current frame's primitive arena and depth ordering table.
void dryfieldNightGRKitchenDrawLightShaftsTask(Task* unusedTask);

void func_dryfield_night_g_r_kitchen_8017D9A4(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_G_R_KITCHEN_H
