#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_water_tower_80182B5C[22];

// dryfield_night_water_tower
extern WorldCoordRoomLighting D_dryfield_night_water_tower_8017E74C[];

extern WorldCollisionRoomResources D_dryfield_night_water_tower_8017E754[];

extern u8* D_dryfield_night_water_tower_8017E764[];

extern ViewCount D_dryfield_night_water_tower_8017E768[];

extern DirectionWarpEntry D_dryfield_night_water_tower_8017E76C[];

extern ViewCamera D_dryfield_night_water_tower_8017F418[];

extern SpriteView D_dryfield_night_water_tower_80182040[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_water_tower_80182C30[];

/// Draws the night Water Tower's light glows for the current room view.
///
/// Every tick enables the room's view-effect gate. Views 2 and 5 draw one
/// flare; view 3 draws a shaft and a flare; view 4 adds a second flare;
/// views 7 and 10 layer two identical flares at one point. Other views draw
/// nothing. `task` is unused; the callback keeps no per-task state.
/// Requires a live session and room effect controller, the loaded room overlay,
/// current view transforms and flare texture, initialized scratch space, and
/// room in the frame's packet arena and ordering table. Accepted light points
/// must have nonzero camera-Z / 4 depth. Packets remain live until GPU completion.
void dryfieldNightWaterTowerDrawGlowsTask(Task* task);

/// Runs the night water-tower room's registration, idle or teardown state.
///
/// `task->state` must be 0..2: 0 installs the message table and registers the
/// task in `GAME_TASK_SLOT_ROOM`, 1 keeps it alive for messages, and 2 kills it.
/// The room overlay must remain loaded until the task is torn down. Spawn
/// arguments and task work are unused.
void dryfieldNightWaterTowerRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_H
