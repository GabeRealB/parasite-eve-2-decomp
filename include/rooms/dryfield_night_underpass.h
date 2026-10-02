#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_UNDERPASS_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_UNDERPASS_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_night_underpass_801802DC[12];

// dryfield_night_underpass
extern GpRoomObjRec D_dryfield_night_underpass_8017DD70[];

extern WorldCoordRoomLighting D_dryfield_night_underpass_8017DDD0[];

extern u8* D_dryfield_night_underpass_8017DE3C[];

extern ViewCount D_dryfield_night_underpass_8017DE54[];

extern GpWarpRec D_dryfield_night_underpass_8017DE60[];

extern ViewCamera D_dryfield_night_underpass_8017E6F8[];

extern SpriteView D_dryfield_night_underpass_8017F420[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_underpass_80180374[];

void func_dryfield_night_underpass_8017DC3C(Task* unused);

void func_dryfield_night_underpass_8017D95C(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_UNDERPASS_H
