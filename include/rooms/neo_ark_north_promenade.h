#ifndef INCLUDE_ROOMS_NEO_ARK_NORTH_PROMENADE_H
#define INCLUDE_ROOMS_NEO_ARK_NORTH_PROMENADE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of retained placement-variant slots, including absent slots.
enum { NEO_ARK_NORTH_PROMENADE_AREA_VARIANT_COUNT = 13 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
///
/// Slots 2, 3, 11 are present; every other slot is absent (NULL, NULL).
/// Supply an index in 0..12; this directory has no terminator or bounds check.
/// The placement selector is independent of room and view selectors.
/// Placements belong to the stage map, recipes to this room. Keep both overlays
/// loaded while loading or using either table. Referenced actor overlays must
/// be loaded for spawning and model-policy updates.
extern AreaVariant gNeoArkNorthPromenadeAreaVariants[NEO_ARK_NORTH_PROMENADE_AREA_VARIANT_COUNT];

/// Extents of the promenade's mapped view resources and surface-class directory.
enum {
    NEO_ARK_NORTH_PROMENADE_VIEW_COUNT          = 6,
    NEO_ARK_NORTH_PROMENADE_SURFACE_CLASS_COUNT = 8,
};

// neo_ark_north_promenade
extern WorldCollisionRoomResources D_neo_ark_north_promenade_80181DB4[];

/// Lighting for room 1: two directional lights and the default ambient minima.
///
/// Indexed by room - 1. Borrows mutable lights from the loaded promenade overlay.
extern WorldCoordRoomLighting gNeoArkNorthPromenadeRoomLighting[1];

/// Logical-view map for room 1, mapping views 1..6 to the same resource indices.
///
/// Indexed by room - 1, then view - 1. Borrows gameplay's `gViewIdentityMap`;
/// only the first `NEO_ARK_NORTH_PROMENADE_VIEW_COUNT` entries belong to this room.
/// The pointer directory requires the promenade overlay to remain loaded.
extern u8* gNeoArkNorthPromenadeViewMaps[1];

/// Logical-view search count for room 1, indexed by room - 1.
///
/// The six identity-mapped views each select a camera, image and sprite descriptor.
/// Borrow this signed halfword array only while the promenade overlay is loaded.
extern ViewCount gNeoArkNorthPromenadeViewCounts[1];

extern DirectionWarpEntry D_neo_ark_north_promenade_80181DD4[];

/// Cameras for mapped views 1..6, indexed by mapped view - 1.
///
/// Rotations use `ONE` for 1.0, translations are negated world-space camera origins,
/// and perspective distances are pixels. Keep the overlay loaded until any queued
/// camera application finishes.
extern ViewCamera gNeoArkNorthPromenadeViewCameras[NEO_ARK_NORTH_PROMENADE_VIEW_COUNT];

/// Sprite descriptors for mapped views 1..6, indexed by mapped view - 1.
///
/// Views 1..3 have no source sprites; views 4..6 have 36, 23 and 32 sources.
/// All views retain image-strip backgrounds and use no clipping-area list.
/// Descriptors and mutable source/batch arrays require the overlay to stay loaded.
extern SpriteView gNeoArkNorthPromenadeSpriteViews[NEO_ARK_NORTH_PROMENADE_VIEW_COUNT];

/// Collision policies and footstep cues indexed by surface class 0..7.
///
/// Class 1 blocks probes and allows weapon impacts with footsteps; class 2 passes
/// probes and ignores weapon impacts with the same cues. Other classes block
/// probes and allow impacts without cues. All classes apply grid pushback.
/// The directory and its borrowed records require the promenade overlay loaded.
extern WorldCollisionSurfaceProperties* gNeoArkNorthPromenadeSurfaceProperties[NEO_ARK_NORTH_PROMENADE_SURFACE_CLASS_COUNT];

/// Runs the room's charging pink flash, peak screen tint and fading star.
///
/// Starts in state zero with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`. `spawnArg1.value` is a positive
/// charge duration in active ticks, consumed as a countdown. Nonzero room
/// effect control pauses it; values at least four cancel it. State three also
/// requests release. Completion frees the effect work and tears down the task.
/// Requires a live `gRoomEffectState` and the room overlay to remain loaded.
void neoArkNorthPromenadeRoomVisualEffectsFlashTask(Task* task);

/// Runs the room's fading beam between two moving endpoint histories.
///
/// Starts in state zero with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`. Borrows its parent coordinate while
/// live and owns two eight-coordinate histories in `Task::work`, freed during
/// teardown. Allocation failure retries with age reset to zero. Initialization
/// counts as the first active tick; later ticks record world-space endpoints
/// and draw seven quads with red:green:blue intensities in the ratio 1:2:3.
/// `spawnArg1.value` is zero for external teardown, or 2..32767 to expire at
/// that active age. Room effect control at two or above holds age and drawing
/// without cancelling. Requires a live `gRoomEffectState` and loaded room overlay.
void neoArkNorthPromenadeRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the promenade's impact flash followed by smoke or orange rings and sparks.
///
/// Starts in state 0 with a coordinate body and counted, zero-aged `EffectWork`
/// in `spawnArg2.pointer` from `effectSpawn`. Nonzero `spawnArg1.value` selects
/// smoke; zero selects rings and bouncing sparks. Active age 7 requests release,
/// and the next active tick frees work and task. Child effects run independently.
/// Nonzero room effect control pauses it; values at least 4 cancel it. Keep the
/// room overlay and scratch/frame packet storage live while drawing.
void neoArkNorthPromenadeRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs the room's animated mote, rising and brightening or drifting steadily before fading.
///
/// Starts in state zero with a coordinate body and zeroed, counted `EffectWork`
/// in `spawnArg2.pointer` from `effectSpawn`. `spawnArg1.value` packs half-extent
/// in bits 0..11 (world units), palette in bits 12..15 (zero selects the default),
/// speed in bits 16..23 (unsigned local-Y units per active tick), and lifetime
/// in bits 24..31 (signed active ticks). The overlapping low two extent bits
/// select steady motion when either is set; bit one makes it upward. Otherwise
/// it rises at the supplied speed plus a random 0..63. The first tick initializes
/// without drawing; later ticks draw on odd ages and start fading after age
/// exceeds lifetime minus eight. Nonzero room effect control pauses it;
/// values at least four cancel it.
/// Completion releases the work and task. Keep the room overlay loaded while live.
void neoArkNorthPromenadeRoomVisualEffectsMoteTask(Task* task);

/// Runs the room's expanding tinted halo and shrinking ring, then a fading star.
///
/// Starts in state zero with a coordinate body and zeroed, counted `EffectWork`
/// in `spawnArg2.pointer` from `effectSpawn`. Its borrowed parent coordinate must
/// remain live; the copied spawn offset is in that parent's local coordinate units.
/// The signed halves of `spawnArg1` supply a positive expansion duration in
/// active ticks (low) and tint-row index 0..2 (high). Initialization replaces
/// the packed word with a countdown. Nonzero room effect control pauses every
/// phase; values at least four cancel it. State three requests release.
/// Completion releases the work and task. Keep the room overlay loaded while live.
void neoArkNorthPromenadeRoomVisualEffectsHaloTask(Task* task);

/// Runs the room's growing orange disc and layered glow inside a fading ring.
///
/// Starts in state zero with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; `spawnArg1` is unused. Draws on the
/// first active tick and fades the ring before the central glow. Nonzero room
/// effect control pauses it; values at least four cancel it. Completion releases
/// the work and task. Keep the room overlay loaded while live.
void neoArkNorthPromenadeRoomVisualEffectsHaloOrangeBurstTask(Task* task);

/// Emits twenty motes around successively higher, rotating local spawn offsets.
///
/// Requires a coordinate body and counted, zero-aged `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; ignores `spawnArg1`. Active ages 1..20
/// emit independent motes that descend eight parent-axis units per active tick;
/// age 21 frees the emitter work and task. The mote effect ID must be installed.
/// Spawn copies each offset; the child motes outlive the emitter without following
/// its work or coordinate. Nonzero room effect control pauses it; values at least
/// 4 cancel it. Keep the room overlay loaded through all child lifetimes, with
/// scratch/frame packet storage available while drawing.
void neoArkNorthPromenadeRoomVisualEffectsSparkEmitterTask(Task* task);

/// Binds the room's seven combat-effect IDs once, then leaves the task idle.
///
/// Starts in state zero; later ticks leave the bindings untouched. The bindings
/// select this loaded overlay's mote, halo, orange burst,
/// spark emitter, flash, twin trail and spark burst tasks. Takes no spawn payload.
void neoArkNorthPromenadeBindRoomEffectsTask(Task* task);

/// Runs the north promenade's room-message task.
///
/// Requires a live task in state 0..2: install the room receiver, idle while
/// messages handle requests, then teardown. The room overlay must remain
/// loaded through dispatch.
void neoArkNorthPromenadeRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_NORTH_PROMENADE_H
