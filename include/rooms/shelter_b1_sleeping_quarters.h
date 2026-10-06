#ifndef INCLUDE_ROOMS_SHELTER_B1_SLEEPING_QUARTERS_H
#define INCLUDE_ROOMS_SHELTER_B1_SLEEPING_QUARTERS_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gShelterB1SleepingQuartersModel02DFC;

extern AreaVariant D_shelter_b1_sleeping_quarters_80183FDC[22];

// shelter_b1_sleeping_quarters
extern u8* D_shelter_b1_sleeping_quarters_80180658[];

extern ViewCount D_shelter_b1_sleeping_quarters_8018065C[];

extern DirectionWarpEntry D_shelter_b1_sleeping_quarters_80180660[];

extern WorldCollisionGrid D_shelter_b1_sleeping_quarters_801810D4;

extern ViewCamera D_shelter_b1_sleeping_quarters_801810F8[];

extern SpriteView D_shelter_b1_sleeping_quarters_80182E70[];

extern WorldCoordRoomLights D_shelter_b1_sleeping_quarters_80183234;

extern WorldCollisionTrigger D_shelter_b1_sleeping_quarters_8018324C[];

extern WorldCollisionOccluder D_shelter_b1_sleeping_quarters_801837A4[];

extern WorldCollisionTrigger D_shelter_b1_sleeping_quarters_801838D0[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_sleeping_quarters_801840B0[];

void func_shelter_b1_sleeping_quarters_8017D608(Task* task);

void func_shelter_b1_sleeping_quarters_8017D888(Task* task);

/// Draws the Sleeping Quarters' view-specific light beams and red glow disc.
///
/// Bank-6 effect task 0x12D. On state 0, selects this room's glow-disc,
/// flying-spark and orange-burst effects and advances to state 1; drawing
/// starts in the same tick and continues in every nonzero state. Uses the
/// current mapped camera index; only views 2..10 emit lights.
/// Requires a live task, the room overlay loaded, composed view matrices,
/// and the current frame's scratch stack, primitive arena and ordering table.
/// Queued primitives remain in the frame arena until GPU completion.
void shelterB1SleepingQuartersDrawViewLightsTask(Task* task);

void func_shelter_b1_sleeping_quarters_8017E6DC(Task* arg0);

void func_shelter_b1_sleeping_quarters_8017F894(Task* arg0);

void func_shelter_b1_sleeping_quarters_8017EC34(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_SLEEPING_QUARTERS_H
