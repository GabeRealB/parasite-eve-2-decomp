#ifndef INCLUDE_ROOMS_DRYFIELD_G_R_KITCHEN_H
#define INCLUDE_ROOMS_DRYFIELD_G_R_KITCHEN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_g_r_kitchen_8017F4B8[13];

/// Number of identity-mapped camera, image and sprite views in the daytime kitchen.
enum { DRYFIELD_G_R_KITCHEN_VIEW_COUNT = 3 };

/// Collision resources for the daytime kitchen's sole room selector, room 1.
///
/// Indexed by room minus one. Borrows the mutable room grid, two view boundaries
/// and seven action triggers; there are no sight occluders. Keep this overlay
/// loaded while the grid is active or its triggers are linked.
extern WorldCollisionRoomResources gDryfieldGRKitchenRoomCollisionResources[1];

/// Logical-view map directory for the daytime kitchen's sole room selector.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..3 select resource indices
/// 1..3. Indexed by room minus one, then view minus one, with no terminator.
/// The directory lives with this overlay; the shared map lives with gameplay.
extern u8* gDryfieldGRKitchenViewMaps[1];

/// Reverse-lookup view counts for the daytime kitchen's sole room selector.
///
/// Room 1 searches three logical-view map entries. Indexed by room minus one;
/// this signed element count is independent of the shared map's capacity.
/// Borrow only while this overlay is loaded.
extern ViewCount gDryfieldGRKitchenViewCounts[1];

/// Model lighting for the daytime kitchen's sole room selector.
///
/// Room 1 has four mutable point lights admitting all views, with no directional
/// or cone lights. A NULL ambient table selects the default view minima.
/// Indexed by room minus one; descriptors and light storage live with this overlay.
extern WorldCoordRoomLighting gDryfieldGRKitchenRoomLighting[1];

/// Arrival and departure descriptors for the saloon and water-tower doors.
///
/// Warp slots 1 and 2 respectively, indexed by slot minus one. Both arrive in
/// view 2, with positions in world units and yaw in 4096 units per turn. Each
/// carries its door's map marker and kitchen-bank arrival/departure sounds.
/// Complete records are copied by transition code while this overlay is loaded.
extern DirectionWarpEntry gDryfieldGRKitchenWarpEntries[2];

/// Three daytime kitchen cameras, indexed by mapped view minus one.
///
/// View 1 looks straight down; views 2 and 3 use perspective distance 230 pixels.
/// Matrix rotations have 12 fractional bits and translations are negated camera
/// origins in world units. Keep this overlay loaded until queued application.
extern ViewCamera gDryfieldGRKitchenViewCameras[DRYFIELD_G_R_KITCHEN_VIEW_COUNT];

/// Three daytime kitchen sprite views, indexed by mapped view minus one.
///
/// View 2 selects seven depth-sorted rectangles; views 1 and 3 have empty lists.
/// All retain decoded background strips and use no draw-area clipping. Mutable
/// descriptors, batches and sources remain borrowed while this overlay is loaded.
extern SpriteView gDryfieldGRKitchenSpriteViews[DRYFIELD_G_R_KITCHEN_VIEW_COUNT];

/// Collision and footstep policies for the daytime kitchen's surface classes 0..7.
///
/// All classes block probes, allow weapon impacts and apply pushback. Class 1
/// has footstep cues; the other seven classes share a silent record. The mesh
/// uses classes 0 and 1, while pushback setup reads all eight entries.
/// Borrowed records and sound tables live with this overlay.
extern WorldCollisionSurfaceProperties* gDryfieldGRKitchenSurfacePropertiesByClass[8];

/// Draws the kitchen's two additive light beams for the current view.
///
/// View 2 uses brighter grey beams; view 3 uses dimmer grey beams. Other
/// views queue nothing. `task` must have a live `TASK_BODY_COORD` body whose
/// coordinate cache maps the room's local endpoints into `GsWSMATRIX`'s input
/// space, including the view parent in the composed transform.
/// Borrows that coordinate for the call without changing task state.
/// Requires composed view matrices, an initialized scratch stack with a free
/// 40-byte block, and a current ordering table and frame arena with room for
/// twelve `POLY_G4` and twelve `DR_TPAGE` packets. Queued packets must remain
/// live until GPU completion.
void dryfieldGRKitchenDrawLightBeamsTask(Task* task);

/// Runs the G & R kitchen's room receiver through initialization, idle and teardown.
///
/// Requires a live bodyless task with state 0 (register), 1 (idle) or 2 (kill).
/// Initialization installs the room message table in `GAME_TASK_SLOT_ROOM`.
/// Keep the kitchen overlay loaded for the task's lifetime.
void dryfieldGRKitchenRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_G_R_KITCHEN_H
