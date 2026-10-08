#ifndef INCLUDE_ROOMS_DRYFIELD_MAIN_STREET_H
#define INCLUDE_ROOMS_DRYFIELD_MAIN_STREET_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_main_street_80184F20[13];

// dryfield_main_street
extern WorldCollisionRoomResources D_dryfield_main_street_80181BBC[];

extern u8* D_dryfield_main_street_80181BCC[];

extern ViewCount D_dryfield_main_street_80181BD0[];

extern WorldCoordRoomLighting D_dryfield_main_street_80181BD4[];

extern DirectionWarpEntry D_dryfield_main_street_80181BDC[];

extern ViewCamera D_dryfield_main_street_80182CC0[];

extern SpriteView D_dryfield_main_street_80184308[];

extern WorldCollisionSurfaceProperties* D_dryfield_main_street_801855EC[];

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
