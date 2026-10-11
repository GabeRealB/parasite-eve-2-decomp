#ifndef INCLUDE_ROOMS_NEO_ARK_SUBSTATION_H
#define INCLUDE_ROOMS_NEO_ARK_SUBSTATION_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// neo_ark_substation
/// Collision resources for the substation's sole room, indexed by room ID minus one.
///
/// Borrows its mutable grid, view-boundary and interaction triggers; no
/// occluder list is stored. The stage map requires this overlay to remain
/// loaded while the resources are linked. There is no terminator.
extern WorldCollisionRoomResources gNeoArkSubstationRoomCollisionResources[1];

/// Lighting for the substation's sole room, indexed by room ID minus one.
///
/// Borrows 17 mutable point lights in every view; no directional or cone lights.
/// A NULL ambient table selects the gameplay default ambient minima.
/// The record and its lights remain valid only while this overlay is loaded.
extern WorldCoordRoomLighting gNeoArkSubstationRoomLighting[1];

/// Logical-view map directory for the substation's sole room.
///
/// Entry zero borrows `gViewIdentityMap`: logical views 1..8 select the same
/// camera, image and sprite indices. The shared map has a larger capacity;
/// this room's count and resource arrays limit valid views to 1..8.
/// The directory remains valid only while this overlay is loaded.
extern u8* gNeoArkSubstationViewMaps[1];

/// Logical-view reverse-search limit for this area's sole room.
///
/// Room 1 searches 8 entries in its view map. Index zero corresponds to room ID 1.
/// Counts are signed halfword element counts, independent of map capacity.
/// The stage map borrows this array while the room overlay is loaded; there
/// is no terminator.
extern ViewCount gNeoArkSubstationRoomViewCounts[1];

/// The substation's two warp endpoints, indexed by warp ID minus one.
///
/// Player and companion arrive at the same world placement, at views 2 and 3.
/// Endpoint 1 supplies area-bank arrival/departure sounds and marks the altar
/// on arrival; endpoint 2 is silent. The stage map borrows both complete
/// records while this overlay is loaded; there is no terminator.
extern DirectionWarpEntry gNeoArkSubstationWarpEntries[2];

/// Authored cameras for mapped view indices 1..8, stored at index minus one.
///
/// Rotation coefficients use `ONE` (4096) per unit; translation is the negated
/// world-space camera origin in game units. Projection distance is in pixels.
/// The stage map borrows this array; keep the room overlay loaded until queued
/// camera tasks have applied their records. There is no terminator.
extern ViewCamera gNeoArkSubstationViewCameras[8];

/// Sprite and clipping resources for the substation's eight mapped views.
///
/// Index by mapped view minus one. Views 2..4 supply 45, 75 and 19 sprites;
/// all first batches have zero count, preserving the decoded background strips.
/// Views 5 and 6 clip scene drawing despite having no source sprites.
/// Records and borrowed lists remain valid only while this overlay is loaded.
extern SpriteView gNeoArkSubstationSpriteViews[8];

/// Collision and footstep policies indexed by the substation's surface class 0..7.
///
/// Classes 0, 1 and 5..7 share a blocking record without footstep cues.
/// Class 2 supplies stair cues, class 3 passes probes, and class 4 blocks them.
/// Every class allows weapon impacts and applies pushback. Grid faces use
/// classes 0, 3 and 4; the ascending stair trigger selects class 2.
/// The directory and borrowed records require this overlay to remain loaded.
extern WorldCollisionSurfaceProperties* gNeoArkSubstationSurfaceProperties[8];

/// Draws the substation's flickering light glows for the current mapped view.
///
/// Bank-6 effect 0x38 draws fixed world-space endpoint pairs in views 2..8;
/// other views emit nothing. The task argument is unused and no state is retained.
/// Requires the substation overlay, composed view matrices, initialized scratch
/// stack and enough space in the current frame's packet arena and ordering table.
void neoArkSubstationDrawLightGlowsTask(Task* unusedTask);

/// Runs the Neo Ark Substation room controller for one tick.
///
/// `task` must be live with state 0..2 and this room overlay loaded.
/// Initialize room messages and optional ambience, remain idle, then kill.
/// State 0 registers the borrowed task in `GAME_TASK_SLOT_ROOM`; state 2
/// requests teardown. The controller allocates no work or body of its own.
void neoArkSubstationRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SUBSTATION_H
