#ifndef INCLUDE_ROOMS_DRYFIELD_BACK_STREET_H
#define INCLUDE_ROOMS_DRYFIELD_BACK_STREET_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_back_street_80180A50[13];

// dryfield_back_street
extern GpRoomObjRec D_dryfield_back_street_8017F9B4[];

extern u8* D_dryfield_back_street_8017F9C4[];

extern GpViewCountRec D_dryfield_back_street_8017F9C8[];

extern WorldCoordRoomLighting D_dryfield_back_street_8017F9CC[];

extern GpWarpRec D_dryfield_back_street_8017F9D4[];

extern ViewCamera D_dryfield_back_street_801802A8[];

extern SpriteView D_dryfield_back_street_80180484[];

extern WorldCollisionSurfaceProperties* D_dryfield_back_street_80181034[];

void func_dryfield_back_street_8017D9D0(Task* task);

void func_dryfield_back_street_8017E434(Task* task);

void func_dryfield_back_street_8017ED1C(Task* task);

void func_dryfield_back_street_8017D970(Task* task);

void func_dryfield_back_street_8017D918(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_BACK_STREET_H
