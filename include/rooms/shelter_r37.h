#ifndef INCLUDE_ROOMS_SHELTER_R37_H
#define INCLUDE_ROOMS_SHELTER_R37_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Room 1's logical-view map for Mine/Shelter area 37.
///
/// Borrows `gViewIdentityMap`: logical views 1..3 select the same 1-based
/// camera, image and sprite indices. Keep this room overlay loaded during lookup.
extern u8* gShelterR37ViewMaps[1];

/// Room 1's reverse-lookup extent: three logical views, without a terminator.
extern ViewCount gShelterR37ViewCounts[1];

/// Area 37's single warp endpoint, indexed by warp ID 1.
///
/// Places both actors at (3756, 0, -197), facing a quarter turn, in view 2.
/// Supplies no transition sounds, map marker or special spawn flags. The
/// loaded overlay owns the record; transitions copy it by value.
extern DirectionWarpEntry gShelterR37WarpEntries[1];

/// Area 37's room-space collision mesh and its 3-by-2 XZ cell index.
///
/// Contains 12 quads, 22 vertices and six unit normals; cells span 4000 game
/// units with biases (4250, 3500). Setup binds the mutable grid to the current
/// view. The descriptor and private mesh pools must remain loaded while active.
extern WorldCollisionGrid gShelterR37RoomCollision;

/// Cameras for mapped view indices 1..3, stored at index minus one.
///
/// Matrices use 12 fractional bits; translation is the negated camera origin
/// in world units, and screen distance is in pixels. No fourth camera exists.
/// Keep the overlay loaded until any borrowed camera request is applied.
extern ViewCamera gShelterR37ViewCameras[3];

/// Sprite descriptors for mapped views 1..3, each selecting decoded image strips.
///
/// Each view has a zero-count first batch followed by a terminal batch, with
/// no source sprites or clipping rectangles. The source slot retains the batch
/// address through its empty interpretation. Resources live in this overlay.
extern SpriteView gShelterR37SpriteViews[3];

/// Room 1's four directional and five point lights, with no cone lights.
///
/// Every light contributes in all views. Runtime composition and shading write
/// the private source arrays, which must remain loaded with this collection.
extern WorldCoordRoomLights gShelterR37RoomLights;

/// Mutable camera-boundary quads for view transitions 3->2, 2->3, 3->4 and 4->3.
///
/// The fourth included record ends the array with LAST. Setup binds and links
/// the records to the current view; keep the overlay loaded until unlinked.
/// View 4 is retained resource data with no corresponding camera or sprite
/// descriptor; whether its boundary can be reached is unproven.
extern WorldCollisionTrigger gShelterR37ViewBoundaryTriggers[4];

/// Collision policies indexed by room-local surface class 0..7, without a terminator.
///
/// Classes 0, 4, 6 and 7 share the default blocking policy without footsteps;
/// class 1 passes probes and ignores weapon impacts, while retaining pushback.
/// Classes 2, 3 and 5 block probes and supply type-1 footstep cue pairs. Their
/// materials and sound-bank contents are unproven. Records live in this overlay.
extern WorldCollisionSurfaceProperties* gShelterR37SurfacePropertiesByClass[8];

/// Runs the room controller's initialize, idle or teardown state.
///
/// Requires this overlay loaded and a live bodyless task with state 0..2.
/// State 0 publishes the task in `GAME_TASK_SLOT_ROOM` and installs its message
/// table; state 1 waits without per-frame work, and state 2 releases the task.
/// Allocates no work. The Shelter map's room table starts it in state 0.
void shelterR37RoomTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_R37_H
