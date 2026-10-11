#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_CELLAR_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_CELLAR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Dryfield night cellar placement/resource directory indexed by `GameLocationKey.variant` 0..11.
///
/// Slots 1, 3, 11 publish paired tables; all other slots are absent (NULL, NULL).
/// This placement selector is independent of the room and logical-view selectors.
/// Placements belong to the stage map; resource recipes belong to this room.
/// Keep both overlays and the actors supplying task descriptors loaded during
/// loading, spawning and model-flag updates. Lookups require an index within
/// this array; there is no terminator or bounds check.
extern AreaVariant gDryfieldNightCellarAreaVariants[12];

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

/// Nighttime cellar logical-view search counts for rooms 1 and 2.
///
/// Room 1 searches eleven identity-map entries; room 2 searches six entries
/// selecting mapped views 1 and 7..11, excluding its two retained zero bytes.
/// Indexed by room - 1; signed halfword values count logical map elements,
/// independently of map capacity and mapped resource extents. No terminator.
/// Borrow the array only while this room overlay is loaded.
extern ViewCount gDryfieldNightCellarViewCounts[2];

extern DirectionWarpEntry D_dryfield_night_cellar_8017DB34[];

/// Eleven cameras shared by the nighttime cellar's two room slots.
///
/// Indexed by mapped 1-based view minus one. Room 1 selects 1..11; room 2
/// selects 1 and 7..11. Rotation uses ONE per unit, translation is the negated
/// world-space origin and perspective distance is pixels. The camera directory
/// borrows this mutable array while the room overlay is loaded.
extern ViewCamera gDryfieldNightCellarViewCameras[11];

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
