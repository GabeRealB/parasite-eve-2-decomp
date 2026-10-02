#ifndef INCLUDE_ROOMS_DRYFIELD_TOILET_H
#define INCLUDE_ROOMS_DRYFIELD_TOILET_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"
#include "overlay.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern OverlayMorphTarget D_dryfield_toilet_801865D0;

extern GpAreaVariant D_dryfield_toilet_80182918[13];

// dryfield_toilet
extern GpRoomObjRec D_dryfield_toilet_8018112C[];

extern u8* D_dryfield_toilet_8018113C[];

extern GpViewCountRec D_dryfield_toilet_80181140[];

extern WorldCoordRoomLighting D_dryfield_toilet_80181144[];

extern GpWarpRec D_dryfield_toilet_8018114C[];

extern ViewCamera D_dryfield_toilet_80181428[];

extern SpriteView D_dryfield_toilet_801821F8[];

extern WorldCollisionSurfaceProperties* D_dryfield_toilet_8018660C[];

void func_dryfield_toilet_8017DEF4(Task* arg0);

void func_dryfield_toilet_8017E64C(Task* arg0);

void func_dryfield_toilet_8017E69C(Task* arg0);

void func_dryfield_toilet_8017EBF4(Task* task);

void func_dryfield_toilet_8017F854(Task* arg0);

void func_dryfield_toilet_8017DCF0(Task* arg0);

void func_dryfield_toilet_8017D9E4(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_TOILET_H
