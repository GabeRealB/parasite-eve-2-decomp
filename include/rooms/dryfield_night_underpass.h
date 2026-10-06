#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_UNDERPASS_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_UNDERPASS_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_underpass_801802DC[12];

// dryfield_night_underpass
extern WorldCollisionRoomResources D_dryfield_night_underpass_8017DD70[];

extern WorldCoordRoomLighting D_dryfield_night_underpass_8017DDD0[];

extern u8* D_dryfield_night_underpass_8017DE3C[];

extern ViewCount D_dryfield_night_underpass_8017DE54[];

extern DirectionWarpEntry D_dryfield_night_underpass_8017DE60[];

extern ViewCamera D_dryfield_night_underpass_8017E6F8[];

extern SpriteView D_dryfield_night_underpass_8017F420[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_underpass_80180374[];

/// Draws the nighttime underpass's world-space flares for the current camera view.
///
/// Called each frame while this room overlay is loaded. `unusedTask` supplies no
/// position or state. Nonzero `GAME_FLAG_053` suppresses drawing; otherwise each
/// of eight fixed points is selected by bit `location.loc.view` of its view mask.
/// The room's view IDs are 1..26, and only views 2..11 select any points.
/// Each selected point uses flare texture column zero and a pixel half-extent of
/// `640 * 39 / depth`, where depth is camera Z / 4 and must be nonzero for an
/// accepted projection. Requires the current view matrices, scratch stack and
/// frame's GPU packet arena and ordering table; queued packets live for the frame.
void dryfieldNightUnderpassDrawFlaresTask(Task* unusedTask);

void func_dryfield_night_underpass_8017D95C(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_UNDERPASS_H
