#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_LOBBY_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_LOBBY_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// dryfield_motel_lobby
extern GpRoomObjRec D_dryfield_motel_lobby_8017F838[];

extern u8* D_dryfield_motel_lobby_8017F848[];

extern GpViewCountRec D_dryfield_motel_lobby_8017F84C[];

extern GpRoomCoordRec D_dryfield_motel_lobby_8017F850[];

extern GpWarpRec D_dryfield_motel_lobby_8017F858[];

extern GpViewRec D_dryfield_motel_lobby_8017FC08[];

extern GpSprtRec D_dryfield_motel_lobby_80180AB0[];

extern WorldCollisionSurfaceProperties* D_dryfield_motel_lobby_80181044[];

void func_dryfield_motel_lobby_8017E9E8(Task* task);

void func_dryfield_motel_lobby_8017F498(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_LOBBY_H
