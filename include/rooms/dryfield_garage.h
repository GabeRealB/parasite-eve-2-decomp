#ifndef INCLUDE_ROOMS_DRYFIELD_GARAGE_H
#define INCLUDE_ROOMS_DRYFIELD_GARAGE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaApplyRec D_dryfield_garage_80180204[6];

extern SVECTOR gDryfieldGarageCollision0108CNormals[39];

extern SVECTOR gDryfieldGarageCollision0108CVerts[106];

extern WorldCollisionGridFace gDryfieldGarageCollision0108CFaces[54];

extern GpAreaVariant D_dryfield_garage_801800E0[13];

// dryfield_garage
extern GpRoomObjRec D_dryfield_garage_8017DCDC[];

extern u8* D_dryfield_garage_8017DCEC[];

extern GpViewCountRec D_dryfield_garage_8017DCF0[];

extern WorldCoordRoomLighting D_dryfield_garage_8017DCF4[];

extern GpWarpRec D_dryfield_garage_8017DCFC[];

extern GpViewRec D_dryfield_garage_8017E670[];

extern SpriteView D_dryfield_garage_8017F5E8[];

extern WorldCollisionSurfaceProperties* D_dryfield_garage_801801E4[];

void func_dryfield_garage_8017DC68(Task* unused);

void func_dryfield_garage_8017DC10(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_GARAGE_H
