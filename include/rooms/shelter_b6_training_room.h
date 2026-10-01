#ifndef INCLUDE_ROOMS_SHELTER_B6_TRAINING_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B6_TRAINING_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/task_types.h"

extern GpAreaVariant D_shelter_b6_training_room_801859DC[13];

// shelter_b6_training_room
extern u8* D_shelter_b6_training_room_80184418[];

extern GpViewCountRec D_shelter_b6_training_room_8018441C[];

extern GpWarpRec D_shelter_b6_training_room_80184420[];

extern GpGridParams D_shelter_b6_training_room_80184734;

extern GpViewRec D_shelter_b6_training_room_80184758[];

extern GpSprtRec D_shelter_b6_training_room_80184D78[];

extern GpRoomCoordSet D_shelter_b6_training_room_80185768;

extern GpObj4A D_shelter_b6_training_room_80185780[];

extern GpObj4A D_shelter_b6_training_room_80185A44[];

extern WorldCoordRoomAmbientEntry D_shelter_b6_training_room_80185BC0[];

extern WorldCollisionSurfaceProperties* D_shelter_b6_training_room_80185C38[];

void func_shelter_b6_training_room_8017D8E8(Task* task);

void func_shelter_b6_training_room_80181930(Task* task);

void func_shelter_b6_training_room_8018294C(Task* task);

void func_shelter_b6_training_room_80182A14(s32 arg0, s32 arg1);

void func_shelter_b6_training_room_8017FC40(GfxCoord* coord, s16 size, u16 color);

void func_shelter_b6_training_room_8017EE70(Task* arg0);

void func_shelter_b6_training_room_8017F8B8(Task* task);

void func_shelter_b6_training_room_80180DB4(Task* task);

void func_shelter_b6_training_room_801811AC(Task* task);

void func_shelter_b6_training_room_80181A3C(Task* task);

void func_shelter_b6_training_room_8018245C(Task* task);

void func_shelter_b6_training_room_801825C0(Task* task);

void func_shelter_b6_training_room_801826E0(Task* task);

void func_shelter_b6_training_room_80182804(Task* task);

void func_shelter_b6_training_room_8017DDE8(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B6_TRAINING_ROOM_H
