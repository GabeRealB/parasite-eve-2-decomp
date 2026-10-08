#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TANK_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TANK_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_water_tank_80180764[13];

// dryfield_night_water_tank
extern WorldCoordRoomLighting D_dryfield_night_water_tank_8017EE50[];

extern WorldCollisionRoomResources D_dryfield_night_water_tank_8017EE58[];

extern u8* D_dryfield_night_water_tank_8017EE74[];

extern ViewCount D_dryfield_night_water_tank_8017EE78[];

extern DirectionWarpEntry D_dryfield_night_water_tank_8017EE7C[];

extern ViewCamera D_dryfield_night_water_tank_8017F4D4[];

extern SpriteView D_dryfield_night_water_tank_801801CC[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_water_tank_80180890[];

/// No-op callback for the night water-tank room's effect task.
///
/// Gameplay selects effect bank 6, slot 0x111 for this room. The callback draws
/// nothing and leaves the task, its work and coordinate body unchanged; it does
/// not advance or release the task. The room overlay must remain loaded while
/// the task can invoke this callback.
void dryfieldNightWaterTankNoOpEffectTask(Task* unusedTask);

/// Runs the night water tank's room initialization, timer update and teardown.
///
/// State 0 installs the room handlers and prepares the tank and visit
/// actors; state 1 holds the Ice Bag timer on variant 11; state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldNightWaterTankRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TANK_H
