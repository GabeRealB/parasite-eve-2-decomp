#ifndef INCLUDE_ROOMS_MINE_TUNNEL_ENTRANCE_H
#define INCLUDE_ROOMS_MINE_TUNNEL_ENTRANCE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_mine_tunnel_entrance_8017F32C[12];

// mine_tunnel_entrance
extern WorldCoordRoomLighting D_mine_tunnel_entrance_8017DB58[];

extern WorldCollisionRoomResources D_mine_tunnel_entrance_8017DB60[];

extern u8* D_mine_tunnel_entrance_8017DB70[];

extern ViewCount D_mine_tunnel_entrance_8017DB74[];

extern DirectionWarpEntry D_mine_tunnel_entrance_8017DB78[];

extern ViewCamera D_mine_tunnel_entrance_8017E0E4[];

extern SpriteView D_mine_tunnel_entrance_8017EA4C[];

extern WorldCollisionSurfaceProperties* D_mine_tunnel_entrance_8017F3E8[];

/// Draws the tunnel entrance's flickering light flares for the current view.
///
/// Bank-6 effect task 0x122 ignores its task argument and enables room view
/// effects on every frame. Mapped views 2..6 select world-space flare centres;
/// other views draw nothing. Each flare's pixel half-extent is its radius scale
/// (768, or 512 for view 6's second flare) times 39 divided by camera Z / 4.
/// Requires nonzero accepted projection depths, the loaded room overlay,
/// composed view matrices, scratch stack, ordering table and packet arena.
/// Queued packets live until the current frame has finished on the GPU.
void mineTunnelEntranceDrawFlaresTask(Task* unusedTask);

/// Runs the mine tunnel entrance's room-message task for one update.
///
/// Requires a live task: state 0 initializes, 1 advances the arrival event,
/// 2 idles and 3 releases the task.
/// The state index is unchecked. Keep this room overlay loaded while the task
/// or its registered message handlers can run; the final state ends its lifetime.
void mineTunnelEntranceRoomTask(Task* task);

#endif // INCLUDE_ROOMS_MINE_TUNNEL_ENTRANCE_H
