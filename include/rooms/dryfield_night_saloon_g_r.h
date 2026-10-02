#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_SALOON_G_R_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_SALOON_G_R_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_night_saloon_g_r_80188EE4[13];

// dryfield_night_saloon_g_r
extern WorldCoordRoomLighting D_dryfield_night_saloon_g_r_80185180[];

extern WorldCollisionRoomResources D_dryfield_night_saloon_g_r_80185190[];

extern u8* D_dryfield_night_saloon_g_r_801851B0[];

extern ViewCount D_dryfield_night_saloon_g_r_801851B8[];

extern GpWarpRec D_dryfield_night_saloon_g_r_801851BC[];

extern ViewCamera D_dryfield_night_saloon_g_r_80185B74[];

extern SpriteView D_dryfield_night_saloon_g_r_80187FC8[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_saloon_g_r_80188F84[];

void func_dryfield_night_saloon_g_r_8017E6C8(Task* arg0);

void func_dryfield_night_saloon_g_r_8017E050(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_SALOON_G_R_H
