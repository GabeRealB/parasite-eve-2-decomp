#ifndef INCLUDE_ROOMS_DRYFIELD_CELLAR_H
#define INCLUDE_ROOMS_DRYFIELD_CELLAR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_cellar_80180ACC[12];

// dryfield_cellar
extern WorldCollisionRoomResources D_dryfield_cellar_8017DBDC[];

/// Logical-view maps for cellar rooms 1 and 2, indexed by room minus one.
///
/// Room 1 borrows the gameplay identity map for 11 views; room 2 maps its six
/// views to camera/image/sprite slots 1, 7, 8, 9, 10 and 11. Entries are
/// unsigned 1-based indices, with no terminator. Keep gameplay and the room
/// overlay loaded while borrowing these maps.
extern u8* gDryfieldCellarRoomViewMaps[2];

extern ViewCount D_dryfield_cellar_8017DC0C[];

/// Lighting for cellar rooms 1 and 2, indexed by room minus one.
///
/// Each room borrows five point lights contributing in every view: warm in
/// room 1 and dim blue in room 2. Both use the default ambient minima.
/// Light arrays remain writable and valid only while the room overlay is loaded.
extern WorldCoordRoomLighting gDryfieldCellarRoomLighting[2];

extern DirectionWarpEntry D_dryfield_cellar_8017DC20[];

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
