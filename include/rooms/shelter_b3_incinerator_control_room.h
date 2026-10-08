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

/// Runs control-room entry setup, message-service idle and teardown.
///
/// The map's room descriptor starts in state 0; valid states are 0 setup,
/// 1 idle and 2 release the task. Dispatch copies the three-entry table by
/// value and does not check the state. Setup installs the room message slot;
/// arrival warp 4 also starts the incinerator follow-up scene. The room overlay
/// and scene resources must remain loaded while their tasks use them.
void shelterB3IncineratorControlRoomTask(Task* task);

/// Runs the incinerator control room's telephone menu for the inventory dispatcher.
///
/// `spawnArg2.pointer` borrows a live UiObject; start its task in state 0.
/// Updates save/statistics child panels and reports completion through that
/// object for the inventory menu to close. States 0..3 and the room's menu
/// resources must remain available until the enclosing menu finishes.
void shelterB3IncineratorControlRoomTelephoneMenuTask(Task* task);

/// Draws the incinerator control room's fixed light glows for the mapped camera view.
///
/// Mapped views 2 through 6 and 8 select capsule glows; views 4 and 8
/// also draw a pulsing cyan diamond or disc. Other views emit no packets.
/// `task` is unused. Requires a composed view matrix, an initialized scratch
/// stack and the current frame's ordering table and packet arena. Queued
/// additive packets remain in that arena until GPU completion.
void shelterB3IncineratorControlRoomDrawViewGlowsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B3_INCINERATOR_CONTROL_ROOM_H
