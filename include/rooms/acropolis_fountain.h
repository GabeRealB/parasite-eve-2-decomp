#ifndef INCLUDE_ROOMS_ACROPOLIS_FOUNTAIN_H
#define INCLUDE_ROOMS_ACROPOLIS_FOUNTAIN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_acropolis_fountain_8017FC9C[11];

// acropolis_fountain
extern GpRoomObjRec D_acropolis_fountain_8017E814[];

extern u8* D_acropolis_fountain_8017E84C[];

extern GpViewCountRec D_acropolis_fountain_8017E854[];

extern WorldCoordRoomLighting D_acropolis_fountain_8017E858[];

extern GpWarpRec D_acropolis_fountain_8017E868[];

extern SpriteView D_acropolis_fountain_8018375C[];

extern GpViewRec D_acropolis_fountain_80183864[];

extern WorldCollisionSurfaceProperties* D_acropolis_fountain_80183B90[];

void func_acropolis_fountain_8017DD44(Task* task);

void func_acropolis_fountain_8017DA78(s32 unused0, s32 unused1);

void func_acropolis_fountain_8017E014(Task* task);

/// Task entries the resident task descriptor tables name. A table in main or
/// gameplay reaches each of these by name, so they are the family's interface
/// to the resident code.
void func_acropolis_fountain_8017DCD4(Task* arg0);

void func_acropolis_fountain_8017D9C4(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_FOUNTAIN_H
