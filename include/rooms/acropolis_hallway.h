#ifndef INCLUDE_ROOMS_ACROPOLIS_HALLWAY_H
#define INCLUDE_ROOMS_ACROPOLIS_HALLWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern AreaVariant D_acropolis_hallway_8017EA3C[13];

extern TmdSource gAcropolisHallwayModel01AE0;

// acropolis_hallway
extern WorldCollisionRoomResources D_acropolis_hallway_8017E258[];

extern u8* D_acropolis_hallway_8017E268[];

extern ViewCount D_acropolis_hallway_8017E26C[];

extern WorldCoordRoomLighting D_acropolis_hallway_8017E270[];

extern DirectionWarpEntry D_acropolis_hallway_8017E278[];

extern SpriteView D_acropolis_hallway_8017EC2C[];

extern ViewCamera D_acropolis_hallway_8017EC68[];

extern WorldCollisionSurfaceProperties* D_acropolis_hallway_8017ED40[];

/// Inert task callback for the hallway's effect-bank-6 slot 0x84.
///
/// Leaves the live task and its coordinate body untouched, including its state
/// and lifetime. Keep the hallway overlay loaded while this callback can run.
void acropolisHallwayEffectControlTask84(Task* unusedTask);

void func_acropolis_hallway_8017D7D0(Task* task);

void func_acropolis_hallway_8017E120(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_HALLWAY_H
