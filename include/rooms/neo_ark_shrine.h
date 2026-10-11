#ifndef INCLUDE_ROOMS_NEO_ARK_SHRINE_H
#define INCLUDE_ROOMS_NEO_ARK_SHRINE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of retained placement-variant slots, including absent slots.
enum { NEO_ARK_SHRINE_AREA_VARIANT_COUNT = 13 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
///
/// Slots 1, 2, 3, 4, 5, 7, 11 are present; every other slot is absent (NULL, NULL).
/// Supply an index in 0..12; this directory has no terminator or bounds check.
/// The placement selector is independent of room and view selectors.
/// Placements belong to the stage map, recipes to this room. Keep both overlays
/// loaded while loading or using either table. Referenced actor overlays must
/// be loaded for spawning and model-policy updates.
extern AreaVariant gNeoArkShrineAreaVariants[NEO_ARK_SHRINE_AREA_VARIANT_COUNT];

/// Number of room selectors and warp endpoints exported by the shrine.
enum {
    NEO_ARK_SHRINE_ROOM_COUNT = 6,
    NEO_ARK_SHRINE_WARP_COUNT = 3,
};

/// Collision bindings for the shrine's six room selectors, indexed by room - 1.
///
/// Rooms 1/4 use the base layout, 2/5 the puzzle's active layout, and 3/6
/// the enemy-release layout. Each pair shares its mutable grid and action
/// triggers; all rooms share the view boundaries and sight occluders.
/// Setup borrows and links the resources, which require this overlay loaded
/// until the collision lists are cleared.
extern WorldCollisionRoomResources gNeoArkShrineCollisionRooms[NEO_ARK_SHRINE_ROOM_COUNT];

/// Lighting bindings for the shrine's six room selectors.
///
/// Indexed by room - 1 (rooms 1..6). Every entry borrows the same mutable
/// light collection and uses the default ambient minima. The room overlay
/// must remain loaded while its records and lights are in use.
extern WorldCoordRoomLighting gNeoArkShrineRoomLighting[6];

/// Logical-view maps for the shrine's six room selectors.
///
/// Indexed by room - 1 (rooms 1..6), then logical view - 1 (views 1..18).
/// Entries are 1-based camera/image/sprite indices in 1..18, without a
/// terminator. Rooms 1 and 3 borrow `gViewIdentityMap`; rooms 4 and 6 share
/// a remap. The private remaps retain two trailing zero bytes outside the
/// logical-view domain. Borrowed room storage requires this overlay loaded.
extern u8* gNeoArkShrineViewMaps[6];

/// Logical-view counts for room selectors 1..6, indexed by room - 1.
///
/// Every room exposes eighteen logical views through `gNeoArkShrineViewMaps`.
/// Counts are signed halfwords; the map overlay borrows this room's storage.
extern ViewCount gNeoArkShrineViewCounts[NEO_ARK_SHRINE_ROOM_COUNT];

/// Actor placements, sounds and initial views for shrine warp endpoints 1..3.
///
/// Indexed by endpoint - 1, with no terminator. Endpoints 1 and 2 serve the
/// Power Plant 1 and Savanna Zone doors; endpoint 3 places both actors near
/// the puzzle and has no sound or map-marker effect. Placements use world
/// coordinates and yaw uses 4096 units per turn. Both actors share each
/// endpoint's placement. These borrowed records require the shrine loaded.
extern DirectionWarpEntry gNeoArkShrineWarpEntries[NEO_ARK_SHRINE_WARP_COUNT];

/// Number of camera and sprite-resource entries addressed by the shrine's mapped views.
enum { NEO_ARK_SHRINE_MAPPED_VIEW_COUNT = 18 };

/// Cameras for the shrine's eighteen mapped views, indexed by mapped view - 1.
///
/// Shared by all six room selectors. Rotation uses ONE for unit length,
/// translation is the negated world-space camera origin, and projection
/// distance is in pixels. Entries 16..18 repeat cameras 4, 10 and 5 for
/// alternate backgrounds. Borrowed records require this room overlay loaded.
extern ViewCamera gNeoArkShrineViewCameras[NEO_ARK_SHRINE_MAPPED_VIEW_COUNT];

/// Sprite resources for the shrine's eighteen mapped views, indexed by mapped view - 1.
///
/// Shared by all six room selectors; logical views select these through
/// `gNeoArkShrineViewMaps`. Every batch list ends with `SPRITE_BATCH_END`,
/// starts with decoded-background selection and indexes its own source
/// elements. Draw-area pointers are NULL. These mutable records and their
/// private source/batch arrays remain borrowed while the room overlay is loaded.
extern SpriteView gNeoArkShrineSpriteViews[NEO_ARK_SHRINE_MAPPED_VIEW_COUNT];

/// Collision policies and footstep cues for the shrine's eight surface classes.
///
/// Indexed directly by class 0..7, independently of the room selector.
/// All classes block probes, allow weapon impacts and apply pushback. Class 1
/// supplies no footstep cues; the others share the room's walking, running
/// and scripted-jump cue bases. The overlay must stay loaded while borrowed.
extern WorldCollisionSurfaceProperties* gNeoArkShrineSurfaceProperties[8];

/// Draws the shrine's fixed light flares selected by the current mapped view.
///
/// The first tick also selects this room's flash, twin-trail and spark-burst
/// effect IDs. Subsequent ticks draw the same view-selected world positions.
/// Requires the view, packet arena, ordering table and scratch stack ready
/// for drawing. The task remains active until external teardown.
void neoArkShrineFlareTask(Task* task);

/// Runs the shrine's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` is a positive charge duration in
/// active ticks, consumed as a countdown. Nonzero room effect control pauses
/// the task; control 4 or above, state 3, or completion releases the effect.
void neoArkShrineRoomVisualEffectsFlashTask(Task* task);

/// Records two parent-relative endpoints and draws their fading twin trails.
///
/// Requires a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`, whose parent stays live until teardown. `Task::work`
/// starts null and owns one allocation holding two eight-coordinate histories. The signed
/// 16-bit age advances only with room effect control below 2, including the
/// initialization tick. Allocation failure resets age to zero for retry.
/// `spawnArg1.value` is the completion age (2..32767); zero prevents automatic
/// completion. Teardown releases both the history and effect allocations.
void neoArkShrineRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the shrine's impact flash with smoke or orange rings and bouncing sparks.
///
/// Requires state 0, a coordinate body and a counted, owned, zero-aged `EffectWork` in
/// `spawnArg2.pointer`; its parent coordinate must stay live until teardown.
/// Nonzero `spawnArg1.value` selects smoke puffs, zero selects two independent
/// sparks followed by fading rings. Active age seven enters release; the next
/// active tick frees the work and task. Nonzero room effect control below four
/// pauses the task; four or above cancels it. Child effects run independently.
/// The gameplay bank-6 descriptor selects this room's loaded implementation.
void neoArkShrineRoomVisualEffectsSparkBurstTask(Task* task);

/// Initializes the shrine's room messages and puzzle, then waits for room messages.
///
/// The map's room descriptor supplies a bodyless task starting in state 0.
/// State 0 registers it in `GAME_TASK_SLOT_ROOM` and resets the board; state 1
/// idles with the message table installed, and state 2 kills the task. The
/// dispatcher requires `Task::state` in 0..2 and does not check that bound.
void neoArkShrineRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SHRINE_H
