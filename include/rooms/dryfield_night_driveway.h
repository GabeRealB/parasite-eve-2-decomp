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

/// Collision resources for nighttime driveway room IDs 1 and 2.
///
/// Both rooms borrow the same mesh, six view boundaries, four action triggers
/// and two sight occluders. Keep this overlay loaded while its grid is active
/// or any mutable trigger/occluder records remain linked.
extern WorldCollisionRoomResources gDryfieldNightDrivewayRoomCollisionResources[2];

/// Logical-view maps for room IDs 1 and 2 of the nighttime driveway.
///
/// Room 1 borrows gameplay's identity map. Room 2 maps logical views 1..10
/// to camera/image/sprite indices 1,9,10,4,5,6,4,8,2,3 respectively.
/// Each room searches ten entries; borrowed room storage requires this overlay.
extern u8* gDryfieldNightDrivewayViewMaps[2];

/// Logical-view search counts for driveway room IDs 1 and 2: ten entries each.
///
/// Signed halfword counts bound reverse searches in `gDryfieldNightDrivewayViewMaps`;
/// they are independent of the maps' storage capacity. The stage directory
/// borrows this array while the room overlay is loaded.
extern ViewCount gDryfieldNightDrivewayViewCounts[2];

/// Four arrival/departure endpoints for nighttime driveway warp IDs 1..4.
///
/// Indexed by warp minus one, without a terminator. XYZ uses world units and
/// yaw uses 4096 units per turn or the named transition-facing sentinels.
/// Initial logical views are 4,4,2,4; endpoint 4 starts the companion arrival
/// scene when a companion is present. The stage directory borrows this
/// overlay; transition handlers copy a complete endpoint before using it.
extern DirectionWarpEntry gDryfieldNightDrivewayWarpEntries[4];

/// Ten cameras indexed by the driveway's mapped view minus one.
///
/// Room 1 uses the identity map; room 2 selects these through its logical-view
/// map. Rotation coefficients use Q12, translation is the negated world
/// camera origin, and projection distance is in screen pixels. Keep this
/// overlay loaded while the stage camera directory borrows the array.
extern ViewCamera gDryfieldNightDrivewayViewCameras[10];

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
