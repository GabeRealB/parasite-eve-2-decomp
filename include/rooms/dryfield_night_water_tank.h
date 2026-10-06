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

void func_dryfield_night_water_tank_8017D984(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TANK_H
