#ifndef INCLUDE_ROOMS_NEO_ARK_SAVANNA_ZONE_H
#define INCLUDE_ROOMS_NEO_ARK_SAVANNA_ZONE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Extents of the savanna zone's mapped view and collision surface directories.
enum {
    NEO_ARK_SAVANNA_ZONE_VIEW_COUNT          = 4,
    NEO_ARK_SAVANNA_ZONE_SURFACE_CLASS_COUNT = 8,
};

/// Number of retained placement-variant slots, including absent slots.
enum { NEO_ARK_SAVANNA_ZONE_AREA_VARIANT_COUNT = 13 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
///
/// Slots 1, 2, 3, 4, 5, 11 are present; every other slot is absent (NULL, NULL).
/// Supply an index in 0..12; this directory has no terminator or bounds check.
/// The placement selector is independent of room and view selectors.
/// Placements belong to the stage map, recipes to this room. Keep both overlays
/// loaded while loading or using either table. Referenced actor overlays must
/// be loaded for spawning and model-policy updates.
extern AreaVariant gNeoArkSavannaZoneAreaVariants[NEO_ARK_SAVANNA_ZONE_AREA_VARIANT_COUNT];

// neo_ark_savanna_zone
extern WorldCollisionRoomResources D_neo_ark_savanna_zone_8017F9E4[];

/// Lighting for the savanna zone's single room, borrowed while its overlay is loaded.
///
/// Selects two directional lights and minimum ambient colours for views 1..4.
extern WorldCoordRoomLighting gNeoArkSavannaZoneRoomLighting[1];

/// The single room's logical-view map, borrowing gameplay's `gViewIdentityMap`.
///
/// Logical views 1..4 select the same 1-based camera, image and sprite index.
/// The shared map has a larger capacity; this room uses only its first four bytes.
extern u8* gNeoArkSavannaZoneViewMaps[1];

extern ViewCount D_neo_ark_savanna_zone_8017FA00[];

extern DirectionWarpEntry D_neo_ark_savanna_zone_8017FA04[];

/// Cameras selected by mapped view index 1..4, stored at index minus one.
///
/// Origins use world units, rotations use 4096 per unit and projection distance
/// is 329 pixels. Queued camera application borrows this loaded overlay's storage.
extern ViewCamera gNeoArkSavannaZoneViewCameras[NEO_ARK_SAVANNA_ZONE_VIEW_COUNT];

/// Sprite fragments and batches selected by mapped view index 1..4.
///
/// View 1 has no source sprites; views 2..4 cover 24, 35 and 29 source elements.
/// Every view retains image strips and has no clipping list. The descriptors
/// and their mutable source/batch arrays remain borrowed while this overlay is loaded.
extern SpriteView gNeoArkSavannaZoneSpriteViews[NEO_ARK_SAVANNA_ZONE_VIEW_COUNT];

/// Collision policies and footstep cues indexed by room-local surface class 0..7.
///
/// All classes block probes and apply pushback. Class 1 disables weapon impacts;
/// classes 2 and 3 supply distinct footstep cue tables. Classes 0 and 4..7 share
/// the solid surface without footsteps. Records remain borrowed while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gNeoArkSavannaZoneSurfaceProperties[NEO_ARK_SAVANNA_ZONE_SURFACE_CLASS_COUNT];

/// Installs the savanna zone's enemy effects and enables ambient effects once.
///
/// State 0 selects this overlay's flash, twin-trail and spark-burst effect IDs
/// and enables room effects, including footstep dust, then enters idle state 1.
/// Requires a live `gRoomEffectState`; later ticks leave the settings alone.
void neoArkSavannaZoneConfigureEffectsTask(Task* task);

/// Runs the savanna zone's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`, as `effectSpawn` supplies. `spawnArg1.value` is a positive
/// charge duration in active ticks, consumed as a countdown after initialization.
/// Nonzero room effect control pauses without drawing; control 4 or above
/// cancels it. While running, state 3 or fade completion releases it. Release
/// frees the work, body and task. The room overlay and effect controller must
/// remain live, with scratch and frame packet space.
void neoArkSavannaZoneRoomVisualEffectsFlashTask(Task* task);

/// Draws the savanna zone's fading beam between two offsets on a moving parent.
///
/// Requires a coordinate body, counted, owned `EffectWork` in `spawnArg2.pointer`,
/// initial state 0 and null `Task::work`. The borrowed parent coordinate, room
/// overlay and effect controller must remain live. Allocates eight world-space
/// snapshots per endpoint in `Task::work`; allocation failure resets age for retry.
/// `spawnArg1.value` is the release age in active ticks (2..32767); zero leaves
/// lifetime to external teardown. Initialization increments age without drawing
/// or checking expiry. Control values below 2 advance the trail; 2 and above
/// freeze it without drawing or cancellation. Teardown frees the histories,
/// effect work, body and task. Drawing needs scratch and frame packet space.
void neoArkSavannaZoneRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the savanna zone's impact flash with smoke or orange rings and bouncing sparks.
///
/// Requires state 0, a coordinate body and counted, owned, zero-aged `EffectWork`
/// in `spawnArg2.pointer`; its parent coordinate must stay live until teardown.
/// Nonzero `spawnArg1.value` selects smoke puffs, zero selects two independent
/// sparks followed by fading rings. Active age seven enters release; the next
/// active tick frees the work and task. Nonzero room effect control below four
/// pauses the task; four or above cancels it. Child effects run independently.
/// The gameplay bank-6 descriptor selects this room's loaded implementation.
void neoArkSavannaZoneRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs the Neo Ark Savanna Zone room controller for one tick.
///
/// `task` must be live with state 0..2 and this room overlay loaded.
/// Initialize room messages, remain idle between messages, then kill.
/// State 0 registers the borrowed task in `GAME_TASK_SLOT_ROOM`; state 2
/// requests teardown. The controller allocates no work or body of its own.
void neoArkSavannaZoneRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SAVANNA_ZONE_H
