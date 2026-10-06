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

extern AreaVariant D_mine_forked_tunnel_80185504[12];

extern TmdSource gMineForkedTunnelModel01B48;

// mine_forked_tunnel
extern WorldCoordRoomLighting D_mine_forked_tunnel_80183634[];

extern WorldCollisionRoomResources D_mine_forked_tunnel_8018363C[];

extern u8* D_mine_forked_tunnel_8018364C[];

extern ViewCount D_mine_forked_tunnel_80183650[];

extern DirectionWarpEntry D_mine_forked_tunnel_80183654[];

extern ViewCamera D_mine_forked_tunnel_80183D94[];

extern SpriteView D_mine_forked_tunnel_80184D64[];

extern WorldCollisionSurfaceProperties* D_mine_forked_tunnel_801855C0[];

/// Draws the tunnel's flickering light flares for the current mapped view.
///
/// Enables view room effects on every call. Views 2 and 3 draw one shared
/// anchor, view 4 draws two anchors, and view 5 draws one; other views draw
/// none. Requires this room's data, composed view matrix, scratch stack and
/// current-frame GPU packet arena and ordering table. Flare half-extent is
/// `768 * 39 / depth` pixels, with nonzero depth equal to camera Z / 4.
/// Gameplay dispatches this callback from effect slot 0x126; the task is ignored.
void mineForkedTunnelDrawViewFlaresTask(Task* unusedTask);

void func_mine_forked_tunnel_8017DBE4(Task* task);

void func_mine_forked_tunnel_8017E25C(Task* task);

#endif // INCLUDE_ROOMS_MINE_FORKED_TUNNEL_H
