#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_SALOON_G_R_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_SALOON_G_R_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Room-directory extent and logical-view extent of the night-saloon resources.
///
/// Both rooms map logical views 1..13 to the same camera and sprite indices.
enum {
    DRYFIELD_NIGHT_SALOON_G_R_ROOM_COUNT = 2,
    DRYFIELD_NIGHT_SALOON_G_R_VIEW_COUNT = 13,
};

/// Number of indexed placement-variant slots retained by the night saloon.
enum { DRYFIELD_NIGHT_SALOON_G_R_VARIANT_COUNT = 13 };

/// Placement/resource pairs for the night saloon, indexed by the saved variant.
///
/// Indices 0..12 have no terminator; only 1..3 are present: Kyle, the Blood
/// Suckler encounter, then Scavenger/Blood Sucklers. Slots 0 and 4..12 hold
/// two NULL pointers and cannot supply a loadable layout. Gameplay's night
/// area directory borrows this table while the room overlay is loaded. The
/// placements independently borrow the loaded Dryfield map overlay, and the
/// room-owned recipes borrow their actor overlays during loading/spawning.
extern AreaVariant gDryfieldNightSaloonGRAreaVariants[DRYFIELD_NIGHT_SALOON_G_R_VARIANT_COUNT];

/// Lighting for night-saloon rooms 1 and 2, sharing one mutable point-light collection.
///
/// Indexed by room - 1. Both entries use the default ambient minima; the map
/// directory borrows this array only while the saloon overlay is loaded.
extern WorldCoordRoomLighting gDryfieldNightSaloonGRRoomLighting[2];

/// Collision resources for night-saloon rooms 1 and 2.
///
/// Indexed by room - 1, without a terminator. Both entries borrow the same
/// grid, view boundaries, action triggers and two sight-blocking quads. The
/// map directory borrows this array while the saloon overlay is loaded; its
/// mutable trigger and occluder records must remain live until unlinked.
extern WorldCollisionRoomResources gDryfieldNightSaloonGRRoomCollisionResources[DRYFIELD_NIGHT_SALOON_G_R_ROOM_COUNT];

/// Logical-view maps for night-saloon rooms 1 and 2.
///
/// Indexed by room - 1. Both borrow gameplay's identity mapping for views 1..13;
/// each byte maps view - 1 to a 1-based camera/image/sprite index. The directory
/// lives with the saloon overlay; the mapping lives with gameplay.
extern u8* gDryfieldNightSaloonGRViewMaps[2];

/// Signed logical-view element counts for night-saloon rooms 1 and 2.
///
/// Indexed by room - 1, with no terminator. Each count limits reverse searches
/// of that room's identity map to views 1..13. The map directory borrows this
/// array while the saloon overlay is loaded; counts are elements, not bytes.
extern ViewCount gDryfieldNightSaloonGRViewCounts[DRYFIELD_NIGHT_SALOON_G_R_ROOM_COUNT];

/// Number of warp endpoints in the night saloon.
enum { DRYFIELD_NIGHT_SALOON_G_R_WARP_COUNT = 2 };

/// Arrival placements and door policies for night-saloon warp IDs 1 and 2.
///
/// Indexed by warp - 1, without a terminator. Each endpoint places player and
/// companion together in world units, with yaw in 4096 units per turn. Warp 1
/// selects view 2 and marker 479; warp 2 selects view 7 and marker 478. Arrival
/// and departure use distinct room sound cues; neither endpoint has a blocked
/// sound or spawn/departure flags. The map directory borrows the array while
/// this overlay is loaded; transition and room-start code copy complete entries.
extern DirectionWarpEntry gDryfieldNightSaloonGRWarpEntries[DRYFIELD_NIGHT_SALOON_G_R_WARP_COUNT];

/// Cameras shared by night-saloon rooms 1 and 2 for mapped views 1..13.
///
/// Indexed by mapped view - 1, without a terminator. Rotation uses Q12 (`ONE`
/// for 1.0); translation is the negated camera origin in world units, applied
/// before rotation. Projection distance is in pixels. The map directory and
/// queued camera tasks borrow these records while the saloon overlay is loaded.
extern ViewCamera gDryfieldNightSaloonGRViewCameras[DRYFIELD_NIGHT_SALOON_G_R_VIEW_COUNT];

/// Sprite sources and batch lists shared by both night-saloon rooms for mapped views 1..13.
///
/// Indexed by mapped view - 1, without a descriptor terminator. Views 1, 8, 12
/// and 13 retain an empty batch-list address in `sources.empty`; the others
/// borrow mutable `sources.elements`. All batch lists end at `SPRITE_BATCH_END`
/// and begin with a zero-count decoded-image selector. All `drawAreas` are NULL.
/// The map directory borrows this table and its lists while the room overlay
/// is loaded; every nonterminal range fits its view's source array.
extern SpriteView gDryfieldNightSaloonGRSpriteViews[DRYFIELD_NIGHT_SALOON_G_R_VIEW_COUNT];

/// Night-saloon collision and footstep policies indexed by surface class 0..7.
///
/// Class 1 has its own footsteps; classes 2 and 3 share a pair. Class 3 passes
/// probes and disables weapon impacts. All classes apply pushback; classes 0
/// and 4..7 have no surface footstep cues. Records are borrowed while this
/// overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightSaloonGRSurfacePropertiesByClass[8];

/// Draws the night saloon's view-gated flares and light beams each frame.
///
/// Bank 6 type 0x10E requires a live single-coordinate task body. Composes its
/// transform for the local-space beams; the flare positions are world-space.
/// The loaded area's current logical view is 1..13. Requires the active view
/// matrices, scratch stack and frame packet arena; emitted packets live until
/// GPU completion. Does not advance task state or allocate persistent storage.
void dryfieldNightSaloonGRDrawGlowsTask(Task* task);

/// Runs the night saloon's room initialization, message wait and teardown.
///
/// State 0 installs the room handlers and hides the pending encounter
/// actors on variant 2; state 1 waits for messages; state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldNightSaloonGRRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_SALOON_G_R_H
