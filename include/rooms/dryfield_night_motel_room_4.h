#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_4_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_4_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_motel_room_4_801802FC[13];

// dryfield_night_motel_room_4
extern WorldCoordRoomLighting D_dryfield_night_motel_room_4_8017DA90[];

extern WorldCollisionRoomResources D_dryfield_night_motel_room_4_8017DA98[];

extern u8* D_dryfield_night_motel_room_4_8017DAA8[];

extern ViewCount D_dryfield_night_motel_room_4_8017DAAC[];

extern DirectionWarpEntry D_dryfield_night_motel_room_4_8017DAB0[];

extern ViewCamera D_dryfield_night_motel_room_4_8017E1F4[];

extern SpriteView D_dryfield_night_motel_room_4_8017FB88[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_motel_room_4_8018039C[];

/// Draws Dryfield Night Motel Room 4's view-selected flickering flares each frame.
///
/// Views 2 and 3 draw two primary-texture flares, view 4 draws one, and view 5
/// draws a primary flare followed by a smaller secondary-texture flare.
/// Other views draw nothing. The task argument is ignored; no per-task state is kept.
/// Requires the room overlay, current view transform, scratch stack, ordering
/// table and packet arena to remain live for the frame. Each selected point
/// reserves a quad packet, including points rejected by depth clipping; queued
/// packets use the arena until GPU completion.
void dryfieldNightMotelRoom4DrawFlaresTask(Task* unusedTask);

/// Runs Dryfield Night Motel Room 4's message-driven room task.
///
/// Requires a live task with state 0 (register), 1 (idle) or 2 (release).
/// State 0 publishes its message table and registers it in `GAME_TASK_SLOT_ROOM`.
/// Keep the room overlay loaded while the task and its handlers are in use;
/// state 2 releases the task through `taskKill`.
void dryfieldNightMotelRoom4RoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_4_H
