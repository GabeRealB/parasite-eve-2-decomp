#ifndef INCLUDE_ROOMS_MINE_TUNNEL_H
#define INCLUDE_ROOMS_MINE_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_mine_tunnel_801801CC[12];

// mine_tunnel
extern WorldCoordRoomLighting D_mine_tunnel_8017E154[];

extern GpRoomObjRec D_mine_tunnel_8017E15C[];

extern u8* D_mine_tunnel_8017E16C[];

extern GpViewCountRec D_mine_tunnel_8017E170[];

extern GpWarpRec D_mine_tunnel_8017E174[];

extern GpViewRec D_mine_tunnel_8017E890[];

extern SpriteView D_mine_tunnel_8017F9A4[];

extern WorldCollisionSurfaceProperties* D_mine_tunnel_8018032C[];

void func_mine_tunnel_8017D7D4(Task* unused);

void func_mine_tunnel_8017D77C(Task* task);

#endif // INCLUDE_ROOMS_MINE_TUNNEL_H
