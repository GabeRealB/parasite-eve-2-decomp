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

// dryfield_night_driveway
extern WorldCoordRoomLighting D_dryfield_night_driveway_801805E0[];

extern WorldCollisionRoomResources D_dryfield_night_driveway_801805F0[];

extern u8* D_dryfield_night_driveway_8018061C[];

extern ViewCount D_dryfield_night_driveway_80180624[];

extern DirectionWarpEntry D_dryfield_night_driveway_80180628[];

extern ViewCamera D_dryfield_night_driveway_80180C30[];

extern SpriteView D_dryfield_night_driveway_80181870[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_driveway_801820F0[];

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
