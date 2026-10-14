#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_UNDERPASS_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_UNDERPASS_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of retained placement-variant slots, including absent slots.
enum { DRYFIELD_NIGHT_UNDERPASS_AREA_VARIANT_COUNT = 12 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
///
/// Slots 1, 2, 3, 7, 11 are present; every other slot is absent (NULL, NULL).
/// Supply an index in 0..11; this directory has no terminator or bounds check.
/// The placement selector is independent of room and view selectors.
/// Placements belong to the stage map, recipes to this room. Keep both overlays
/// loaded while loading or using either table. Referenced actor overlays must
/// be loaded for spawning and model-policy updates.
extern AreaVariant gDryfieldNightUnderpassAreaVariants[DRYFIELD_NIGHT_UNDERPASS_AREA_VARIANT_COUNT];

/// Collision resources for room selectors 1..6, indexed by room minus one.
///
/// All six room slots borrow the same grid, sixteen view boundaries, six action triggers
/// and three sight occluders.
///
/// The stage map borrows this directory while the room overlay is loaded. Keep the grid
/// and its pools live while active, and the mutable trigger/occluder lists live until
/// unlinked. This directory has no terminator or bounds check.
extern WorldCollisionRoomResources gDryfieldNightUnderpassRoomCollisionResources[6];

/// Model lighting for nighttime underpass room IDs 1..6.
///
/// Rooms 1, 3, 5 and 6 share nine point lights, including eight ceiling lamps;
/// rooms 2 and 4 share six dimmer point lights. All use the default ambient
/// minima. Keep this overlay loaded while its mutable lights are in use.
extern WorldCoordRoomLighting gDryfieldNightUnderpassRoomLighting[6];

/// Logical-view maps for nighttime underpass room IDs 1..6.
///
/// Indexed by room minus one, then logical view minus one. Room 1 borrows
/// `gViewIdentityMap` for views 1..26; rooms 2..6 each map eleven logical views
/// to camera/image/sprite indices 1..26. Their stored twelfth zero byte is
/// outside the live range, not a terminator. Keep the room overlay loaded.
extern u8* gDryfieldNightUnderpassViewMaps[6];

/// Nighttime underpass logical-view search counts for room selectors 1..6.
///
/// Room 1 searches 26 identity-map entries; rooms 2..6 each search eleven
/// entries in their distinct maps, excluding each map's retained twelfth zero.
/// Indexed by room - 1; signed halfword values count logical map elements,
/// independently of map capacity and mapped resource extents. No terminator.
/// Borrow the array only while this room overlay is loaded.
extern ViewCount gDryfieldNightUnderpassViewCounts[6];

/// Warp endpoint records for slots 1..3, indexed by warp minus one.
///
/// Slots 1..3 serve the Cellar, Water Hole and General Store connections, with arrival
/// views 6, 2 and 7. Slots 1/2 mark their doors; slot 3 fades on permitted departure and
/// uses the same arrival/departure sound. None has a blocked sound.
///
/// Positions use signed whole world coordinates; yaw uses 4096 units per turn. The two
/// byte spans have unproven roles and remain zero. The stage map borrows this array;
/// keep the room overlay loaded through departure or arrival setup. Consumers copy
/// complete entries and actor spawns borrow their transforms synchronously. There is
/// no terminator or bounds check.
extern DirectionWarpEntry gDryfieldNightUnderpassWarpEntries[3];

/// Number of mapped camera/image/sprite slots in the nighttime underpass.
enum { DRYFIELD_NIGHT_UNDERPASS_MAPPED_VIEW_COUNT = 26 };

/// Cameras for nighttime underpass mapped view indices 1..26.
///
/// Index by `viewGetMappedIndex() - 1`, after the room's logical-view mapping.
/// Rotations use Q12, translations are negated world-space camera origins,
/// and projection distances are pixels. The table has no terminator;
/// keep this overlay loaded until a queued camera application has finished.
extern ViewCamera gDryfieldNightUnderpassViewCameras[DRYFIELD_NIGHT_UNDERPASS_MAPPED_VIEW_COUNT];

/// Sprite descriptors for nighttime underpass mapped view indices 1..26.
///
/// Index by `viewGetMappedIndex() - 1`. Each batch list keeps decoded background
/// strips and ends in SPRITE_BATCH_END; views 3, 4, 5, 9, 13, 14, 15 and 19
/// additionally select source sprites. All draw-area pointers are NULL.
/// Descriptors and their mutable source/batch arrays borrow this overlay's
/// lifetime and must stay loaded during packet allocation and drawing.
extern SpriteView gDryfieldNightUnderpassSpriteViews[DRYFIELD_NIGHT_UNDERPASS_MAPPED_VIEW_COUNT];

/// Nighttime underpass collision policies indexed directly by surface class 0..7.
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// Classes 2 and 4 share footstep cue bases; class 3 has its own set;
/// classes 0, 1 and 5..7 have no footstep table. The records belong to this
/// overlay and remain valid only while it is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightUnderpassSurfacePropertiesByClass[8];

/// Draws the nighttime underpass's world-space flares for the current camera view.
///
/// Called each frame while this room overlay is loaded. `unusedTask` supplies no
/// position or state. Nonzero `GAME_FLAG_053` suppresses drawing; otherwise each
/// of eight fixed points is selected by bit `location.view` of its view mask.
/// The room's view IDs are 1..26, and only views 2..11 select any points.
/// Each selected point uses flare texture column zero and a pixel half-extent of
/// `640 * 39 / depth`, where depth is camera Z / 4 and must be nonzero for an
/// accepted projection. Requires the current view matrices, scratch stack and
/// frame's GPU packet arena and ordering table; queued packets live for the frame.
void dryfieldNightUnderpassDrawFlaresTask(Task* unusedTask);

/// Runs the night underpass's room initialization, message wait and teardown.
///
/// State 0 installs the room handlers; state 1 waits for messages;
/// state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldNightUnderpassRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_UNDERPASS_H
