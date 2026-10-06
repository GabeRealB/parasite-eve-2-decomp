#ifndef INCLUDE_ROOMS_DRYFIELD_G_R_KITCHEN_H
#define INCLUDE_ROOMS_DRYFIELD_G_R_KITCHEN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_g_r_kitchen_8017F4B8[13];

// dryfield_g_r_kitchen
extern WorldCollisionRoomResources D_dryfield_g_r_kitchen_8017EC28[];

extern u8* D_dryfield_g_r_kitchen_8017EC38[];

extern ViewCount D_dryfield_g_r_kitchen_8017EC3C[];

extern WorldCoordRoomLighting D_dryfield_g_r_kitchen_8017EC40[];

extern DirectionWarpEntry D_dryfield_g_r_kitchen_8017EC48[];

extern ViewCamera D_dryfield_g_r_kitchen_8017EEE4[];

extern SpriteView D_dryfield_g_r_kitchen_8017F014[];

extern WorldCollisionSurfaceProperties* D_dryfield_g_r_kitchen_8017F53C[];

/// Draws the kitchen's two additive light beams for the current view.
///
/// View 2 uses brighter grey beams; view 3 uses dimmer grey beams. Other
/// views queue nothing. `task` must have a live `TASK_BODY_COORD` body whose
/// coordinate cache maps the room's local endpoints into `GsWSMATRIX`'s input
/// space, including the view parent in the composed transform.
/// Borrows that coordinate for the call without changing task state.
/// Requires composed view matrices, an initialized scratch stack with a free
/// 40-byte block, and a current ordering table and frame arena with room for
/// twelve `POLY_G4` and twelve `DR_TPAGE` packets. Queued packets must remain
/// live until GPU completion.
void dryfieldGRKitchenDrawLightBeamsTask(Task* task);

void func_dryfield_g_r_kitchen_8017D9A4(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_G_R_KITCHEN_H
