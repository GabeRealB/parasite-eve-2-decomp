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

/// Runs the switch-controlled area object's setup, path update or teardown state.
///
/// Requires this room overlay loaded, a live TMD task and `state` in 0..2.
/// State 0 initializes work and a child; state 1 updates motion and drawing;
/// state 2 releases the area object's enemy allocation and starts task teardown.
/// No state runs while scene actor control is paused. Teardown may release the task.
void mineForkedTunnelAreaObjectTask(Task* task);

/// Runs the room controller's setup, idle message-receiving or teardown state.
///
/// Requires this room overlay loaded and `state` in 0..2. State 0 installs
/// room handlers, registers the task in `GAME_TASK_SLOT_ROOM` and initializes
/// room sprite visibility; state 1 remains idle; state 2 starts task teardown.
/// This controller continues dispatching while scene actor control is paused.
void mineForkedTunnelRoomTask(Task* task);

#endif // INCLUDE_ROOMS_MINE_FORKED_TUNNEL_H
