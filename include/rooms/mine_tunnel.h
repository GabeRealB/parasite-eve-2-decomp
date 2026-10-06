#ifndef INCLUDE_ROOMS_MINE_TUNNEL_H
#define INCLUDE_ROOMS_MINE_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_mine_tunnel_801801CC[12];

// mine_tunnel
extern WorldCoordRoomLighting D_mine_tunnel_8017E154[];

extern WorldCollisionRoomResources D_mine_tunnel_8017E15C[];

extern u8* D_mine_tunnel_8017E16C[];

extern ViewCount D_mine_tunnel_8017E170[];

extern DirectionWarpEntry D_mine_tunnel_8017E174[];

extern ViewCamera D_mine_tunnel_8017E890[];

extern SpriteView D_mine_tunnel_8017F9A4[];

extern WorldCollisionSurfaceProperties* D_mine_tunnel_8018032C[];

/// Draws the mine tunnel's flickering light flares for the current mapped view.
///
/// Enables room-view ambient effects every tick. Mapped views 2, 3, 4 and 5
/// select one, five, two and two flare anchors respectively; other indices draw
/// none. Rejected projections emit no packets.
/// Requires the tunnel overlay to stay loaded, a live `gRoomEffectState`,
/// composed view matrices, an initialized scratch stack and the current frame's
/// packet arena and ordering table, with room for up to five textured quads.
/// Accepted projections require nonzero camera Z / 4. Packets live until that
/// frame's GPU draw completes. `unusedTask` and its spawn arguments are ignored.
void mineTunnelDrawViewFlaresTask(Task* unusedTask);

void func_mine_tunnel_8017D77C(Task* task);

#endif // INCLUDE_ROOMS_MINE_TUNNEL_H
