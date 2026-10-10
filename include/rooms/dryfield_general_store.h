#ifndef INCLUDE_ROOMS_DRYFIELD_GENERAL_STORE_H
#define INCLUDE_ROOMS_DRYFIELD_GENERAL_STORE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Placement layouts indexed directly by the General Store's saved variant.
///
/// Slots 1 and 10 are populated; 0, 2..9 and 11..12 are absent. Variant 1
/// borrows five placements from the Dryfield map; variant 10 supplies eight
/// room-owned placements. Resources and task recipes remain borrowed while
/// loading or spawning, and placements must outlive actors retaining them.
extern AreaVariant gDryfieldGeneralStorePlacementVariants[13];

/// Collision resources for the General Store's single room.
///
/// Room 1 supplies a mesh, 28 directed view boundaries, 21 action triggers
/// and four sight occluders. Setup binds mutable geometry to the current view;
/// keep the room overlay loaded while the grid is active or records are linked.
extern WorldCollisionRoomResources gDryfieldGeneralStoreRoomCollisionResources[1];

/// Logical-view map directory for the General Store's single room.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1 through 16 select the same
/// 1-based camera, image and sprite indices. Keep the room overlay loaded while
/// borrowing this directory; the map itself belongs to gameplay.
extern u8* gDryfieldGeneralStoreRoomViewMaps[1];

/// Logical-view reverse-search count for the General Store's single room.
///
/// Room 1 searches 16 entries in its identity map. This is an element count,
/// independent of the shared map's capacity; borrow it while the room is loaded.
extern ViewCount gDryfieldGeneralStoreRoomViewCounts[1];

/// Lighting descriptor for the General Store's single room.
///
/// Room 1 borrows twelve mutable point lights and an ambient table for logical
/// views 1 through 16. All referenced storage lives in this room overlay and
/// must remain loaded through coordinate updates and shading queries.
extern WorldCoordRoomLighting gDryfieldGeneralStoreRoomLighting[1];

/// Three arrival/departure endpoints, indexed by 1-based warp ID minus one.
///
/// Slots 1..3 serve the Gas Station, Back Street and Underpass doorways.
/// Arrival views are 2, 7 and 14; map markers 489 and 470 apply to the first
/// two endpoints. There is no terminator. Transitions copy a complete record;
/// keep the room overlay loaded until the copy is made.
extern DirectionWarpEntry gDryfieldGeneralStoreWarpEntries[3];

/// Cameras for mapped General Store views 1 through 16, stored at index minus one.
///
/// The room's identity map selects these records without a terminator.
/// Rotations use 4096 for unit scale, origins use world game coordinates and
/// perspective distances use pixels. Keep the room loaded until queued camera
/// application has copied the selected record.
extern ViewCamera gDryfieldGeneralStoreViewCameras[16];

/// Sprite descriptors for mapped General Store views 1 through 16.
///
/// Indexed by mapped view minus one. Views 2..14 borrow source rectangles;
/// views 1, 15 and 16 retain empty-batch addresses and use decoded image strips.
/// Every batch list ends with SPRITE_BATCH_END, and every view has no clip list.
/// The descriptors and mutable source/batch pools live with the room overlay.
extern SpriteView gDryfieldGeneralStoreSpriteViews[16];

/// Collision policies and footstep cues indexed by room surface class 0 through 7.
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// Class 1 supplies footsteps; classes 0 and 2 through 7 share a silent record.
/// The directory and borrowed records remain valid while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldGeneralStoreSurfacePropertiesByClass[8];

/// Inert callback for the General Store's room-effect slot 0xC4 in task bank 6.
///
/// Ignores `unusedTask`, draws nothing and neither advances nor kills the task.
/// Its coordinate body remains owned by the task until external teardown.
void dryfieldGeneralStoreNoOpEffectTask(Task* unusedTask);

/// Runs the General Store's room receiver through entry setup, idle and teardown.
///
/// Requires a live bodyless task with state 0 (entry), 1 (idle) or 2 (kill).
/// Entry registers `GAME_TASK_SLOT_ROOM`, broadcasts the pending entry actor
/// command or completes the started scene flag, and permits post-CAP sound
/// messages. Requires a live scene manager before the scene starts; keep the
/// store overlay loaded for the task's lifetime.
void dryfieldGeneralStoreRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_GENERAL_STORE_H
