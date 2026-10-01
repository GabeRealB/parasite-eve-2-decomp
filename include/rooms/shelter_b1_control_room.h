#ifndef INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_b1_control_room_80181BBC[2];

extern GpAreaApplyRec D_shelter_b1_control_room_80183BE0[2];

extern TaskDesc D_shelter_b1_control_room_80181B88;

extern GpAreaVariant D_shelter_b1_control_room_80183A98[22];

// shelter_b1_control_room
extern u8* D_shelter_b1_control_room_80181C70[];

extern GpViewCountRec D_shelter_b1_control_room_80181C74[];

extern GpWarpRec D_shelter_b1_control_room_80181C78[];

extern GpGridParams D_shelter_b1_control_room_801820F8;

extern GpViewRec D_shelter_b1_control_room_8018211C[];

extern GpSprtRec D_shelter_b1_control_room_801833BC[];

extern GpRoomCoordSet D_shelter_b1_control_room_801834DC;

extern GpObj4A D_shelter_b1_control_room_801834F4[];

extern GpObj4A D_shelter_b1_control_room_80183624[];

extern WorldCoordRoomAmbientEntry D_shelter_b1_control_room_80183B48[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_control_room_80183BC0[];

void func_shelter_b1_control_room_8017EECC(Task* task);

void func_shelter_b1_control_room_8017F150(Task* task);

void func_shelter_b1_control_room_8017FF80(Task* arg0);

void func_shelter_b1_control_room_801804D8(Task* task);

void func_shelter_b1_control_room_80181138(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_H
