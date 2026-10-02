#ifndef INCLUDE_ROOMS_NEO_ARK_SUBSTATION_H
#define INCLUDE_ROOMS_NEO_ARK_SUBSTATION_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// neo_ark_substation
extern GpRoomObjRec D_neo_ark_substation_8017E3F0[];

extern WorldCoordRoomLighting D_neo_ark_substation_8017E400[];

extern u8* D_neo_ark_substation_8017E408[];

extern GpViewCountRec D_neo_ark_substation_8017E40C[];

extern GpWarpRec D_neo_ark_substation_8017E410[];

extern ViewCamera D_neo_ark_substation_8017E8C8[];

extern SpriteView D_neo_ark_substation_8017F584[];

extern WorldCollisionSurfaceProperties* D_neo_ark_substation_80180328[];

void func_neo_ark_substation_8017D874(Task* unused);

void func_neo_ark_substation_8017D81C(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SUBSTATION_H
