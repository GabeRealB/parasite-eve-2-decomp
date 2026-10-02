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

extern GpAreaVariant D_acropolis_hallway_8017EA3C[13];

extern TmdSource gAcropolisHallwayModel01AE0;

// acropolis_hallway
extern GpRoomObjRec D_acropolis_hallway_8017E258[];

extern u8* D_acropolis_hallway_8017E268[];

extern GpViewCountRec D_acropolis_hallway_8017E26C[];

extern WorldCoordRoomLighting D_acropolis_hallway_8017E270[];

extern GpWarpRec D_acropolis_hallway_8017E278[];

extern SpriteView D_acropolis_hallway_8017EC2C[];

extern GpViewRec D_acropolis_hallway_8017EC68[];

extern WorldCollisionSurfaceProperties* D_acropolis_hallway_8017ED40[];

void func_acropolis_hallway_8017D828(Task* unused);

void func_acropolis_hallway_8017D7D0(Task* task);

void func_acropolis_hallway_8017E120(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_HALLWAY_H
