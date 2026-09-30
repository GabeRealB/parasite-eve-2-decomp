#ifndef INCLUDE_ROOMS_SHELTER_B6_GROWTH_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B6_GROWTH_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_b6_growth_room_80180338[13];

// shelter_b6_growth_room
extern u8* D_shelter_b6_growth_room_8017F378[];

extern GpViewCountRec D_shelter_b6_growth_room_8017F37C[];

extern GpWarpRec D_shelter_b6_growth_room_8017F380[];

extern GpGridParams D_shelter_b6_growth_room_8017FAF0;

extern GpViewRec D_shelter_b6_growth_room_8017FB14[];

extern GpSprtRec D_shelter_b6_growth_room_8017FEB8[];

extern GpRoomCoordSet D_shelter_b6_growth_room_8017FF78;

extern GpObj4A D_shelter_b6_growth_room_8017FF90[];

extern GpObj4A D_shelter_b6_growth_room_801803A0[];

extern WorldCoordRoomAmbientEntry D_shelter_b6_growth_room_80180730[];

extern GpRoomParamRec* D_shelter_b6_growth_room_801807A8[];

void func_shelter_b6_growth_room_8017D7D4(Task* task);

// Called by the actor overlay's event scripts while this room is loaded.
void func_shelter_b6_growth_room_8017D82C(s32 arg0);

void func_shelter_b6_growth_room_8017D9D8(Task* task);

void func_shelter_b6_growth_room_8017E564(Task* task);

void func_shelter_b6_growth_room_8017EAC8(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B6_GROWTH_ROOM_H
