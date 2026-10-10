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

extern ViewCamera D_dryfield_cellar_8017DF78[];

extern SpriteView D_dryfield_cellar_8017FE40[];

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
