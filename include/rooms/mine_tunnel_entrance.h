#ifndef INCLUDE_ROOMS_MINE_TUNNEL_ENTRANCE_H
#define INCLUDE_ROOMS_MINE_TUNNEL_ENTRANCE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_mine_tunnel_entrance_8017F32C[12];

// mine_tunnel_entrance
extern GpRoomCoordRec D_mine_tunnel_entrance_8017DB58[];

extern GpRoomObjRec D_mine_tunnel_entrance_8017DB60[];

extern u8* D_mine_tunnel_entrance_8017DB70[];

extern GpViewCountRec D_mine_tunnel_entrance_8017DB74[];

extern GpWarpRec D_mine_tunnel_entrance_8017DB78[];

extern GpViewRec D_mine_tunnel_entrance_8017E0E4[];

extern GpSprtRec D_mine_tunnel_entrance_8017EA4C[];

extern WorldCollisionSurfaceProperties* D_mine_tunnel_entrance_8017F3E8[];

void func_mine_tunnel_entrance_8017D720(Task* unused);

void func_mine_tunnel_entrance_8017D6BC(Task* task);

#endif // INCLUDE_ROOMS_MINE_TUNNEL_ENTRANCE_H
