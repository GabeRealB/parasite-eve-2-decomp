#ifndef INCLUDE_ROOMS_MINE_TUNNEL_H
#define INCLUDE_ROOMS_MINE_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of retained placement-variant slots, including absent slots.
enum { MINE_TUNNEL_AREA_VARIANT_COUNT = 12 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
///
/// Slots 1, 2, 3, 4, 11 are present; every other slot is absent (NULL, NULL).
/// Supply an index in 0..11; this directory has no terminator or bounds check.
/// The placement selector is independent of room and view selectors.
/// Placements and recipes belong to this room overlay; keep it loaded while
/// loading or using either table. Referenced actor overlays must be loaded for
/// spawning and model-policy updates.
extern AreaVariant gMineTunnelAreaVariants[MINE_TUNNEL_AREA_VARIANT_COUNT];

/// Light collection and per-view ambient minima for the tunnel's single room.
///
/// Indexed by room - 1. The shelter map borrows this array and its writable
/// light records only while the tunnel overlay is loaded.
extern WorldCoordRoomLighting gMineTunnelRoomLighting[1];

extern WorldCollisionRoomResources D_mine_tunnel_8017E15C[];

/// Logical-view mapping for the tunnel's single room, indexed by room - 1.
///
/// Borrows gameplay's `gViewIdentityMap`; logical views 1..5 map unchanged to
/// the five camera and sprite slots. The room directory lives with this overlay.
extern u8* gMineTunnelRoomViewMaps[1];

extern ViewCount D_mine_tunnel_8017E170[];

extern DirectionWarpEntry D_mine_tunnel_8017E174[];

/// Five tunnel cameras, indexed by the mapped 1-based view minus one.
///
/// Rotation uses 4096 per unit, translation is the negated world-space origin,
/// and projection distances are pixels. Records must remain loaded until applied.
extern ViewCamera gMineTunnelViewCameras[5];

/// Sprite sources and batches for mapped tunnel views 1..5, indexed at view - 1.
///
/// View 1 has no source sprites; the other views have 10, 51, 63 and 74.
/// Every view retains decoded background strips and has no clipping-area list.
/// The descriptors and their mutable lists are borrowed while the overlay is loaded.
extern SpriteView gMineTunnelSpriteViews[5];

/// Tunnel collision policies for surface classes 0..7, with footstep cues on class 1.
///
/// All classes block probes, allow weapon impacts and apply pushback. Class 1
/// is the floor; the other slots share a record without footstep cues.
/// The shelter map borrows the table and records while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gMineTunnelSurfaceProperties[8];

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

/// Runs the tunnel's room-message receiver and encounter-restoration setup.
///
/// Requires the loaded tunnel and a live task in state 0 (initialize), 1 (idle)
/// or 2 (release). Borrows the room message table while registered; the body and
/// spawn arguments are unused. Teardown leaves the resident room slot set,
/// so message senders must stop using it before the task is killed.
void mineTunnelRoomTask(Task* task);

#endif // INCLUDE_ROOMS_MINE_TUNNEL_H
