#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_CELLAR_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_CELLAR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_cellar_80180780[12];

// dryfield_night_cellar
extern WorldCoordRoomLighting D_dryfield_night_cellar_8017DAF0[];

extern WorldCollisionRoomResources D_dryfield_night_cellar_8017DB00[];

extern u8* D_dryfield_night_cellar_8017DB28[];

extern ViewCount D_dryfield_night_cellar_8017DB30[];

extern DirectionWarpEntry D_dryfield_night_cellar_8017DB34[];

extern ViewCamera D_dryfield_night_cellar_8017DE84[];

extern SpriteView D_dryfield_night_cellar_8017FAB8[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_cellar_801807F4[];

/// Draws the nighttime cellar's switch-gated pair of flickering flares each frame.
///
/// `GAME_FLAG_UNDERPASS_SWITCH_2` must equal 1; camera views 2 and 3 select
/// separate pairs of world points, and other views draw nothing. The callback
/// argument is unused. The current view transform, scratch stack, ordering
/// table and packet arena must be ready for drawing, with nonzero depth for
/// accepted points. Borrows the room's points for this call; queued flare
/// packets belong to the current frame and remain live until GPU completion.
void dryfieldNightCellarDrawGlowsTask(Task* unused);

void func_dryfield_night_cellar_8017D748(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_CELLAR_H
