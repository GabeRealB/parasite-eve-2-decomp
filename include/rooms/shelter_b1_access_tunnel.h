#ifndef INCLUDE_ROOMS_SHELTER_B1_ACCESS_TUNNEL_H
#define INCLUDE_ROOMS_SHELTER_B1_ACCESS_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b1_access_tunnel_8017FE50[22];

// shelter_b1_access_tunnel
extern u8* D_shelter_b1_access_tunnel_8017E7E4[];

extern ViewCount D_shelter_b1_access_tunnel_8017E7E8[];

extern DirectionWarpEntry D_shelter_b1_access_tunnel_8017E7EC[];

extern WorldCollisionGrid D_shelter_b1_access_tunnel_8017EB24;

extern ViewCamera D_shelter_b1_access_tunnel_8017EB48[];

extern SpriteView D_shelter_b1_access_tunnel_8017F6A0[];

extern WorldCoordRoomLights D_shelter_b1_access_tunnel_8017FA1C;

extern WorldCollisionTrigger D_shelter_b1_access_tunnel_8017FA34[];

extern WorldCollisionTrigger D_shelter_b1_access_tunnel_8017FBFC[];

extern WorldCollisionOccluder D_shelter_b1_access_tunnel_8017FD2C[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_access_tunnel_8017FF24[];

/// Runs the access tunnel's room-message receiver.
///
/// Requires a live task with state 0 (register handlers), 1 (idle), or 2 (kill).
/// Keep the room and Shelter map overlays loaded while its task and borrowed
/// message table remain available. The task allocates no work.
void shelterB1AccessTunnelRoomTask(Task* task);

/// Draws the access tunnel's light glows for mapped views 2 through 5 each frame.
///
/// Other views emit no packets. The task argument is unused; this callback does
/// not advance a state or end its task. Requires the room overlay, current view
/// matrices, scratch stack and frame packet arena to remain available during
/// drawing; queued primitives live in that arena until GPU completion.
void shelterB1AccessTunnelDrawGlowsTask(Task* unused);

#endif // INCLUDE_ROOMS_SHELTER_B1_ACCESS_TUNNEL_H
