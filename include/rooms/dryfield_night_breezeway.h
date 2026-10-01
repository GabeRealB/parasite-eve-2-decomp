#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_BREEZEWAY_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_BREEZEWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_night_breezeway_801803E4[22];

// dryfield_night_breezeway
extern WorldCoordRoomLighting D_dryfield_night_breezeway_8017E6E4[];

extern GpRoomObjRec D_dryfield_night_breezeway_8017E6EC[];

extern u8* D_dryfield_night_breezeway_8017E6FC[];

extern GpViewCountRec D_dryfield_night_breezeway_8017E700[];

extern GpWarpRec D_dryfield_night_breezeway_8017E704[];

extern GpViewRec D_dryfield_night_breezeway_8017EBE8[];

extern SpriteView D_dryfield_night_breezeway_8017FD10[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_breezeway_801804B8[];

void func_dryfield_night_breezeway_8017E5BC(Task* unused);

void func_dryfield_night_breezeway_8017D680(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_BREEZEWAY_H
