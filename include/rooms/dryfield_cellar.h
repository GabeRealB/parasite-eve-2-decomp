#ifndef INCLUDE_ROOMS_DRYFIELD_CELLAR_H
#define INCLUDE_ROOMS_DRYFIELD_CELLAR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Dryfield cellar placement/resource directory indexed by `GameLocationKey.variant` 0..11.
///
/// Slot 1 publishes paired tables; all other slots are absent (NULL, NULL).
/// This placement selector is independent of the room and logical-view selectors.
/// Placements belong to the stage map; resource recipes belong to this room.
/// Keep both overlays and the actors supplying task descriptors loaded during
/// loading, spawning and model-flag updates. Lookups require an index within
/// this array; there is no terminator or bounds check.
extern AreaVariant gDryfieldCellarAreaVariants[12];

/// Collision resources for cellar room selectors 1 and 2, indexed by room minus one.
///
/// Both rooms borrow the same grid, six directed view boundaries and one sight
/// occluder. Each supplies its own ten action triggers. Setup mutates resource
/// links and trigger coordinates; keep the room overlay loaded until unlinked.
extern WorldCollisionRoomResources gDryfieldCellarRoomCollisionResources[2];

/// Logical-view maps for cellar rooms 1 and 2, indexed by room minus one.
///
/// Room 1 borrows the gameplay identity map for 11 views; room 2 maps its six
/// views to camera/image/sprite slots 1, 7, 8, 9, 10 and 11. Entries are
/// unsigned 1-based indices, with no terminator. Keep gameplay and the room
/// overlay loaded while borrowing these maps.
extern u8* gDryfieldCellarRoomViewMaps[2];

/// Logical-view reverse-search lengths for cellar rooms 1 and 2: 11 and 6.
///
/// Indexed by room minus one. These signed halfword element counts limit each
/// room's view-map search; they do not describe map capacity or mapped resource
/// extent. There is no terminator. Borrow while the room overlay is loaded.
extern ViewCount gDryfieldCellarRoomViewCounts[2];

/// Lighting for cellar rooms 1 and 2, indexed by room minus one.
///
/// Each room borrows five point lights contributing in every view: warm in
/// room 1 and dim blue in room 2. Both use the default ambient minima.
/// Light arrays remain writable and valid only while the room overlay is loaded.
extern WorldCoordRoomLighting gDryfieldCellarRoomLighting[2];

/// The cellar's underpass arrival/departure endpoint, indexed by warp ID minus one.
///
/// Only warp ID 1 exists. Both actors arrive at world (5611, 3, 3945), facing
/// yaw 2048 (4096 units per turn), with initial logical view 2. Arrival and
/// departure request cellar-bank sounds; a blocked departure is silent. The
/// endpoint updates the cellar/underpass map marker. There is no terminator;
/// borrow the record only while this room overlay is loaded.
extern DirectionWarpEntry gDryfieldCellarWarpEntries[1];

/// Eleven cameras shared by the cellar's two room slots.
///
/// Indexed by mapped 1-based view minus one. Room 1 selects 1..11; room 2
/// selects 1 and 7..11. Rotation uses ONE per unit, translation is the negated
/// world-space origin and perspective distance is pixels. The camera directory
/// borrows this mutable array while the room overlay is loaded.
extern ViewCamera gDryfieldCellarViewCameras[11];

/// Dryfield cellar's sprite resources, indexed by mapped view minus one.
///
/// Mapped views 1, 6 and 11 have no sources; all others borrow sprite pools.
/// The two logical-room maps select this same directory.
/// Every slot retains decoded image strips and has no clipping list. Empty
/// source slots retain batch addresses without supplying SpriteSource elements.
/// Descriptors and mutable source/batch lists stay borrowed while the owning
/// room overlay is loaded; selectors must be nonzero and within this array.
extern SpriteView gDryfieldCellarSpriteViews[11];

/// Cellar collision and footstep properties, indexed by surface class 0..7.
///
/// All classes borrow one blocking surface record with weapon impacts and
/// pushback enabled and the same footstep bases. Keep the room overlay loaded
/// while using this directory or its borrowed records; it has no terminator.
extern WorldCollisionSurfaceProperties* gDryfieldCellarSurfacePropertiesByClass[8];

/// Draws the cellar's switch-gated pair of flickering flares each frame.
///
/// `GAME_FLAG_UNDERPASS_SWITCH_2` must equal 1; camera views 2 and 3 select
/// separate pairs of local points, and other views draw nothing. `task` must
/// own a live `TASK_BODY_COORD` body with its cached transform refreshed.
/// The current view, scratch stack, ordering table and packet arena must be
/// ready for drawing. Borrows the coordinate and room's points for this call;
/// queued flare packets belong to the current frame.
void dryfieldCellarDrawGlowsTask(Task* task);

/// Runs the cellar's room receiver through initialization, idle and teardown.
///
/// Requires a live bodyless task with state 0 (register), 1 (idle) or 2 (kill).
/// Initialization registers `GAME_TASK_SLOT_ROOM` and permits post-CAP sound
/// messages. Keep the cellar overlay loaded for the task's lifetime.
void dryfieldCellarRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_CELLAR_H
