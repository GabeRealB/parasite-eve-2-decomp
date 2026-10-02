#ifndef INCLUDE_ROOMS_DRYFIELD_SALOON_G_R_H
#define INCLUDE_ROOMS_DRYFIELD_SALOON_G_R_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_saloon_g_r_80181B1C[13];

// dryfield_saloon_g_r
extern WorldCollisionRoomResources D_dryfield_saloon_g_r_8017EDA0[];

extern u8* D_dryfield_saloon_g_r_8017EDD0[];

extern ViewCount D_dryfield_saloon_g_r_8017EDD8[];

extern WorldCoordRoomLighting D_dryfield_saloon_g_r_8017EDDC[];

extern DirectionWarpEntry D_dryfield_saloon_g_r_8017EDEC[];

extern ViewCamera D_dryfield_saloon_g_r_8017F7A4[];

extern SpriteView D_dryfield_saloon_g_r_80180E2C[];

extern WorldCollisionSurfaceProperties* D_dryfield_saloon_g_r_80181BBC[];

void func_dryfield_saloon_g_r_8017DA70(Task* arg0);

void func_dryfield_saloon_g_r_8017DA18(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_SALOON_G_R_H
