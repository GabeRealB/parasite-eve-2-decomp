#ifndef INCLUDE_ROOMS_SHELTER_B2_OPERATING_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B2_OPERATING_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_b2_operating_room_80184124[12];

// shelter_b2_operating_room
extern u8* D_shelter_b2_operating_room_80180BC8[];

extern GpViewCountRec D_shelter_b2_operating_room_80180BCC[];

extern GpWarpRec D_shelter_b2_operating_room_80180BD0[];

extern GpGridParams D_shelter_b2_operating_room_80181364;

extern GpViewRec D_shelter_b2_operating_room_80181388[];

extern GpSprtRec D_shelter_b2_operating_room_80183184[];

extern GpRoomCoordSet D_shelter_b2_operating_room_80183718;

extern GpObj4A D_shelter_b2_operating_room_80183730[];

extern GpObj3A D_shelter_b2_operating_room_80183A28[];

extern GpObj4A D_shelter_b2_operating_room_80183ADC[];

extern WorldCoordRoomAmbientEntry D_shelter_b2_operating_room_80184184[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_operating_room_801841F4[];

void func_shelter_b2_operating_room_8017DD60(Task* task);

void func_shelter_b2_operating_room_8017F254(Task* task);

void func_shelter_b2_operating_room_8017DDB8(Task* arg0);

void func_shelter_b2_operating_room_8017ECFC(Task* arg0);

void func_shelter_b2_operating_room_8017FEB4(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B2_OPERATING_ROOM_H
