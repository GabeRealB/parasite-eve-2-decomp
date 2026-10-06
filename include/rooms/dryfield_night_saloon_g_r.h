#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_SALOON_G_R_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_SALOON_G_R_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_saloon_g_r_80188EE4[13];

// dryfield_night_saloon_g_r
extern WorldCoordRoomLighting D_dryfield_night_saloon_g_r_80185180[];

extern WorldCollisionRoomResources D_dryfield_night_saloon_g_r_80185190[];

extern u8* D_dryfield_night_saloon_g_r_801851B0[];

extern ViewCount D_dryfield_night_saloon_g_r_801851B8[];

extern DirectionWarpEntry D_dryfield_night_saloon_g_r_801851BC[];

extern ViewCamera D_dryfield_night_saloon_g_r_80185B74[];

extern SpriteView D_dryfield_night_saloon_g_r_80187FC8[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_saloon_g_r_80188F84[];

/// Draws the night saloon's view-gated flares and light beams each frame.
///
/// Bank 6 type 0x10E requires a live single-coordinate task body. Composes its
/// transform for the local-space beams; the flare positions are world-space.
/// The loaded area's current logical view is 1..13. Requires the active view
/// matrices, scratch stack and frame packet arena; emitted packets live until
/// GPU completion. Does not advance task state or allocate persistent storage.
void dryfieldNightSaloonGRDrawGlowsTask(Task* task);

void func_dryfield_night_saloon_g_r_8017E050(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_SALOON_G_R_H
