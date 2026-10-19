#ifndef INCLUDE_ROOMS_MIST_PARKING_H
#define INCLUDE_ROOMS_MIST_PARKING_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Bodyless priority-192 display task for this room's shop UI session.
///
/// `spawnArg1.value` carries the packed stock-profile/category selector.
/// The task creates and owns its UI tree, retaining the root in spawnArg2.
/// Gameplay queues this exported recipe by saved stage/area; the parking
/// overlay, shop panels and catalog resources must remain loaded until exit.
extern TaskDesc gMistParkingShopSessionTaskDesc;

/// Number of retained placement-variant slots, including absent slots.
enum { MIST_PARKING_AREA_VARIANT_COUNT = 13 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
///
/// Slots 1, 2 are present; every other slot is absent (NULL, NULL).
/// Supply an index in 0..12; this directory has no terminator or bounds check.
/// The placement selector is independent of room and view selectors.
/// Placements belong to the stage map, recipes to this room. Keep both overlays
/// loaded while loading or using either table. Referenced actor overlays must
/// be loaded for spawning and model-policy updates.
extern AreaVariant gMistParkingAreaVariants[MIST_PARKING_AREA_VARIANT_COUNT];

/// Dimensions of the parking area's room variants, logical views and surface classes.
enum {
    MIST_PARKING_ROOM_VARIANT_COUNT  = 4,
    MIST_PARKING_VIEW_COUNT          = 20,
    MIST_PARKING_WARP_COUNT          = 4,
    MIST_PARKING_SURFACE_CLASS_COUNT = 8
};

/// Collision resources for parking room variants 1..4, indexed by room - 1.
///
/// All variants share the writable grid and view boundaries. Rooms 1/2 select
/// the early-chapter action triggers; rooms 3/4 select the later set. There are
/// no occluder records. The map directory borrows these resources while the
/// parking overlay is loaded; linked grid and trigger storage must stay live.
extern WorldCollisionRoomResources gMistParkingRoomCollisionResources[MIST_PARKING_ROOM_VARIANT_COUNT];

/// Logical-view maps for the parking area's four room variants.
///
/// Index by room - 1, then logical view - 1, with rooms 1..4 and views 1..20.
/// Each byte is a 1-based camera/image/sprite index. Rooms 1/2 use the early
/// chapter permutation; rooms 3/4 borrow `gViewIdentityMap`. No terminator is
/// stored. The map overlay borrows this directory while this room is loaded.
extern u8* gMistParkingViewMaps[MIST_PARKING_ROOM_VARIANT_COUNT];

/// Logical-view search lengths for parking room variants 1..4, indexed by room - 1.
///
/// Each signed 16-bit element counts twenty entries in the variant's view map.
/// These bound reverse searches; they do not count sprite source records.
/// No terminator is stored. The map directory borrows the loaded room's array.
extern ViewCount gMistParkingViewCounts[MIST_PARKING_ROOM_VARIANT_COUNT];

/// Light collections and ambient minima for the parking area's four room variants.
///
/// Index by room - 1 for rooms 1..4. Rooms 1/3 select 28 point lights and the
/// default ambient minima; rooms 2/4 select two point lights and a table of
/// minimum ambient RGB levels indexed by logical view 1..20. Light transforms
/// and attenuation stay writable. All borrowed data requires this overlay loaded.
extern WorldCoordRoomLighting gMistParkingRoomLightingTable[MIST_PARKING_ROOM_VARIANT_COUNT];

/// Four parking arrival/departure endpoints, indexed by 1-based warp ID minus one.
///
/// Player and companion share each endpoint's placement, in world units and
/// 4096 yaw units per turn. Default logical views are 2, 3, 10 and 5. Endpoint 1
/// has arrival/departure door sounds; the others are silent. No map marker or
/// scripted-player/fade flags are set. Gameplay copies each complete entry;
/// the map directory borrows this array only while the parking overlay is loaded.
extern DirectionWarpEntry gMistParkingWarpEntries[MIST_PARKING_WARP_COUNT];

/// Cameras for mapped parking view slots 1..20, indexed by mapped index minus one.
///
/// Rotation coefficients use 4096 per unit, translations are negated world
/// origins, and projection distances are pixels. All room variants share this
/// array through their logical-view maps. Queued camera tasks borrow records
/// until dispatch, requiring the parking overlay to remain loaded.
extern ViewCamera gMistParkingViewCameras[MIST_PARKING_VIEW_COUNT];

/// Sprite resources for mapped parking view slots 1..20, indexed by mapped index minus one.
///
/// Every slot keeps the decoded background strips and has no draw-area list.
/// Nonempty slots borrow source rectangles and sentinel-terminated drawing
/// ranges; empty slots retain their batch address in `sources.empty` without
/// supplying sprites. The map directory, allocation and drawing borrow these
/// writable descriptors and lists while the parking overlay is loaded.
extern SpriteView gMistParkingSpriteViews[MIST_PARKING_VIEW_COUNT];

/// Collision policies and footstep cues indexed by parking surface class 0..7.
///
/// Every class blocks probes, allows weapon impacts and applies pushback.
/// Classes 1/3 select paired walk/run cues; classes 0/2/4..7 have no footstep
/// table. The leading policy byte's role is unproven. The stage-map directory
/// borrows these records only while the parking overlay is loaded.
extern WorldCollisionSurfaceProperties* gMistParkingSurfacePropertiesByClass[MIST_PARKING_SURFACE_CLASS_COUNT];

/// Placement choices for the collision patch controlled with Pierce's visibility.
enum {
    MIST_PARKING_PIERCE_PATCH_RESTORED = 0,
    MIST_PARKING_PIERCE_PATCH_LOWERED  = 1
};

/// Restores Pierce's collision patch, optionally lowering it by 2000 world Y units.
///
/// Zero restores its original position; every nonzero `lowerPatch` applies the
/// positive Y offset. Re-copies two normals/faces and six XYZ vertices into the
/// live room grid before translating, so repeated calls never accumulate offsets.
/// Vector fourth halfwords, the rest of the grid and its cell table are untouched.
/// Requires the mist_parking overlay and its writable live grid to remain loaded.
void mistParkingSetPierceCollisionPatchLowered(s32 lowerPatch);

/// Updates the parking-lot telephone's save menu and optional play statistics.
///
/// `spawnArg2.pointer` must hold the live UI object owned by this menu task.
/// Its save and statistics children must remain linked until their answers
/// are consumed. Requires the parking overlay and telephone UI resources.
void mistParkingTelephoneMenuTask(Task* task);

/// Starts the selected parking departure script once EVS and CAP are idle.
///
/// The room answer word selects departure when it is 2, staying otherwise.
/// The producer of that word is unproven. Kills this continuation after starting
/// the script; the callback does not use its work, body or spawn arguments.
/// Requires the parking overlay and its dialogue resources to remain loaded.
void mistParkingContinueDepartureChoiceTask(Task* task);

/// Draws the current mapped camera view's grey capsule glows and pulsing cyan point glow.
///
/// Per-frame effect callback for bank 6, slot 0x39; `unused` is ignored.
/// Requires the room overlay and view map to remain loaded, the current view
/// matrix to be composed, and an initialized scratch stack, ordering table and
/// packet arena. Queued additive packets live until that frame's GPU work ends.
/// Views without a listed glow emit no packets; task state is unchanged.
void mistParkingDrawGlowsTask(Task* unused);

/// Runs the parking-lot room's message receiver and arrival conversation setup.
///
/// `state` is 0 to initialize, 1 to idle or 2 to release, with no bounds check.
/// Initialization registers the task in the room slot and may start the
/// variant-2 arrival conversation. Requires the live session, save state and
/// parking overlay to stay loaded until the task is released.
void mistParkingRoomTask(Task* task);

#endif // INCLUDE_ROOMS_MIST_PARKING_H
