#ifndef INCLUDE_ROOMS_NEO_ARK_SOUTH_PROMENADE_H
#define INCLUDE_ROOMS_NEO_ARK_SOUTH_PROMENADE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_neo_ark_south_promenade_801808E4[13];

/// Extents of the promenade's mapped view resources and surface-class directory.
enum {
    NEO_ARK_SOUTH_PROMENADE_VIEW_COUNT          = 5,
    NEO_ARK_SOUTH_PROMENADE_SURFACE_CLASS_COUNT = 8,
};

/// Lighting for room 1: two directional lights and the default ambient minima.
///
/// Indexed by room - 1. Borrows mutable lights from the loaded promenade overlay.
extern WorldCoordRoomLighting gNeoArkSouthPromenadeRoomLighting[1];

extern WorldCollisionRoomResources D_neo_ark_south_promenade_8017F6F4[];

/// Logical-view map for room 1, mapping views 1..5 to the same resource indices.
///
/// Indexed by room - 1, then view - 1. Borrows gameplay's `gViewIdentityMap`;
/// the room uses its first `NEO_ARK_SOUTH_PROMENADE_VIEW_COUNT` entries.
/// Keep the room and gameplay overlays loaded while the directory is borrowed.
extern u8* gNeoArkSouthPromenadeViewMaps[1];

/// Logical-view search count for room 1, indexed by room - 1.
///
/// Five identity-mapped views each select a camera, image and sprite descriptor.
/// Borrow this signed halfword array only while the room overlay is loaded.
extern ViewCount gNeoArkSouthPromenadeViewCounts[1];

extern DirectionWarpEntry D_neo_ark_south_promenade_8017F70C[];

/// Cameras for mapped views 1..5, indexed by mapped view - 1.
///
/// Rotations use `ONE` for 1.0, translations are negated world-space camera origins,
/// and perspective distances are pixels. Keep the overlay loaded until any queued
/// camera application finishes.
extern ViewCamera gNeoArkSouthPromenadeViewCameras[NEO_ARK_SOUTH_PROMENADE_VIEW_COUNT];

/// Sprite descriptors for mapped views 1..5, indexed by mapped view - 1.
///
/// Views 1, 4 and 5 have no source sprites; views 2 and 3 have 25 and 39 sources.
/// All views retain image-strip backgrounds and have no clipping-area list.
/// Descriptors and mutable source/batch arrays require the overlay to stay loaded.
extern SpriteView gNeoArkSouthPromenadeSpriteViews[NEO_ARK_SOUTH_PROMENADE_VIEW_COUNT];

/// Collision policies and footstep cues indexed by surface class 0..7.
///
/// Class 1 blocks probes and allows weapon impacts with footsteps; class 2 passes
/// probes and ignores weapon impacts with the same cues. Other classes block
/// probes and allow impacts without cues. All classes apply grid pushback.
/// The directory and its borrowed records require the room overlay to stay loaded.
extern WorldCollisionSurfaceProperties* gNeoArkSouthPromenadeSurfaceProperties[NEO_ARK_SOUTH_PROMENADE_SURFACE_CLASS_COUNT];

/// Records two offset endpoints of a moving anchor and draws their fading beam.
///
/// Requires a coordinate body, initialized room-effect state and an owned
/// `EffectWork` in `spawnArg2.pointer`; its borrowed parent must outlive the task.
/// `spawnArg1.value` is a lifetime compared with the wrapping signed 16-bit age
/// after initialization (0 disables timed teardown). Two eight-entry histories
/// belong to `Task::work` and are freed with the task. Control values below 2
/// advance the beam; higher values suspend it without cancelling it.
void neoArkSouthPromenadeRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs an impact flash followed by smoke puffs or expanding orange rings and sparks.
///
/// Starts in state 0 with a coordinate body and owned, zero-aged `EffectWork`
/// in `spawnArg2.pointer`. Nonzero `spawnArg1.value` selects smoke; zero selects
/// rings. Active age 7 enters release; age 8 releases work and the task.
/// Room effect control 1..3 pauses and 4 or above cancels. Spawned effects
/// remain independent. Keep this overlay and the effect controller loaded.
void neoArkSouthPromenadeRoomVisualEffectsSparkBurstTask(Task* task);

/// Registers this room's golem scream flash, sword trail and projectile burst once.
///
/// State 0 publishes the effect IDs and advances to state 1, which does no work.
/// Keep this room overlay loaded while the registered effect tasks can run.
void neoArkSouthPromenadeRegisterGolemEffectsTask(Task* task);

/// Runs the room's golem scream flash through charging, screen tint and star fade.
///
/// Requires a coordinate body, initialized room-effect state and an owned
/// `EffectWork` in `spawnArg2.pointer`. `spawnArg1.value` must start as a positive
/// charge duration in active ticks and is consumed as a countdown. Nonzero room
/// effect control pauses it; values >= 4 cancel it. State 3 releases the effect
/// and task on the next running tick.
void neoArkSouthPromenadeRoomVisualEffectsFlashTask(Task* task);

/// Runs the South Promenade room controller for synchronous room messages.
///
/// Requires a live task with state 0 (register handlers), 1 (idle), or 2 (kill).
/// Keep the room and Neo Ark map overlays loaded while its task and borrowed
/// message table remain available.
void neoArkSouthPromenadeRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SOUTH_PROMENADE_H
