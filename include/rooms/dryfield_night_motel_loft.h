#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern GpAreaVariant D_dryfield_night_motel_loft_80180888[13];

extern TmdSource gDryfieldNightMotelLoftActor135400Model071AC;

// dryfield_night_motel_loft
extern WorldCoordRoomLighting D_dryfield_night_motel_loft_8017EDB0[];

extern WorldCollisionRoomResources D_dryfield_night_motel_loft_8017EDC0[];

extern u8* D_dryfield_night_motel_loft_8017EDF0[];

extern ViewCount D_dryfield_night_motel_loft_8017EDF8[];

extern GpWarpRec D_dryfield_night_motel_loft_8017EDFC[];

extern ViewCamera D_dryfield_night_motel_loft_8017F144[];

extern SpriteView D_dryfield_night_motel_loft_8017FBE4[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_motel_loft_8018090C[];

void func_dryfield_night_motel_loft_8017E090(Task* task);

void func_dryfield_night_motel_loft_8017DB64(Task* arg0);

void func_dryfield_night_motel_loft_8017D964(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_H
