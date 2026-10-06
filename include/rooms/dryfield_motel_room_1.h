#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_1_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_1_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_motel_room_1_801814DC[13];

// dryfield_motel_room_1
extern WorldCollisionRoomResources D_dryfield_motel_room_1_8017E484[];

extern u8* D_dryfield_motel_room_1_8017E4B0[];

extern ViewCount D_dryfield_motel_room_1_8017E4B8[];

extern WorldCoordRoomLighting D_dryfield_motel_room_1_8017E4BC[];

extern DirectionWarpEntry D_dryfield_motel_room_1_8017E4CC[];

extern ViewCamera D_dryfield_motel_room_1_8017EAE0[];

extern SpriteView D_dryfield_motel_room_1_80180C90[];

extern WorldCollisionSurfaceProperties* D_dryfield_motel_room_1_8018157C[];

/// Inert callback for motel room 1's room-effect slot 0xC9 in task bank 6.
///
/// Ignores `unusedTask`, draws nothing and leaves the task live without
/// advancing its state. The task's resources remain owned until external teardown.
void dryfieldMotelRoom1NoOpEffectTask(Task* unusedTask);

void func_dryfield_motel_room_1_8017D754(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_1_H
