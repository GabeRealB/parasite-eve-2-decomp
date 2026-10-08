#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_TOILET_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_TOILET_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_toilet_8017F354[12];

// dryfield_night_toilet
extern WorldCoordRoomLighting D_dryfield_night_toilet_8017DAB0[];

extern WorldCollisionRoomResources D_dryfield_night_toilet_8017DAB8[];

extern u8* D_dryfield_night_toilet_8017DAC8[];

extern ViewCount D_dryfield_night_toilet_8017DACC[];

extern DirectionWarpEntry D_dryfield_night_toilet_8017DAD0[];

extern ViewCamera D_dryfield_night_toilet_8017DDAC[];

extern SpriteView D_dryfield_night_toilet_8017EC40[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_toilet_8017F3D8[];

/// Draws the night toilet's flickering light flare selected by the camera view.
///
/// Effect-bank 6 callback for slot 0x10C. View 4 selects the second world point;
/// views 5 and 9 select the first, and every other view draws nothing. The task
/// is unused. Texture column 1 uses a perspective radius scale of 512, giving
/// a pixel half-extent of 512 * 39 / (camera Z / 4).
///
/// Requires the room overlay and flare texture loaded, a composed view matrix,
/// scratch space and a current GPU packet arena and ordering table. A selected
/// flare reserves one packet even when rejected by near clipping; queued
/// geometry borrows the frame arena until GPU completion. Retains no task data.
void dryfieldNightToiletDrawFlareTask(Task* unusedTask);

/// Runs the night toilet's room initialization, message wait and teardown.
///
/// State 0 installs the room handlers and starts the first-visit event on
/// variant 1; state 1 waits for messages; state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldNightToiletRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_TOILET_H
