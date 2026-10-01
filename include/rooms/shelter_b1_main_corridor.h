#ifndef INCLUDE_ROOMS_SHELTER_B1_MAIN_CORRIDOR_H
#define INCLUDE_ROOMS_SHELTER_B1_MAIN_CORRIDOR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_b1_main_corridor_80185C30[22];

// shelter_b1_main_corridor
extern u8* D_shelter_b1_main_corridor_801831F8[];

extern GpViewCountRec D_shelter_b1_main_corridor_801831FC[];

extern GpWarpRec D_shelter_b1_main_corridor_80183200[];

extern WorldCollisionGrid D_shelter_b1_main_corridor_801840F0;

extern GpViewRec D_shelter_b1_main_corridor_80184114[];

extern GpSprtRec D_shelter_b1_main_corridor_80185128[];

extern GpRoomCoordSet D_shelter_b1_main_corridor_801853E0;

extern GpObj4A D_shelter_b1_main_corridor_801853F8[];

extern GpObj4A D_shelter_b1_main_corridor_801858B8[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_main_corridor_80185D04[];

void func_shelter_b1_main_corridor_8017DD98(Task* task);

void func_shelter_b1_main_corridor_8017DDF0(Task* arg0);

void func_shelter_b1_main_corridor_801810F8(Task* arg0);

void func_shelter_b1_main_corridor_80181B5C(Task* task);

void func_shelter_b1_main_corridor_80182444(Task* task);

void func_shelter_b1_main_corridor_8017EAD4(Task* task);

void func_shelter_b1_main_corridor_8017F81C(Task* arg0);

void func_shelter_b1_main_corridor_8017FBB4(Task* arg0);

void func_shelter_b1_main_corridor_80180FC4(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B1_MAIN_CORRIDOR_H
