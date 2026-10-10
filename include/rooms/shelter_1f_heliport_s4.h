#ifndef INCLUDE_ROOMS_SHELTER_1F_HELIPORT_S4_H
#define INCLUDE_ROOMS_SHELTER_1F_HELIPORT_S4_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of logical views and identity-mapped camera/sprite slots in the stage-4 heliport.
enum { SHELTER_1F_HELIPORT_S4_VIEW_COUNT = 5 };

/// Room 1's logical-view map, borrowing `gViewIdentityMap` for views 1..5.
///
/// The map overlay borrows this one-entry room directory while the heliport is
/// loaded; the mapped indices select the camera and sprite arrays below.
extern u8* gShelter1fHeliportS4RoomViewMaps[1];

/// Room 1's five logical views, limiting reverse searches of its view map.
extern ViewCount gShelter1fHeliportS4RoomViewCounts[1];

/// Warp endpoint 1: both actors arrive at (8000, 0, 1632), yaw zero, in view 2.
///
/// No transition sounds, special spawn flags or map marker are supplied. The
/// stage map borrows this endpoint while the heliport overlay remains loaded.
extern DirectionWarpEntry gShelter1fHeliportS4WarpEntries[1];

/// The heliport's 15-face collision mesh, indexed by a 3x3 XZ cell grid.
///
/// Cells span 4000 game units, with origin (0, -2000) in XZ. Faces use surface
/// class 0 and index seven normals and 34 vertices. Room setup binds the view
/// coordinate; the descriptor and writable pools stay live until room unload.
extern WorldCollisionGrid gShelter1fHeliportS4CollisionGrid;

/// World-to-camera transforms and projection distances for mapped views 1..5.
///
/// Rotation uses `ONE` per unit; translation is the negated world origin in game
/// units and projection distance is in pixels. Queued camera requests borrow
/// these records until applied, requiring the heliport overlay to stay loaded.
extern ViewCamera gShelter1fHeliportS4ViewCameras[SHELTER_1F_HELIPORT_S4_VIEW_COUNT];

/// Image-strip backgrounds without source sprites or clipping for mapped views 1..5.
///
/// Each `SpriteView::sources.empty` address retains its batch list and supplies no sprite
/// elements. The descriptors and mutable lists belong to the loaded heliport.
extern SpriteView gShelter1fHeliportS4SpriteViews[SHELTER_1F_HELIPORT_S4_VIEW_COUNT];

/// Four directional and five point lights, active in every heliport view, with no cone lights.
///
/// Placement uses world game units and RGB intensity has 12 fractional bits.
/// Coordinate updates and shading mutate the borrowed arrays while this
/// overlay is loaded. The stage map supplies the default ambient minima.
extern WorldCoordRoomLights gShelter1fHeliportS4RoomLights;

/// Directed view boundaries pairing 2 with 3, 2 with 5, and 3 with 4.
///
/// The final record carries `WORLD_COLLISION_TRIGGER_LAST`. Room setup binds coordinates,
/// links and enables the records; keep this writable storage loaded until
/// unlinked. View 1 has no boundary in this array.
extern WorldCollisionTrigger gShelter1fHeliportS4ViewBoundaryTriggers[6];

/// Surface classes 0..7 share blocking probes, weapon impacts and pushback, without footstep cues.
///
/// Entries borrow one room-owned policy for the lifetime of the loaded heliport.
extern WorldCollisionSurfaceProperties* gShelter1fHeliportS4SurfacePropertiesByClass[8];

/// Maintains the heliport room's message receiver for the loaded room overlay.
///
/// State 0 installs and publishes the receiver, state 1 idles, and state 2
/// releases the task. Call with a live task whose state is in 0..2. Spawn
/// arguments are unused; map_shelter selects this callback for stage 4, area 38.
void shelter1fHeliportS4RoomTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_1F_HELIPORT_S4_H
