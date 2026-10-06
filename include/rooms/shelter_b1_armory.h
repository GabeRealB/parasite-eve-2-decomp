#ifndef INCLUDE_ROOMS_SHELTER_B1_ARMORY_H
#define INCLUDE_ROOMS_SHELTER_B1_ARMORY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_b1_armory_801824D0;

extern AreaVariant D_shelter_b1_armory_801854E0[11];

// shelter_b1_armory
extern u8* D_shelter_b1_armory_80182580[];

extern ViewCount D_shelter_b1_armory_80182584[];

extern DirectionWarpEntry D_shelter_b1_armory_80182588[];

extern WorldCollisionGrid D_shelter_b1_armory_80182ED0;

extern ViewCamera D_shelter_b1_armory_80182EF4[];

extern SpriteView D_shelter_b1_armory_80184220[];

extern WorldCoordRoomLights D_shelter_b1_armory_80184AFC;

extern WorldCollisionTrigger D_shelter_b1_armory_80184B14[];

extern WorldCollisionTrigger D_shelter_b1_armory_80184E0C[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_armory_80185554[];

void func_shelter_b1_armory_8018078C(Task* task);

/// Draws the armory's view-dependent beams and glows for the current frame.
///
/// The lock indicator is green when the armory is unlocked and red otherwise.
/// The task argument is unused; no task state or spawn arguments are read.
/// Requires this room overlay to be loaded, a valid current view mapping and
/// composed view matrices, plus an initialized scratch stack and enough space
/// in the current frame's GPU packet arena and depth ordering table.
void shelterB1ArmoryDrawGlowsTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_B1_ARMORY_H
