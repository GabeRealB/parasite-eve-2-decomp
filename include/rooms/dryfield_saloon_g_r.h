#ifndef INCLUDE_ROOMS_DRYFIELD_SALOON_G_R_H
#define INCLUDE_ROOMS_DRYFIELD_SALOON_G_R_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of retained placement-variant slots, including absent slots.
enum { DRYFIELD_SALOON_G_R_AREA_VARIANT_COUNT = 13 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
///
/// Slots 1, 2 are present; every other slot is absent (NULL, NULL).
/// Supply an index in 0..12; this directory has no terminator or bounds check.
/// The placement selector is independent of room and view selectors.
/// Placements belong to the stage map, recipes to this room. Keep both overlays
/// loaded while loading or using either table. Referenced actor overlays must
/// be loaded for spawning and model-policy updates.
extern AreaVariant gDryfieldSaloonGRAreaVariants[DRYFIELD_SALOON_G_R_AREA_VARIANT_COUNT];

/// Logical-view search length and independent camera-table extent for the daytime saloon.
enum {
    DRYFIELD_SALOON_G_R_LOGICAL_VIEW_COUNT = 13,
    DRYFIELD_SALOON_G_R_CAMERA_COUNT       = 13,
};

/// Collision resources shared by daytime saloon room selectors 1 and 2.
///
/// Indexed by room minus one, with no terminator. Both entries borrow one
/// grid, 16 view boundaries, 14 action triggers and two sight occluders.
/// Setup binds the grid/trigger coordinates and links the mutable records
/// through their LAST flags. Keep this overlay loaded until they are retired.
extern WorldCollisionRoomResources gDryfieldSaloonGRRoomCollisionResources[2];

/// Logical-view maps for daytime saloon room selectors 1 and 2.
///
/// Indexed by room minus one; both entries borrow the same unsigned byte map.
/// Logical views 1..13 select camera, image and sprite indices 1..9 or 13;
/// views 9..12 all select resource index 9. No map terminator is used.
/// The saloon overlay must remain loaded while its directory or map is used.
extern u8* gDryfieldSaloonGRRoomViewMaps[2];

/// Thirteen logical views in each of the daytime saloon's two room selectors.
///
/// Indexed by room minus one, with no terminator. These signed halfword counts
/// limit reverse searches of the room's logical-view map; they do not measure
/// the camera table's extent. The loaded saloon overlay owns the count array.
extern ViewCount gDryfieldSaloonGRRoomViewCounts[2];

/// Lighting for daytime saloon room selectors 1 and 2.
///
/// Indexed by room minus one; both entries share seven mutable point lights.
/// No directional or cone lights are supplied; NULL ambient tables select the
/// default view minima. Descriptors and light storage borrow the loaded overlay.
extern WorldCoordRoomLighting gDryfieldSaloonGRRoomLighting[2];

/// Parking-lot and kitchen warp endpoints in the daytime saloon.
///
/// Indexed by warp slot minus one: slot 1 is the parking-lot door and slot 2
/// the kitchen door, with no terminator. Player and companion share each
/// arrival pose in room-coordinate units; yaw uses 4096 units per turn. Arrivals
/// select logical views 2 and 7 respectively; neither endpoint has a blocked
/// sound or special transition flags. Keep this overlay loaded during lookups.
extern DirectionWarpEntry gDryfieldSaloonGRWarpEntries[2];

/// Thirteen mapped-view camera records for the daytime saloon.
///
/// Indexed by mapped resource view minus one, with no terminator. Logical
/// views 9..12 all select slot 9; camera slots 10..12 remain unselected by the
/// current map. Rotation uses `ONE` per unit, translation is the negated camera
/// origin in world units, and projection distance is in pixels. A queued
/// camera application borrows its record until dispatch, so keep this overlay
/// loaded through that dispatch.
extern ViewCamera gDryfieldSaloonGRViewCameras[DRYFIELD_SALOON_G_R_CAMERA_COUNT];

/// Thirteen mapped-view sprite descriptors for the daytime saloon.
///
/// Indexed by mapped resource view minus one, independently of room selector.
/// Slots 2..7 have source sprites; the other slots retain empty batch lists.
/// Every list keeps decoded background strips, and no slot has clipping areas.
/// Resource slots 10..12 are unselected by the current logical-view map.
/// Descriptors and mutable source/batch storage stay live with the room overlay.
extern SpriteView gDryfieldSaloonGRSpriteViews[13];

/// Collision response and footsteps for the saloon's eight surface classes.
///
/// Indexed directly by class 0..7 from grid faces or contacts, with no terminator.
/// Classes 0 and 4..7 share silent blocking properties; class 1 has its own
/// footsteps, and classes 2/3 share another cue set. Only class 3 passes probes
/// and suppresses weapon impacts; every class applies pushback.
/// The directory and its borrowed records stay live with the saloon overlay.
extern WorldCollisionSurfaceProperties* gDryfieldSaloonGRSurfacePropertiesByClass[8];

/// Queues the saloon's view-gated flares, twin light shafts and tapered beam each frame.
///
/// Requires a live `TASK_BODY_COORD` task with an already composed local-to-world
/// coordinate, the saloon overlay loaded, and its current 1-based view in 1..13.
/// Point entries 0..5 select texture column 0 and 6..10 select column 2; the
/// two shafts share roots 14/17 with guide pairs 15/18 and 16/19. The beam
/// runs from point 13 to point 12. Flare half-extents are `512 * 39 / depth`
/// pixels and beam radii are `256 * 64 / depth`, with depth in camera Z / 4.
/// Flares, each shaft's last tip and the beam's second end require depth >= 17;
/// the beam's first depth is clamped to 16.
///
/// Requires initialized view matrices, scratch stack, ordering table and a
/// writable frame packet arena. Packets remain live through GPU completion;
/// no input pointers are retained and the task's state is unchanged.
void dryfieldSaloonGRDrawLightEffectsTask(Task* task);

/// Runs the daytime saloon's room initialization, message wait and teardown.
///
/// State 0 installs the room handlers; state 1 waits for messages;
/// state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldSaloonGRRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_SALOON_G_R_H
