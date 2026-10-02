#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_2_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_2_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_motel_room_2_80180410[12];

// dryfield_motel_room_2
extern u8* D_dryfield_motel_room_2_8017D6E4[];

extern GpRoomObjRec D_dryfield_motel_room_2_8017D6E8[];

extern GpViewCountRec D_dryfield_motel_room_2_8017D6F8[];

extern WorldCoordRoomLighting D_dryfield_motel_room_2_8017D6FC[];

extern GpWarpRec D_dryfield_motel_room_2_8017D704[];

extern ViewCamera D_dryfield_motel_room_2_8017DE30[];

extern SpriteView D_dryfield_motel_room_2_8017FCD0[];

extern WorldCollisionSurfaceProperties* D_dryfield_motel_room_2_801804B0[];

void func_dryfield_motel_room_2_8017D6B4(Task* unused);

void func_dryfield_motel_room_2_8017D65C(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_2_H
