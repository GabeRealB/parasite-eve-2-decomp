#ifndef INCLUDE_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_H
#define INCLUDE_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of retained placement-variant slots, including absent slots.
enum { NEO_ARK_EVE_ACCESS_TUNNEL_AREA_VARIANT_COUNT = 13 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
///
/// Slots 1, 11 are present; every other slot is absent (NULL, NULL).
/// Supply an index in 0..12; this directory has no terminator or bounds check.
/// The placement selector is independent of room and view selectors.
/// Placements belong to the stage map, recipes to this room. Keep both overlays
/// loaded while loading or using either table. Referenced actor overlays must
/// be loaded for spawning and model-policy updates.
extern AreaVariant gNeoArkEveAccessTunnelAreaVariants[NEO_ARK_EVE_ACCESS_TUNNEL_AREA_VARIANT_COUNT];

/// Counts of mapped view resources and warp endpoints in the tunnel.
enum {
    NEO_ARK_EVE_ACCESS_TUNNEL_VIEW_COUNT = 7,
    NEO_ARK_EVE_ACCESS_TUNNEL_WARP_COUNT = 2,
};

/// Room 1's collision grid, view boundaries, action triggers and sight occluder.
///
/// The stage map borrows this one-entry room array at area 8. Setup binds the
/// grid to the view and links the mutable trigger/occluder pools; keep the
/// tunnel overlay loaded until those resources have been unlinked.
extern WorldCollisionRoomResources gNeoArkEveAccessTunnelCollisionRooms[1];

/// Room 1's five point lights and default minimum ambient levels.
///
/// The stage map borrows this one-entry room array at area 8. A NULL ambient
/// table selects gameplay's default minima. The record and its mutable light
/// collection stay live only while the tunnel overlay is loaded.
extern WorldCoordRoomLighting gNeoArkEveAccessTunnelRoomLighting[1];

/// Room 1's logical-view map, sharing gameplay's identity mapping.
///
/// Logical views 1..7 map to the same 1-based camera/image/sprite indices.
/// This one-entry room directory belongs to the tunnel overlay; the borrowed
/// map belongs to gameplay and only its first seven entries serve this room.
extern u8* gNeoArkEveAccessTunnelViewMaps[1];

/// Room 1's seven logical views, used as the reverse-mapping search length.
///
/// The stage map borrows this one-entry signed-halfword count array at area 8.
/// It counts logical views independently of the shared identity map's capacity.
/// Keep the tunnel overlay loaded while looking up the count.
extern ViewCount gNeoArkEveAccessTunnelViewCounts[1];

/// The tunnel's Shelter-side and EVE-elevator-side warp endpoints, in that order.
///
/// The stage map indexes these with warp - 1 for warp IDs 1 and 2. Each endpoint
/// places the player and companion at the same position and yaw, using world
/// units and 4096 angle units per turn.
/// Shelter arrival selects view 2 with no sound; elevator arrival selects view
/// 5 and area-bank cue 4, while departure uses cue 3. Neither endpoint requests
/// scripted-player mode or departure fading. Later arrivals mark the stored
/// map-flag nibble. Keep the overlay loaded while selecting or copying an entry.
extern DirectionWarpEntry gNeoArkEveAccessTunnelWarpEntries[NEO_ARK_EVE_ACCESS_TUNNEL_WARP_COUNT];

/// The tunnel's seven cameras, indexed by mapped view - 1 for mapped views 1..7.
///
/// Rotations use `ONE` (4096) for 1.0, translations are negated world-space
/// camera origins, and projection distances are pixels. The stage map borrows
/// this array at area 8; keep the overlay loaded through queued application.
extern ViewCamera gNeoArkEveAccessTunnelViewCameras[NEO_ARK_EVE_ACCESS_TUNNEL_VIEW_COUNT];

/// Neo Ark eve access tunnel's sprite resources, indexed by mapped view minus one.
///
/// Views 1, 6 and 7 have no sources; views 2..5 borrow sprite pools.
/// The room uses an identity logical-view map.
/// Every slot retains decoded image strips and has no clipping list. Empty
/// source slots retain batch addresses without supplying SpriteSource elements.
/// Descriptors and mutable source/batch lists stay borrowed while the owning
/// room overlay is loaded; selectors must be nonzero and within this array.
extern SpriteView gNeoArkEveAccessTunnelSpriteViews[NEO_ARK_EVE_ACCESS_TUNNEL_VIEW_COUNT];

/// Collision policy and footstep records indexed by tunnel surface class 0..7.
///
/// Classes 1 and 2 have footstep cues; 0 and 3..7 share a silent record. Every
/// class blocks probes, permits weapon impacts and applies pushback. The stage
/// map borrows this table only while the tunnel overlay is loaded.
extern WorldCollisionSurfaceProperties* gNeoArkEveAccessTunnelSurfaceProperties[8];

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
