#ifndef INCLUDE_ROOMS_SHELTER_1F_AIRLOCK_H
#define INCLUDE_ROOMS_SHELTER_1F_AIRLOCK_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_1f_airlock_8017F758[12];

// shelter_1f_airlock
extern GpRoomObjRec D_shelter_1f_airlock_8017E59C[];

extern WorldCoordRoomLighting D_shelter_1f_airlock_8017E5AC[];

extern u8* D_shelter_1f_airlock_8017E5B4[];

extern GpViewCountRec D_shelter_1f_airlock_8017E5B8[];

extern GpWarpRec D_shelter_1f_airlock_8017E5BC[];

extern GpViewRec D_shelter_1f_airlock_8017E85C[];

extern SpriteView D_shelter_1f_airlock_8017F07C[];

extern WorldCollisionSurfaceProperties* D_shelter_1f_airlock_8017F84C[];

void func_shelter_1f_airlock_8017D678(Task* task);

void func_shelter_1f_airlock_8017D6D0(Task* unused);

#endif // INCLUDE_ROOMS_SHELTER_1F_AIRLOCK_H
