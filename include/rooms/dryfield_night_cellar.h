#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_CELLAR_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_CELLAR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_cellar_80180780[12];

/// Model lighting for nighttime cellar rooms 1 and 2, indexed by room - 1.
///
/// Each room borrows its own five-point-light collection. Both use the default
/// ambient minima in every view. Descriptors and light arrays remain live only
/// while the nighttime cellar overlay is loaded; light updates mutate the arrays.
extern WorldCoordRoomLighting gDryfieldNightCellarRoomLighting[2];

extern WorldCollisionRoomResources D_dryfield_night_cellar_8017DB00[];

/// Logical-view maps for nighttime cellar rooms 1 and 2, indexed by room - 1.
///
/// Map entries are unsigned 1-based camera, image and sprite indices, indexed
/// by logical view - 1. Room 1 uses the shared identity map for logical views
/// 1..11; room 2 uses {1, 7, 8, 9, 10, 11} for logical views 1..6. There is no
/// terminator. The directory and room 2 map are borrowed from the loaded overlay.
extern u8* gDryfieldNightCellarViewMaps[2];

extern ViewCount D_dryfield_night_cellar_8017DB30[];

extern DirectionWarpEntry D_dryfield_night_cellar_8017DB34[];

extern ViewCamera D_dryfield_night_cellar_8017DE84[];

/// Dryfield night cellar's sprite resources, indexed by mapped view minus one.
///
/// Mapped views 1, 6 and 11 have no sources; all others borrow sprite pools.
/// The two logical-room maps select this same directory.
/// Every slot retains decoded image strips and has no clipping list. Empty
/// source slots retain batch addresses without supplying SpriteSource elements.
/// Descriptors and mutable source/batch lists stay borrowed while the owning
/// room overlay is loaded; selectors must be nonzero and within this array.
extern SpriteView gDryfieldNightCellarSpriteViews[11];

/// Collision and footstep policies for the nighttime cellar's surface classes 0..7.
///
/// Every class borrows the same blocking surface record: weapon impacts and
/// pushback are enabled, with paired walking/running footstep scripts. The
/// directory and referenced records stay valid only while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightCellarSurfacePropertiesByClass[8];

/// Draws the nighttime cellar's switch-gated pair of flickering flares each frame.
///
/// `GAME_FLAG_UNDERPASS_SWITCH_2` must equal 1; camera views 2 and 3 select
/// separate pairs of world points, and other views draw nothing. The callback
/// argument is unused. The current view transform, scratch stack, ordering
/// table and packet arena must be ready for drawing, with nonzero depth for
/// accepted points. Borrows the room's points for this call; queued flare
/// packets belong to the current frame and remain live until GPU completion.
void dryfieldNightCellarDrawGlowsTask(Task* unused);

/// Dispatches the nighttime cellar's room receiver lifecycle.
///
/// `state` must be 0 (register messages), 1 (idle), or 2 (kill). Start in state
/// 0; initialization publishes the task in `GAME_TASK_SLOT_ROOM` and enters
/// state 1. The room's borrowed message and state tables must remain loaded
/// through teardown. Dispatch copies the complete three-entry state table.
void dryfieldNightCellarRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_CELLAR_H
