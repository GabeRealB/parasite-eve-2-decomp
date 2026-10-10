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

/// Collision resources for the night Water Tower's only room (room ID 1).
///
/// Stage 3 area 20 borrows this room array. Its grid and both trigger arrays
/// are writable during binding and contact dispatch; there are no occluders.
/// Keep the overlay loaded until the grid is retired and triggers are unlinked.
extern WorldCollisionRoomResources gDryfieldNightWaterTowerCollisionRooms[1];

extern u8* D_dryfield_night_water_tower_8017E764[];

/// Ten logical views searched through the identity map for night Water Tower room 1.
///
/// This signed halfword count limits reverse lookup; it is not the identity
/// map's capacity. The stage map borrows the one-element array while this
/// room overlay is loaded; there is no terminator.
extern ViewCount gDryfieldNightWaterTowerViewCounts[1];

/// Four night Water Tower warp endpoints, indexed by warp ID minus one.
///
/// IDs 1..4 serve the G & R kitchen, breezeway, water tank and motel room 6,
/// respectively. Entries supply world-coordinate spawns, yaw in 4096 units
/// per turn, the initial logical view, and departure/arrival effects. The
/// last two endpoints fade on departure. There is no terminator; the stage
/// directory borrows the array while this overlay is loaded.
extern DirectionWarpEntry gDryfieldNightWaterTowerWarpEntries[4];

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
