#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_GENERAL_STORE_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_GENERAL_STORE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Dryfield night general store placement/resource directory indexed by `GameLocationKey.variant` 0..21.
///
/// Slots 1, 2, 3, 21 publish paired tables; all other slots are absent (NULL, NULL).
/// This placement selector is independent of the room and logical-view selectors.
/// Placements belong to the stage map; resource recipes belong to this room.
/// Keep both overlays and the actors supplying task descriptors loaded during
/// loading, spawning and model-flag updates. Lookups require an index within
/// this array; there is no terminator or bounds check.
extern AreaVariant gDryfieldNightGeneralStoreAreaVariants[22];

/// Logical and mapped view slots supplied by the night General Store.
enum { DRYFIELD_NIGHT_GENERAL_STORE_VIEW_COUNT = 16 };

/// One-based warp slots for the night General Store's three doorways.
enum {
    DRYFIELD_NIGHT_GENERAL_STORE_WARP_GAS_STATION = 1,
    DRYFIELD_NIGHT_GENERAL_STORE_WARP_BACK_STREET = 2,
    DRYFIELD_NIGHT_GENERAL_STORE_WARP_UNDERPASS   = 3,
    DRYFIELD_NIGHT_GENERAL_STORE_WARP_COUNT       = 3,
};

/// The night general store's single room: twelve point lights and ambient minima by view.
///
/// Room 1 selects element zero; ambient entries cover logical views 1..16.
/// The stage map borrows this array and its resources while the room overlay
/// is loaded. Light transforms and attenuation remain writable during use.
extern WorldCoordRoomLighting gDryfieldNightGeneralStoreRoomLighting[1];

/// Room 1's collision mesh, view boundaries, interactions and sight occluders.
///
/// Index by room - 1. The stage map borrows this single-room directory and
/// its resources while the overlay is loaded. The grid is bound to the view;
/// trigger and occluder records stay writable and live until unlinked.
extern WorldCollisionRoomResources gDryfieldNightGeneralStoreRoomCollisionResources[1];

/// Room 1's logical-view map directory, borrowing gameplay's identity mapping.
///
/// Index by room - 1, then logical view - 1. Views 1..16 map to the same
/// 1-based camera, image and sprite indices. The shared map has its own
/// capacity, `VIEW_IDENTITY_MAP_LENGTH`, and no terminator. Keep the room
/// overlay and gameplay loaded while following the directory and map.
extern u8* gDryfieldNightGeneralStoreViewMaps[1];

/// Room 1's signed logical-view search count, covering views 1..16.
///
/// Index by room - 1. This limits reverse lookup in the logical-view map;
/// it does not describe the shared identity map's capacity. The stage map
/// borrows this one-element array while the room overlay is loaded.
extern ViewCount gDryfieldNightGeneralStoreViewCounts[1];

/// Arrival placements and departure options for the room's three doorways.
///
/// Index by the one-based `DRYFIELD_NIGHT_GENERAL_STORE_WARP_*` slot minus one:
/// gas station, back street, underpass. Their initial views are 2, 7 and 14.
/// Transforms use world game units and 4096 yaw units per turn. Entries have
/// no terminator; the stage map borrows them while this overlay is loaded.
extern DirectionWarpEntry gDryfieldNightGeneralStoreWarpEntries[DRYFIELD_NIGHT_GENERAL_STORE_WARP_COUNT];

/// Cameras and projection distances for mapped views 1..16.
///
/// Index by mapped view - 1. Rotation uses 4096 per unit; translation stores
/// the negated world-space camera origin and projection distance is pixels.
/// There is no terminator. The stage map borrows these records while this
/// overlay is loaded; queued camera application must finish before unloading.
extern ViewCamera gDryfieldNightGeneralStoreViewCameras[DRYFIELD_NIGHT_GENERAL_STORE_VIEW_COUNT];

/// Foreground sprite resources for mapped views 1..16.
///
/// Index by mapped view - 1. Views 1, 15 and 16 store their empty batch list
/// in `sources.empty`; the other views store `SpriteSource` arrays in
/// `sources.elements`. Every list begins with a zero-count batch to retain
/// decoded background strips, and `drawAreas` is `NULL` in every view. There is
/// no directory terminator. The stage map borrows the directory and mutable
/// source/batch storage while the room overlay is loaded.
extern SpriteView gDryfieldNightGeneralStoreSpriteViews[DRYFIELD_NIGHT_GENERAL_STORE_VIEW_COUNT];

/// The night general store's collision and footstep policies for surface classes 0..7.
///
/// Every class blocks probes, enables weapon impacts and applies pushback.
/// Classes 0 and 3..7 share one record; class 1 retains a distinct record with
/// the same values. Class 2 selects different footstep cues. The stage map
/// borrows the directory, records and sounds while the room overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightGeneralStoreSurfacePropertiesByClass[8];

/// Draws the nighttime general store's grey light shafts visible in the current view.
///
/// Views 2, 3, 12 and 13 draw one shaft; views 4 and 8 draw two. Other views
/// emit no packets. The task argument is unused. Requires the room overlay to
/// remain loaded, a composed view, an initialized scratch stack and space in
/// the current frame's primitive arena and depth ordering table.
void dryfieldNightGeneralStoreDrawLightShaftsTask(Task* unusedTask);

/// Dispatches the night General Store's room-message receiver task.
///
/// Requires a live bodyless task in state 0..2: initialize, idle, release.
/// The map and room callbacks must stay loaded while dispatching. Initialization
/// registers a borrowed room-task slot; stop messaging before teardown, which
/// leaves that slot unchanged. Neither spawn argument is consumed.
void dryfieldNightGeneralStoreRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_GENERAL_STORE_H
