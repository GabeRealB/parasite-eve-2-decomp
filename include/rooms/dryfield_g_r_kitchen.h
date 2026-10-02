#ifndef INCLUDE_ROOMS_DRYFIELD_G_R_KITCHEN_H
#define INCLUDE_ROOMS_DRYFIELD_G_R_KITCHEN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_g_r_kitchen_8017F4B8[13];

// dryfield_g_r_kitchen
extern GpRoomObjRec D_dryfield_g_r_kitchen_8017EC28[];

extern u8* D_dryfield_g_r_kitchen_8017EC38[];

extern ViewCount D_dryfield_g_r_kitchen_8017EC3C[];

extern WorldCoordRoomLighting D_dryfield_g_r_kitchen_8017EC40[];

extern GpWarpRec D_dryfield_g_r_kitchen_8017EC48[];

extern ViewCamera D_dryfield_g_r_kitchen_8017EEE4[];

extern SpriteView D_dryfield_g_r_kitchen_8017F014[];

extern WorldCollisionSurfaceProperties* D_dryfield_g_r_kitchen_8017F53C[];

void func_dryfield_g_r_kitchen_8017EB04(Task* arg0);

void func_dryfield_g_r_kitchen_8017D9A4(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_G_R_KITCHEN_H
