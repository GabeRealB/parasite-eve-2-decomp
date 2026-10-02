#ifndef INCLUDE_ROOMS_MINE_FORKED_TUNNEL_H
#define INCLUDE_ROOMS_MINE_FORKED_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern GpAreaVariant D_mine_forked_tunnel_80185504[12];

extern TmdSource gMineForkedTunnelModel01B48;

// mine_forked_tunnel
extern WorldCoordRoomLighting D_mine_forked_tunnel_80183634[];

extern GpRoomObjRec D_mine_forked_tunnel_8018363C[];

extern u8* D_mine_forked_tunnel_8018364C[];

extern GpViewCountRec D_mine_forked_tunnel_80183650[];

extern GpWarpRec D_mine_forked_tunnel_80183654[];

extern GpViewRec D_mine_forked_tunnel_80183D94[];

extern SpriteView D_mine_forked_tunnel_80184D64[];

extern WorldCollisionSurfaceProperties* D_mine_forked_tunnel_801855C0[];

void func_mine_forked_tunnel_8017E78C(Task* unused);

void func_mine_forked_tunnel_8017DBE4(Task* task);

void func_mine_forked_tunnel_8017E25C(Task* task);

#endif // INCLUDE_ROOMS_MINE_FORKED_TUNNEL_H
