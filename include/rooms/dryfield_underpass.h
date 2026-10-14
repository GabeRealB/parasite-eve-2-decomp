#ifndef INCLUDE_ROOMS_DRYFIELD_UNDERPASS_H
#define INCLUDE_ROOMS_DRYFIELD_UNDERPASS_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Extents of the underpass's room directories and collision-surface directory.
enum {
    DRYFIELD_UNDERPASS_ROOM_COUNT          = 6,
    DRYFIELD_UNDERPASS_SURFACE_CLASS_COUNT = 8,
};

/// Number of camera/image/sprite resource slots selected by the room view maps.
enum { DRYFIELD_UNDERPASS_MAPPED_VIEW_COUNT = 26 };

/// Stored extents of the underpass's placement-variant and warp directories.
enum {
    DRYFIELD_UNDERPASS_VARIANT_SLOT_COUNT = 13,
    DRYFIELD_UNDERPASS_WARP_COUNT         = 3,
};

/// Placement layouts indexed directly by the underpass's saved variant byte.
///
/// Slot 1 pairs the Gray Stalker placements with their actor resource table;
/// slots 0 and 2..12 are absent and contain two NULL pointers. The thirteen
/// slots have no terminator. Selectors must be in 0..12; spawning requires
/// occupied slot 1. Gameplay borrows both tables while this room is loaded.
extern AreaVariant gDryfieldUnderpassAreaVariants[DRYFIELD_UNDERPASS_VARIANT_SLOT_COUNT];

/// Collision resources for underpass rooms 1..6, indexed by room minus one.
///
/// All rooms borrow one grid, six action triggers and three sight occluders.
/// Rooms 1..4 use sixteen post-event view boundaries; rooms 5..6 use two
/// pre-event boundaries. The directory has no terminator; each trigger and
/// occluder array includes its LAST-marked final record. Gameplay binds and
/// mutates those resources, which must stay loaded until their lists are cleared.
extern WorldCollisionRoomResources gDryfieldUnderpassRoomCollisionResources[DRYFIELD_UNDERPASS_ROOM_COUNT];

/// Logical-view maps for underpass rooms 1..6, indexed by room minus one.
///
/// Each byte at view minus one selects a 1-based camera, image and sprite view.
/// Room 1 borrows the gameplay identity map for 26 views; rooms 2..6 each use
/// eleven entries of a twelve-byte room-owned map. The retained final zero is
/// outside those logical-view ranges, not a terminator. Borrowed room maps
/// remain valid only while the underpass overlay is loaded.
extern u8* gDryfieldUnderpassRoomViewMaps[DRYFIELD_UNDERPASS_ROOM_COUNT];

/// Logical-view search counts for rooms 1..6, indexed by room minus one.
///
/// Room 1 searches 26 identity-map entries; rooms 2..6 search eleven entries
/// and exclude their maps' retained final zero. These signed-halfword counts
/// measure logical views, independently of mapped resource indices and map
/// capacities. The count array has no terminator and is borrowed only while
/// the underpass overlay is loaded.
extern ViewCount gDryfieldUnderpassRoomViewCounts[DRYFIELD_UNDERPASS_ROOM_COUNT];

/// Light collections for underpass rooms 1..6, indexed by room minus one.
///
/// Rooms 2 and 4 use the alternate six-point collection; the other rooms use
/// nine points. All entries use the default ambient minima. Gameplay borrows
/// the records and mutates light transforms and attenuation while this overlay
/// is loaded; no pointers may survive unloading it.
extern WorldCoordRoomLighting gDryfieldUnderpassRoomLighting[DRYFIELD_UNDERPASS_ROOM_COUNT];

/// Arrival and departure endpoints for underpass warp IDs 1..3, indexed by ID minus one.
///
/// Endpoints 1, 2 and 3 connect to the cellar, water hole and general store,
/// selecting initial views 6, 2 and 7. Player and companion share each endpoint's
/// world position and yaw (4096 units per turn). Endpoints 1 and 2 play distinct
/// arrival/departure sounds and update map flags 463 and 462; endpoint 3 uses
/// one sound in both directions and requests a departure fade. Blocked sounds
/// are absent. There is no terminator; gameplay borrows the three records while
/// this room overlay is loaded and copies each selected record by value.
extern DirectionWarpEntry gDryfieldUnderpassWarpEntries[DRYFIELD_UNDERPASS_WARP_COUNT];

/// Cameras for mapped underpass resource views 1..26, indexed by view minus one.
///
/// All six rooms select from this array through their logical-view maps.
/// Rotation uses `ONE` for unit scale, translation is the negated camera
/// origin in world units, and projection distances are pixels. There is no
/// terminator; queued camera application requires the overlay to stay loaded.
extern ViewCamera gDryfieldUnderpassViewCameras[DRYFIELD_UNDERPASS_MAPPED_VIEW_COUNT];

/// Sprite resources for mapped underpass views 1..26, indexed by view minus one.
///
/// Views 3, 4, 5, 9, 13, 14, 15 and 19 select textured source rectangles;
/// the other views retain an empty batch-list address in `sources.empty`.
/// Every list's first zero count keeps decoded-image background strips, and
/// `SPRITE_BATCH_END` ends each list. All draw-area pointers are NULL.
/// This array and its mutable source/range lists are borrowed while the
/// underpass overlay is loaded; none of their pointers may survive unloading.
extern SpriteView gDryfieldUnderpassSpriteViews[DRYFIELD_UNDERPASS_MAPPED_VIEW_COUNT];

/// Underpass collision and footstep properties indexed by surface class 0..7.
///
/// Every class blocks probes, accepts weapon impacts and applies pushback.
/// Classes 3 and 4 supply distinct footstep cue sets; the other classes are
/// silent. Class 2 retains its own record despite matching the default values
/// used by classes 0, 1 and 5..7. The loaded room overlay owns all records.
extern WorldCollisionSurfaceProperties* gDryfieldUnderpassSurfacePropertiesByClass[DRYFIELD_UNDERPASS_SURFACE_CLASS_COUNT];

/// Draws the underpass's local-space flares for the current view.
///
/// Called each frame while this room overlay is loaded. `task` must have a live
/// coordinate body with its composed transform current. Borrows that coordinate
/// without changing the task or retaining pointers. Nonzero `GAME_FLAG_053`
/// suppresses drawing; otherwise each of eight fixed points is selected by bit
/// `location.view` of its view mask. Room view IDs are 1..26; views 2..7, 9
/// and 11 select points.
/// Each selected point uses flare texture column zero and a pixel half-extent of
/// `640 * 39 / depth`, where depth is camera Z / 4 and must be at least 17 to draw.
/// Requires current view matrices, scratch stack and the frame's GPU packet arena
/// and ordering table. Reserves one quad per selected point even if depth rejects
/// it; queued packets borrow the arena until GPU completion.
void dryfieldUnderpassDrawFlaresTask(Task* task);

/// Runs the underpass room's entry, idle or teardown state.
///
/// The stage map spawns this receiver for area 38. `task->state` must be 0..2:
/// entry installs the message table and prepares the event, idle keeps the
/// receiver alive, and state 2 releases the task. Keep this room overlay loaded
/// while it can receive messages. No additional work or spawn payload is read.
void dryfieldUnderpassRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_UNDERPASS_H
