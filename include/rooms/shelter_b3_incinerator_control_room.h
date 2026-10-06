#ifndef INCLUDE_ROOMS_SHELTER_B3_INCINERATOR_CONTROL_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B3_INCINERATOR_CONTROL_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b3_incinerator_control_room_80182610[11];

// shelter_b3_incinerator_control_room
extern u8* D_shelter_b3_incinerator_control_room_80181920[];

extern ViewCount D_shelter_b3_incinerator_control_room_80181928[];

extern DirectionWarpEntry D_shelter_b3_incinerator_control_room_8018192C[];

extern WorldCollisionGrid D_shelter_b3_incinerator_control_room_80181CC0;

extern ViewCamera D_shelter_b3_incinerator_control_room_80181CE4[];

extern SpriteView D_shelter_b3_incinerator_control_room_80182140[];

extern WorldCoordRoomLights D_shelter_b3_incinerator_control_room_801824A0;

extern WorldCollisionTrigger D_shelter_b3_incinerator_control_room_801824B8[];

extern WorldCollisionTrigger D_shelter_b3_incinerator_control_room_80182668[];

extern WorldCollisionTrigger D_shelter_b3_incinerator_control_room_801827E4[];

extern WorldCollisionOccluder D_shelter_b3_incinerator_control_room_801829AC[];

extern WorldCollisionSurfaceProperties* D_shelter_b3_incinerator_control_room_80182A20[];

void func_shelter_b3_incinerator_control_room_8017FCB8(Task* task);

void func_shelter_b3_incinerator_control_room_8017EA64(Task* task);

/// Draws the incinerator control room's fixed light glows for the mapped camera view.
///
/// Mapped views 2 through 6 and 8 select capsule glows; views 4 and 8
/// also draw a pulsing cyan diamond or disc. Other views emit no packets.
/// `task` is unused. Requires a composed view matrix, an initialized scratch
/// stack and the current frame's ordering table and packet arena. Queued
/// additive packets remain in that arena until GPU completion.
void shelterB3IncineratorControlRoomDrawViewGlowsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B3_INCINERATOR_CONTROL_ROOM_H
