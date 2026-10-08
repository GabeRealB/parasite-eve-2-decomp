#ifndef INCLUDE_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_H
#define INCLUDE_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_neo_ark_eve_access_tunnel_801806B8[13];

// neo_ark_eve_access_tunnel
extern WorldCollisionRoomResources D_neo_ark_eve_access_tunnel_8017EB78[];

extern WorldCoordRoomLighting D_neo_ark_eve_access_tunnel_8017EB88[];

extern u8* D_neo_ark_eve_access_tunnel_8017EB90[];

extern ViewCount D_neo_ark_eve_access_tunnel_8017EB94[];

extern DirectionWarpEntry D_neo_ark_eve_access_tunnel_8017EB98[];

extern ViewCamera D_neo_ark_eve_access_tunnel_8017F080[];

extern SpriteView D_neo_ark_eve_access_tunnel_801800A0[];

extern WorldCollisionSurfaceProperties* D_neo_ark_eve_access_tunnel_80180780[];

/// Destruction states accepted by `neoArkEveAccessTunnelSetPartDestroyedSprites`.
enum {
    NEO_ARK_EVE_ACCESS_TUNNEL_PART_INTACT    = 0,
    NEO_ARK_EVE_ACCESS_TUNNEL_PART_DESTROYED = 1,
};

/// Shows or hides the destroyed-part scenery sprites for one tunnel enemy part.
///
/// `partSlot` selects 0 or 1; `destroyed` is 0 for intact, 1 for destroyed.
/// Other byte values do nothing. Slot 0 updates batch 4 of sprite-view element
/// 2; slot 1 updates batch 3 of element 3 and batch 2 of element 4 (zero-based).
/// The current stage and area's tunnel sprite tables must remain loaded.
void neoArkEveAccessTunnelSetPartDestroyedSprites(u8 partSlot, u8 destroyed);

/// Draws the tunnel's grey capsule glows for the current mapped camera view.
///
/// Per-frame effect callback for bank 6, slot 0x14F; `unusedTask` is ignored.
/// Mapped views 2..6 select world-space endpoint pairs, with view 4 also drawing
/// view 5's glows; other views draw nothing. Requires the tunnel overlay and
/// current view transform to remain loaded and the frame's primitive arena
/// ready. Each pair queues an additive capsule using perspective-scaled radii.
void neoArkEveAccessTunnelDrawViewGlowsTask(Task* unusedTask);

/// Runs the EVE access tunnel's room-message task.
///
/// Requires a live task in state 0..2: initialize, update part sprites and
/// backdrop decode masking, then teardown. State 1 remains available for
/// messages. The room overlay must remain loaded through dispatch.
void neoArkEveAccessTunnelRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_H
