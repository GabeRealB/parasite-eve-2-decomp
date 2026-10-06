#ifndef INCLUDE_ROOMS_SHELTER_R47_H
#define INCLUDE_ROOMS_SHELTER_R47_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_r47_80187618;

extern WorldCollisionGrid D_shelter_r47_8018828C;

extern AreaApplyRec D_shelter_r47_8018A638[21];

extern AreaVariant D_shelter_r47_80187CB8[12];

// shelter_r47
extern u8* D_shelter_r47_80187674[];

extern ViewCount D_shelter_r47_80187678[];

extern DirectionWarpEntry D_shelter_r47_8018767C[];

extern WorldCollisionTrigger D_shelter_r47_801876B4[];

extern ViewCamera D_shelter_r47_801882B0[];

extern SpriteView D_shelter_r47_80189C68[];

extern WorldCoordRoomLights D_shelter_r47_8018A5BC;

extern WorldCollisionSurfaceProperties* D_shelter_r47_8018A618[];

extern WorldCollisionTrigger D_shelter_r47_8018787C[13];

void func_shelter_r47_801807B4(Task* task);

void func_shelter_r47_8017EC04(Task* task);

/// Draws the room's additive light glows for the current mapped camera view.
///
/// Effect-bank callback 0x14B. Mapped views 5, 13, 14 and 44 select fixed
/// world-point subsets; other views emit no packets. The cyan glow at the
/// first point is a pulsing diamond except in view 44, where it is a layered
/// pulsing disc. The other points use flickering grey or cool-grey discs.
/// `unusedTask` is ignored; no task state, body or work is read or changed.
/// Requires this room overlay, its active view transform, initialized scratch
/// storage and the current frame's ordering table and packet arena with enough
/// space. Accepted points must project to nonzero depth. Queued packets remain
/// in that arena until the frame's GPU work finishes.
void shelterR47DrawViewGlowsTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_R47_H
