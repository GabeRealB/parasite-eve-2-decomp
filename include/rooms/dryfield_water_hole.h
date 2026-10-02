#ifndef INCLUDE_ROOMS_DRYFIELD_WATER_HOLE_H
#define INCLUDE_ROOMS_DRYFIELD_WATER_HOLE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_water_hole_801827BC[13];

// dryfield_water_hole
extern WorldCollisionRoomResources D_dryfield_water_hole_8017FD2C[];

extern u8* D_dryfield_water_hole_8017FD84[];

extern ViewCount D_dryfield_water_hole_8017FD94[];

extern WorldCoordRoomLighting D_dryfield_water_hole_8017FD9C[];

extern GpWarpRec D_dryfield_water_hole_8017FDBC[];

extern ViewCamera D_dryfield_water_hole_80180284[];

extern SpriteView D_dryfield_water_hole_80181634[];

extern WorldCollisionSurfaceProperties* D_dryfield_water_hole_801828AC[];

void func_dryfield_water_hole_8017EC90(Task* task);

void func_dryfield_water_hole_8017F118(Task* task);

void func_dryfield_water_hole_8017E040(Task* arg0);

void func_dryfield_water_hole_8017D840(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_WATER_HOLE_H
