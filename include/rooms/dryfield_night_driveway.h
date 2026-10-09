#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_DRIVEWAY_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_DRIVEWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_driveway_80181F4C[22];

/// Model lighting for room IDs 1 and 2 of the nighttime driveway.
///
/// Both descriptors borrow the same eight mutable point lights and ten-view
/// ambient table. Keep this overlay loaded throughout lighting updates.
extern WorldCoordRoomLighting gDryfieldNightDrivewayRoomLighting[2];

extern WorldCollisionRoomResources D_dryfield_night_driveway_801805F0[];

/// Logical-view maps for room IDs 1 and 2 of the nighttime driveway.
///
/// Room 1 borrows gameplay's identity map. Room 2 maps logical views 1..10
/// to camera/image/sprite indices 1,9,10,4,5,6,4,8,2,3 respectively.
/// Each room searches ten entries; borrowed room storage requires this overlay.
extern u8* gDryfieldNightDrivewayViewMaps[2];

extern ViewCount D_dryfield_night_driveway_80180624[];

extern DirectionWarpEntry D_dryfield_night_driveway_80180628[];

extern ViewCamera D_dryfield_night_driveway_80180C30[];

/// Sprite resources indexed by the driveway's mapped view minus one (views 1..10).
///
/// Views 2/3/4/5/9/10 select 9/17/54/12/9/17 source rectangles; the other
/// views have empty lists. All retain the decoded background. Views 3 and 10
/// also select a clipping list. Sources and batches remain mutable and require
/// the room overlay to stay loaded while cached packets or clip commands use them.
extern SpriteView gDryfieldNightDrivewaySpriteViews[10];

/// Nighttime driveway collision policies indexed directly by surface class 0..7.
///
/// Classes 0 and 3..7 block probes, permit weapon impacts and have no footstep
/// cues. Class 1 passes probes and ignores weapon impacts. Class 2 blocks
/// probes, permits impacts and supplies footstep cues. Every class applies
/// pushback. The records and sound bases belong to this loaded room overlay.
extern WorldCollisionSurfaceProperties* gDryfieldNightDrivewaySurfacePropertyTable[8];

/// Runs the night driveway's room-message task.
///
/// Requires state 0..2: install the receiver and optional companion arrival
/// scene, idle while messages handle requests, then teardown. Keep the room
/// overlay loaded throughout dispatch and any event playback.
void dryfieldNightDrivewayRoomTask(Task* task);

/// Draws the night driveway's view-selected light shafts each frame.
///
/// Enables the ambient-effect gate even in views that draw no shafts. Views
/// 2/9 draw the first stored point pair, 4/7 the second, 5 the third, and 3/10
/// both the first and second. Other views draw none. Ignores `unusedTask`.
/// Requires live room-effect state, composed view matrices, initialized scratch
/// storage and the current frame's packet arena and ordering table. Accepted
/// projected endpoints must have nonzero camera Z / 4; queued packets remain
/// in the frame arena until GPU completion.
void dryfieldNightDrivewayDrawLightShaftsTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_DRIVEWAY_H
