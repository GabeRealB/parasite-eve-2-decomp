#ifndef INCLUDE_ROOMS_DRYFIELD_WATER_TANK_H
#define INCLUDE_ROOMS_DRYFIELD_WATER_TANK_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_water_tank_80188BF0[13];

// dryfield_water_tank
extern WorldCollisionRoomResources D_dryfield_water_tank_801868E0[];

extern u8* D_dryfield_water_tank_801868F0[];

extern ViewCount D_dryfield_water_tank_801868F4[];

extern WorldCoordRoomLighting D_dryfield_water_tank_801868F8[];

extern DirectionWarpEntry D_dryfield_water_tank_80186900[];

extern ViewCamera D_dryfield_water_tank_80186EE0[];

extern SpriteView D_dryfield_water_tank_80187F80[];

extern WorldCollisionSurfaceProperties* D_dryfield_water_tank_80188CFC[];

/// Updates the ambient-effect gate for the current water-tank camera view.
///
/// Mapped view 9 publishes `ROOM_EFFECT_VIEW_ENABLED`; views 1..8 and 10 publish
/// `ROOM_EFFECT_VIEW_DISABLED`. Requires initialized `gRoomEffectState`, the
/// water-tank overlay and view resources to remain loaded, and a mapped index
/// in 1..10. The mapping and table lookup are unchecked. The task argument is
/// unused; the callback runs every frame without changing state or ending itself.
void dryfieldWaterTankUpdateViewEffectGateTask(Task* task);

/// Runs the water-tank room's initialization, ambience and teardown states.
///
/// State 0 installs room messages, registers the room task, spawns the room's
/// scene task, starts ambience and synchronizes mechanism sprites. State 1
/// updates view-specific ambience each frame; state 2 kills the task.
/// Requires `task->state` in 0..2 and the room overlay/resources to remain loaded.
void dryfieldWaterTankRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_WATER_TANK_H
