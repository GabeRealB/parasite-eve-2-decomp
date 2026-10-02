#ifndef INCLUDE_ROOMS_ACROPOLIS_SANCTUARY_H
#define INCLUDE_ROOMS_ACROPOLIS_SANCTUARY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern GpAreaVariant D_acropolis_sanctuary_8018402C[12];

extern TmdSource gAcropolisSanctuaryModel090F0;

extern TmdSource gAcropolisSanctuaryModel09584;

// acropolis_sanctuary
extern GpRoomObjRec D_acropolis_sanctuary_801827EC[];

extern u8* D_acropolis_sanctuary_801827FC[];

extern ViewCount D_acropolis_sanctuary_80182800[];

extern WorldCoordRoomLighting D_acropolis_sanctuary_80182804[];

extern GpWarpRec D_acropolis_sanctuary_8018280C[];

extern SpriteView D_acropolis_sanctuary_801860C8[];

extern ViewCamera D_acropolis_sanctuary_80186188[];

extern WorldCollisionSurfaceProperties* D_acropolis_sanctuary_801863F8[];

void func_acropolis_sanctuary_8017E00C(Task* task);

void func_acropolis_sanctuary_8017E134(Task* arg0);

void func_acropolis_sanctuary_8017E338(Task* arg0);

void func_acropolis_sanctuary_8017EC90(Task* arg0);

void func_acropolis_sanctuary_8017F4E8(Task* arg0);

void func_acropolis_sanctuary_8017D9E8(Task* task);

void func_acropolis_sanctuary_80180264(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_SANCTUARY_H
