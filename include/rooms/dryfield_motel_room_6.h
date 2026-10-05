#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_6_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_6_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_dryfield_motel_room_6_80182D0C[2];

extern AreaVariant D_dryfield_motel_room_6_80186764[12];

// dryfield_motel_room_6
extern WorldCollisionRoomResources D_dryfield_motel_room_6_80182D98[];

extern u8* D_dryfield_motel_room_6_80182DA8[];

extern ViewCount D_dryfield_motel_room_6_80182DAC[];

extern WorldCoordRoomLighting D_dryfield_motel_room_6_80182DB0[];

extern DirectionWarpEntry D_dryfield_motel_room_6_80182DB8[];

extern ViewCamera D_dryfield_motel_room_6_80183840[];

extern SpriteView D_dryfield_motel_room_6_801856CC[];

extern WorldCollisionSurfaceProperties* D_dryfield_motel_room_6_80186808[];

void func_dryfield_motel_room_6_8017EA58(Task* task);

/// Initializes and updates the player's reflection in daytime motel room 6.
///
/// State must be 0 (start bodyless) or 1 (initialized model and work).
/// `spawnArg1.value` selects the floor (0) or room mirror plane (1).
/// Requires a live player with a TMD body. The task owns its cloned model and
/// work, borrows the player's geometry, and is parented to the player.
/// Keep this overlay loaded while the reflection and its attachment tasks live.
void dryfieldMotelRoom6PlayerReflectionTask(Task* reflectionTask);
void motelRoom6DayDrawGlow(Task* unused);

void func_dryfield_motel_room_6_80181B18(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_6_H
