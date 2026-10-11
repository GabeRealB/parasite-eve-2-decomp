#ifndef INCLUDE_ROOMS_DRYFIELD_MAIN_STREET_H
#define INCLUDE_ROOMS_DRYFIELD_MAIN_STREET_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Dryfield main street placement/resource directory indexed by `GameLocationKey.variant` 0..12.
///
/// Slots 1, 2, 3, 7, 10 publish paired tables; all other slots are absent (NULL, NULL).
/// This placement selector is independent of the room and logical-view selectors.
/// Variant 10 owns its placements in this room; other placements belong to the
/// stage map. All resource recipes belong to this room. Keep each storage owner
/// and the actors supplying task descriptors loaded during loading, spawning
/// and model-flag updates. Lookups require an index within this array; there is
/// no terminator or bounds check.
extern AreaVariant gDryfieldMainStreetAreaVariants[13];

/// Collision resources for Main Street's single room, indexed by room ID minus one.
///
/// Room 1 borrows the mutable grid, 26 view boundaries and ten action triggers,
/// with no sight occluders. Activation binds the geometry to the current view.
/// Keep this overlay loaded until its grid is retired and its triggers unlinked.
extern WorldCollisionRoomResources gDryfieldMainStreetRoomCollisionResources[1];

/// Logical-view map directory for Main Street's single room.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..13 select the same
/// camera, image and sprite indices. The shared map has its own capacity;
/// the room's view count limits reverse searches. Keep this directory live
/// while the Main Street overlay is loaded.
extern u8* gDryfieldMainStreetRoomViewMaps[1];

/// Reverse-lookup view count for Main Street's single room.
///
/// Room 1 searches all thirteen identity-map entries. Cameras and sprite views
/// cover the same indices; the map itself has a separate, larger capacity.
/// The Dryfield map borrows this array while the room overlay is loaded.
extern ViewCount gDryfieldMainStreetRoomViewCounts[1];

/// Lighting directory for Main Street's single room.
///
/// Room 1 borrows sixteen mutable point lights, eligible in every view, and
/// uses gameplay's default ambient minima. The directory and light storage
/// remain valid only while the Main Street overlay is loaded.
extern WorldCoordRoomLighting gDryfieldMainStreetRoomLighting[1];

/// Main Street's seven arrival/departure endpoints, indexed by warp ID minus one.
///
/// Slots 1..7 lead to Gas Station, Motel Room 1, Parking Lot, Motel Rooms 2..4,
/// and Driveway, respectively. Player and companion share each arrival pose;
/// XYZ uses room units and yaw uses 4096 per turn. Each endpoint supplies its
/// arrival view, area-bank sounds and map-marker flag. There is no terminator.
/// Warp handling copies a complete entry while this room overlay is loaded.
extern DirectionWarpEntry gDryfieldMainStreetWarpEntries[7];

/// Main Street's thirteen cameras, indexed by mapped view minus one.
///
/// The identity map makes logical views 1..13 select the same array indices.
/// Rotations use ONE (4096), translations are negated room-space camera origins,
/// and projection distances use pixels. No count or terminator is stored here.
/// Queued camera application requires this overlay until the record is consumed.
extern ViewCamera gDryfieldMainStreetViewCameras[13];

/// Sprite resources for Main Street's thirteen mapped views, indexed by view minus one.
///
/// Views 1, 8, 9, 11 and 12 use empty batch lists and decoded background strips;
/// the remaining views supply mutable depth-sorted sources. Every batch list
/// ends with SPRITE_BATCH_END, and no view uses a clipping list. The Dryfield
/// map borrows this directory and its storage while the room overlay is loaded.
extern SpriteView gDryfieldMainStreetSpriteViews[13];

/// Main Street's collision and footstep policies, indexed by surface class 0..7.
///
/// Classes 0 and 5..7 share a silent blocking record. Class 1 passes probes
/// and ignores weapon impacts; classes 2..4 block probes and supply distinct
/// footstep cue sets. All classes apply pushback. Records and cue sets are
/// borrowed from this room overlay and must not outlive it.
extern WorldCollisionSurfaceProperties* gDryfieldMainStreetSurfacePropertiesByClass[8];

/// Animates and drifts one daytime main street puff (effect 0x601B1).
///
/// Requires the counted effect task and zeroed `EffectWork` created by
/// `effectSpawn`, with a live coordinate body and work in `spawnArg2.pointer`.
/// `spawnArg1.value` packs size factor in bits 0-11 (0..4095), cell period in bits
/// 12-14 (1..7 ticks), and speed in bits 16-23 (coordinate units per tick).
/// A zero period nibble (bits 12-15) selects one tick; a zero speed byte selects
/// 64. A nonzero period nibble must have nonzero bits 12-14; bit 15 alone
/// decodes to zero and is invalid. Bits 24-31 are ignored.
///
/// Initializes a fixed random screen angle (4096 units per turn) and a drift
/// with nonpositive local X, zero Y and signed Z. Draws before moving; cell zero
/// lasts one tick and cells 1-9 each last the selected period. Releases its work
/// and task after 1 + 9 * period ticks. Normal scheduling must compose the dirty
/// coordinate between ticks; callers must discard the work pointer on release.
void dryfieldMainStreetPuffTask(Task* task);

/// Runs Main Street's charging pink flash, peak screen tint and fading star.
///
/// Starts in state zero with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`. `spawnArg1.value` is a positive
/// charge duration in active ticks, consumed as a countdown. Nonzero room
/// effect control pauses it; values at least four cancel it. State three also
/// requests release. Completion frees the effect work and tears down the task.
/// Requires a live `gRoomEffectState` and the room overlay to remain loaded.
void dryfieldMainStreetRoomVisualEffectsFlashTask(Task* task);

/// Runs Main Street's fading beam between two moving endpoint histories.
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
void dryfieldMainStreetRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs Main Street's impact flash followed by smoke or orange rings and sparks.
///
/// Effect 0x60295 requires a state-zero coordinate task and counted, zero-aged
/// `EffectWork` in `spawnArg2.pointer` from `effectSpawn`. Nonzero
/// `spawnArg1.value` selects smoke; zero selects rings and bouncing sparks.
/// Enters release at active age seven and frees work and task on age eight.
/// Room effect control pauses below four when nonzero and cancels at four or
/// above. Child effects live independently. Requires live room effect state
/// and the room overlay to stay loaded; discard borrowed work after teardown.
void dryfieldMainStreetRoomVisualEffectsSparkBurstTask(Task* task);

/// Publishes Main Street's view effect mode and maintains view-eight smoke.
///
/// Effect 0x600C3 starts in state zero, installs the room's flash/trail/burst
/// effect IDs, and retains the last mapped view in `spawnArg1.value`.
/// Entry to mapped view eight requests 48 puffs; later odd animation frames
/// request one. Placement is sampled in the input space of `GsWSMATRIX`,
/// with XYZ ranges -1185..-886, -1255..-656 and 9836..10535 game units.
/// Spawn requests copy the position from reusable room storage and may fail
/// at the normal effect limit. Requires live room effect state, a mapped view
/// covered by its mode table, and the room overlay to remain loaded. The
/// effect spawner owns this task's work and teardown.
void dryfieldMainStreetRoomEffectsTask(Task* task);

/// Registers and dispatches the daytime Main Street room task.
///
/// The Dryfield map spawns this bodyless task in state zero. States 0..2 are
/// initialization, idle and release; the selector is unchecked. Initialization
/// installs the room's message table, enables CAP completion sound cues and
/// broadcasts the initial actor command while the first-visit scene is unseen.
/// Requires a live scene task and the room overlay to remain loaded while its
/// callbacks or message table can be reached.
void dryfieldMainStreetTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MAIN_STREET_H
