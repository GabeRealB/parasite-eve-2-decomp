#ifndef INCLUDE_ROOMS_NEO_ARK_SUBSTATION_H
#define INCLUDE_ROOMS_NEO_ARK_SUBSTATION_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// neo_ark_substation
extern WorldCollisionRoomResources D_neo_ark_substation_8017E3F0[];

extern WorldCoordRoomLighting D_neo_ark_substation_8017E400[];

extern u8* D_neo_ark_substation_8017E408[];

extern ViewCount D_neo_ark_substation_8017E40C[];

extern DirectionWarpEntry D_neo_ark_substation_8017E410[];

extern ViewCamera D_neo_ark_substation_8017E8C8[];

extern SpriteView D_neo_ark_substation_8017F584[];

extern WorldCollisionSurfaceProperties* D_neo_ark_substation_80180328[];

/// Draws the substation's flickering light glows for the current mapped view.
///
/// Bank-6 effect 0x38 draws fixed world-space endpoint pairs in views 2..8;
/// other views emit nothing. The task argument is unused and no state is retained.
/// Requires the substation overlay, composed view matrices, initialized scratch
/// stack and enough space in the current frame's packet arena and ordering table.
void neoArkSubstationDrawLightGlowsTask(Task* unusedTask);

void func_neo_ark_substation_8017D81C(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SUBSTATION_H
