#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_HOLE_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_HOLE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaApplyRec D_dryfield_night_water_hole_80183618[4];

extern GpAreaVariant D_dryfield_night_water_hole_80183418[22];

// dryfield_night_water_hole
extern GpRoomObjRec D_dryfield_night_water_hole_80180A04[];

extern WorldCoordRoomLighting D_dryfield_night_water_hole_80180A44[];

extern u8* D_dryfield_night_water_hole_80180A94[];

extern ViewCount D_dryfield_night_water_hole_80180AA4[];

extern GpWarpRec D_dryfield_night_water_hole_80180AAC[];

extern ViewCamera D_dryfield_night_water_hole_80180F74[];

extern SpriteView D_dryfield_night_water_hole_80182384[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_water_hole_801835F8[];

void func_dryfield_night_water_hole_8017F6DC(Task* task);

void func_dryfield_night_water_hole_8017F254(Task* task);

void func_dryfield_night_water_hole_8017E6D0(Task* arg0);

void func_dryfield_night_water_hole_8017DE30(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_HOLE_H
