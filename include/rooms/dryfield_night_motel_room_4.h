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

/// Dryfield Night Motel Room 4's authored lighting and default ambient minima.
///
/// The night-Dryfield stage directory selects area 14's room slot 1 at room - 1.
/// Borrowed lighting storage remains writable and requires the room overlay loaded.
extern WorldCoordRoomLighting gDryfieldNightMotelRoom4RoomLighting[1];

extern WorldCollisionRoomResources D_dryfield_night_motel_room_4_8017DA98[];

/// The room's logical-view directory, borrowing gameplay's identity map for views 1..6.
///
/// Room slot 1 is indexed by room - 1; map bytes use logical view - 1 and hold
/// 1-based camera, image and sprite indices. The directory needs this overlay
/// loaded; the borrowed mapping needs gameplay loaded.
extern u8* gDryfieldNightMotelRoom4ViewMaps[1];

extern ViewCount D_dryfield_night_motel_room_4_8017DAAC[];

extern DirectionWarpEntry D_dryfield_night_motel_room_4_8017DAB0[];

extern ViewCamera D_dryfield_night_motel_room_4_8017E1F4[];

/// Sprite resources for mapped views 1..6, indexed by mapped view - 1.
///
/// View 1 has no source sprites. Views 2..6 contain 37, 83, 45, 88 and 50
/// sources; every view retains decoded background strips and has no draw-area
/// list. The records and their mutable sources and batches need this overlay
/// loaded while cached packets are built or sprites are drawn.
extern SpriteView gDryfieldNightMotelRoom4SpriteViews[6];

/// Collision policies and footstep cues indexed by room-local surface class 0..7.
///
/// Classes 0 and 4..7 share blocking policy without footstep cues. Classes 1
/// and 2 block with distinct footstep pairs; class 3 passes probes and ignores
/// weapon impacts without footstep cues. Every class applies pushback. Records
/// and sound tables are borrowed while this room overlay remains loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightMotelRoom4SurfaceProperties[8];

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
