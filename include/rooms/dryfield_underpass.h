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

extern AreaVariant D_dryfield_underpass_8017F868[13];

// dryfield_underpass
extern WorldCollisionRoomResources D_dryfield_underpass_8017EB20[];

/// Logical-view maps for underpass rooms 1..6, indexed by room minus one.
///
/// Each byte at view minus one selects a 1-based camera, image and sprite view.
/// Room 1 borrows the gameplay identity map for 26 views; rooms 2..6 each use
/// eleven entries of a twelve-byte room-owned map. The retained final zero is
/// outside those logical-view ranges, not a terminator. Borrowed room maps
/// remain valid only while the underpass overlay is loaded.
extern u8* gDryfieldUnderpassRoomViewMaps[DRYFIELD_UNDERPASS_ROOM_COUNT];

extern ViewCount D_dryfield_underpass_8017EBD4[];

/// Light collections for underpass rooms 1..6, indexed by room minus one.
///
/// Rooms 2 and 4 use the alternate six-point collection; the other rooms use
/// nine points. All entries use the default ambient minima. Gameplay borrows
/// the records and mutates light transforms and attenuation while this overlay
/// is loaded; no pointers may survive unloading it.
extern WorldCoordRoomLighting gDryfieldUnderpassRoomLighting[DRYFIELD_UNDERPASS_ROOM_COUNT];

extern DirectionWarpEntry D_dryfield_underpass_8017EC10[];

extern ViewCamera D_dryfield_underpass_8017F4A8[];

extern SpriteView D_dryfield_underpass_80180250[];

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
/// `location.loc.view` of its view mask. Room view IDs are 1..26; views 2..7, 9
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
