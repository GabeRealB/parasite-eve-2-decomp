#ifndef INCLUDE_ROOMS_NEO_ARK_SOUTH_PROMENADE_H
#define INCLUDE_ROOMS_NEO_ARK_SOUTH_PROMENADE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_neo_ark_south_promenade_801808E4[13];

// neo_ark_south_promenade
extern GpRoomCoordRec D_neo_ark_south_promenade_8017F6EC[];

extern GpRoomObjRec D_neo_ark_south_promenade_8017F6F4[];

extern u8* D_neo_ark_south_promenade_8017F704[];

extern GpViewCountRec D_neo_ark_south_promenade_8017F708[];

extern GpWarpRec D_neo_ark_south_promenade_8017F70C[];

extern GpViewRec D_neo_ark_south_promenade_8017FDB0[];

extern GpSprtRec D_neo_ark_south_promenade_801803E4[];

extern WorldCollisionSurfaceProperties* D_neo_ark_south_promenade_801809AC[];

void func_neo_ark_south_promenade_8017E184(Task* task);

void func_neo_ark_south_promenade_8017EA6C(Task* task);

void func_neo_ark_south_promenade_8017D6D0(Task* arg0);

void func_neo_ark_south_promenade_8017D720(Task* task);

void func_neo_ark_south_promenade_8017D678(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SOUTH_PROMENADE_H
