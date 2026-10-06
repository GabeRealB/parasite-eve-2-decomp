#ifndef INCLUDE_ROOMS_DRYFIELD_JUNK_YARD_H
#define INCLUDE_ROOMS_DRYFIELD_JUNK_YARD_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern AreaVariant D_dryfield_junk_yard_8017F558[13];

/// Models the Dryfield map UI overlay's enemy descriptors attach.
extern TmdSource gDryfieldJunkYardModel01378;

// dryfield_junk_yard
extern WorldCollisionRoomResources D_dryfield_junk_yard_8017ED04[];

extern WorldCoordRoomLighting D_dryfield_junk_yard_8017ED14[];

extern u8* D_dryfield_junk_yard_8017ED1C[];

extern ViewCount D_dryfield_junk_yard_8017ED20[];

extern DirectionWarpEntry D_dryfield_junk_yard_8017ED24[];

extern ViewCamera D_dryfield_junk_yard_8017F5C0[];

extern SpriteView D_dryfield_junk_yard_80180C28[];

extern WorldCollisionSurfaceProperties* D_dryfield_junk_yard_80181C28[];

/// Enables the junk yard's view effects, including dust from actor footsteps.
///
/// Gameplay dispatches this room callback as effect task 0xD8. The room-effect
/// controller must be live; the task argument is ignored and no state advances.
void dryfieldJunkYardEnableViewEffectsTask(Task* unusedTask);

void func_dryfield_junk_yard_8017D5F4(Task* task);

void func_dryfield_junk_yard_8017DCB4(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_JUNK_YARD_H
