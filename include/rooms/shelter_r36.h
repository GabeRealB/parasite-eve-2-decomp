#ifndef INCLUDE_ROOMS_SHELTER_R36_H
#define INCLUDE_ROOMS_SHELTER_R36_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// R36's placement layouts, indexed by variant 0..10.
///
/// Variant 1 borrows the two scripted-walker placements and their resource recipes;
/// slots 0 and 2..10 are absent (both pointers NULL). Gameplay's stage-4 area-36
/// record borrows this table and pairs it with the live saved placement state.
/// Keep the room and actor overlays loaded while loading or using the layout.
extern AreaVariant gShelterR36AreaVariants[11];

/// R36's single room entry, mapping logical views to camera/image/sprite indices.
///
/// Indexed by room - 1, with room 1 borrowing `gViewIdentityMap` unchanged.
/// R36 has 11 logical views; that count is independent of the shared map's
/// capacity. The directory stays valid while this room overlay is loaded.
extern u8* gShelterR36RoomViewMaps[1];

/// Logical-view search count for R36's single room: eleven views.
///
/// Indexed by room - 1 (room 1 only), independently of `gViewIdentityMap`'s capacity.
/// The map overlay borrows this count while R36 is loaded; there is no terminator.
extern ViewCount gShelterR36ViewCounts[1];

/// R36's scene and movie arrival endpoints, indexed by warp - 1 (warps 1..2).
///
/// Both place player and companion at the origin with half-turn yaw and select
/// view 2, without sounds, option flags or a map marker. Warp 1 starts the arrival
/// scene; warp 2 starts the movie ending. The stage map borrows these complete
/// records while the room is loaded; the array has no terminator.
extern DirectionWarpEntry gShelterR36WarpEntries[2];

/// R36's class-1 floor quad and its 3 by 3 collision-cell index.
///
/// The floor lies at Y = 0, spanning X and Z from -4000 to 4000 game units.
/// Cells are 4000 units wide with origin (-4000, -4000); all nine lists contain
/// the floor face. Activation binds the mutable grid to the current view.
/// The descriptor and its private mesh/index arrays belong to this overlay
/// and must remain loaded throughout collision queries.
extern WorldCollisionGrid gShelterR36FloorCollisionGrid;

/// R36's eleven cameras, selected by mapped view index - 1 (views 1..11).
///
/// View 1 looks down from above; view 2 has its own camera and projection distance;
/// views 3..11 share the same transform and distance. Rotation uses 4096 per unit,
/// translation is the negated world-space origin, and perspective distance is in
/// pixels. Queued camera tasks borrow these records until application, requiring
/// the room overlay to stay loaded. The array has no terminator.
extern ViewCamera gShelterR36ViewCameras[11];

/// R36's eleven sprite descriptors, indexed by mapped view - 1 (views 1..11).
///
/// Views 2 and 3 borrow source sprites and batch ranges. View 1 and views 4..11
/// retain their batch-list address in `sources.empty` and select no source sprites.
/// Every view has a terminated batch list and NULL clipping areas. Descriptors
/// and their mutable private arrays belong to the room and must stay loaded
/// through packet allocation and drawing; this array itself has no terminator.
extern SpriteView gShelterR36SpriteViews[11];

/// R36's four directional and five point lights for model shading.
///
/// All lights accept every view; there are no cone lights. Coordinate updates
/// and lighting queries mutate the private light arrays, which belong to this
/// overlay and remain valid only while it is loaded.
extern WorldCoordRoomLights gShelterR36RoomLights;

/// Ten directed view boundaries for R36 room 1, including the final LAST record.
///
/// Pairs connect views 5/4, 2/4, 3/2 and two boundaries between 6/2. Origins and
/// relative quad corners use whole room units; normals use 4096 per unit.
/// Room setup binds, links and enables these writable records; collision latches
/// hits and view handling consumes them. The map borrows this array while the
/// room is loaded. LAST ends the scan and is part of the tenth live boundary.
extern WorldCollisionTrigger gShelterR36ViewBoundaryTriggers[10];

/// R36's collision and footstep policies, indexed by surface class 0..7.
///
/// Classes 0, 4, 6 and 7 share a blocking policy without footsteps. Class 1
/// is the floor: probes pass and weapon impacts are ignored. Classes 2, 3 and 5
/// block probes and carry distinct footstep request tables. Every class enables
/// pushback. Records and sound tables are borrowed from this loaded overlay;
/// the floor mesh uses only class 1.
extern WorldCollisionSurfaceProperties* gShelterR36SurfacePropertiesByClass[8];

/// Runs the R36 room controller's initialization, message service or teardown.
///
/// `task->state` must be 0..2: arrival setup, idle service and release.
/// The map overlay spawns this zero-body task for stage 4, area 36. Keep the
/// room overlay and its resources loaded while the controller is live.
void shelterR36RoomTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_R36_H
