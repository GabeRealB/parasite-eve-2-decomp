#ifndef INCLUDE_ROOMS_SHELTER_B6_NURSERY_H
#define INCLUDE_ROOMS_SHELTER_B6_NURSERY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gShelterB6NurseryModel07BAC;

extern AreaVariant D_shelter_b6_nursery_801874A4[13];

// shelter_b6_nursery
extern u8* D_shelter_b6_nursery_80185304[];

extern ViewCount D_shelter_b6_nursery_80185308[];

extern DirectionWarpEntry D_shelter_b6_nursery_8018530C[];

extern WorldCollisionGrid D_shelter_b6_nursery_801858A0;

extern ViewCamera D_shelter_b6_nursery_801858C4[];

extern SpriteView D_shelter_b6_nursery_80186FD0[];

extern WorldCoordRoomLights D_shelter_b6_nursery_80187294;

extern WorldCollisionTrigger D_shelter_b6_nursery_801872AC[];

extern WorldCollisionTrigger D_shelter_b6_nursery_8018750C[];

extern WorldCoordRoomAmbientEntry D_shelter_b6_nursery_8018789C[];

extern WorldCollisionSurfaceProperties* D_shelter_b6_nursery_80187958[];

/// Dispatches the nursery room task's initialization, idle or release state.
///
/// Requires this overlay and `task->state` in 0..2. Initialization installs
/// room messages, registers the room task, restores companion HP and starts
/// the entry script selected by nursery progress. State 1 waits for messages;
/// state 2 releases the task. Dispatch uses a copy of the three-entry table.
void shelterB6NurseryRoomTask(Task* task);

void func_shelter_b6_nursery_8017FFF4(void);

/// Shows or hides the second sprite batch in the nursery's view 13.
///
/// `hidden` is 0 to show or 1 to hide; other byte values leave it unchanged.
/// Requires the active session's stage and area to select the nursery sprite
/// tables. Changes view 13 regardless of the current view.
void shelterB6NurserySetView13SpriteHidden(u8 hidden);

/// Spins and draws one grey triangular shard of the nursery's spark shower.
///
/// Requires a coordinate body and counted, owned `EffectWork` in `spawnArg2`.
/// The low 12 bits of `spawnArg1.value` give its radius in local coordinate
/// units. Initializes a Q12 direction, speed 64..127, shade 64..191 and Euler
/// increments in 4096 units per turn. Releases the work when parent-space Y
/// exceeds zero. Room effect control 2..3 pauses it; 4 or above cancels it.
void shelterB6NurserySparkShowerShardTask(Task* task);

/// Sets the nursery's glint-pulse and spark-shower cues for its view-effect task.
///
/// Stores the low 16 bits of each argument. A nonzero fastGlintPulse selects
/// the faster glint pulse in views 3, 6, 8 and 10; a nonzero sparkShowerScale
/// triggers sixteen shards in view 13 and scales their spread and radius.
/// The view-effect task clears both on initialization and after that shower.
/// Requires the nursery overlay and its initialized view-effect task to stay
/// loaded until the cues are consumed; (0, 0) clears any pending cues.
void shelterB6NurserySetEffectCues(s32 fastGlintPulse, s32 sparkShowerScale);

/// Draws the nursery's view glints and spawns its debris, particles and cued shower.
///
/// Requires the nursery overlay, active view, room-effect state and effect
/// resources. Initialization publishes the nursery flash/trail/burst IDs and
/// clears both effect cues. View 12 spawns three chunks once; view 13 enables
/// ongoing particles at two anchors, including after leaving that view.
/// A shower cue in view 13 spawns sixteen shards and then clears both cues.
/// Particle emission uses alternate animation frames. This controller does
/// not inspect room-effect control or release itself; children own their work.
void shelterB6NurseryViewEffectsTask(Task* task);

/// Tumbles and bounces one nursery debris model while emitting trail particles.
///
/// Requires a TMD body and counted, owned `EffectWork` in `spawnArg2.pointer`.
/// Initialization enables model drawing, seeds a Q12 direction and speed
/// 64..127 coordinate units per active frame, and stores Euler increments in
/// `work->pos` at 4096 units per turn. Collision mixes the room-space normal
/// into that direction and reduces speed to two thirds; a missed collision
/// adds 384 to its signed-halfword Y direction. Trail particles spawn on odd
/// active ages and switch texture strips after age 64. Control 2..3 pauses;
/// control 4 or above or leaving mapped view 12 releases the work. There is
/// no age-based release, and the signed-halfword age retains its wrap.
void shelterB6NurseryDebrisChunkTask(Task* task);

/// Animates one nursery particle through a ten-frame or eight-frame texture strip.
///
/// Requires a coordinate body and counted, owned `EffectWork` in `spawnArg2`.
/// `spawnArg1.value` packs half-diagonal (bits 0..11), frame period (12..14,
/// default 1 when bits 12..15 are zero), speed (16..23, zero selects 64),
/// motion mode (24..27), and the eight-frame strip selector (bit 31).
/// A nonzero period is required: bit 15 alone must not select the period.
/// Motion modes are 0 stationary, 1 negative-Y fan, 2 scatter, 3 negative-Y
/// jet, 5 spawn-offset direction, 6 XZ scatter, and 7 parent-rotated jet.
/// Existing nonzero `move` bypasses direction generation and speed scaling.
/// Each frame draws before moving, then advances the strip at its period;
/// completion releases the work. This task runs regardless of room effect control.
void shelterB6NurseryAnimatedParticleTask(Task* task);

/// Runs the nursery's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and counted, owned `EffectWork` in `spawnArg2`.
/// `spawnArg1.value` is a positive charge duration in active ticks, consumed
/// as a countdown. Nonzero room effect control pauses it; control 4 or above,
/// state 3, or completion releases the effect.
void shelterB6NurseryRoomVisualEffectsFlashTask(Task* task);

/// Records two anchor-relative endpoints and draws their fading trail as a beam.
///
/// Requires a coordinate body and counted, owned `EffectWork` in `spawnArg2`;
/// its parent coordinate stays live through teardown. `work` starts null and
/// owns sixteen allocated coordinate snapshots, eight for each endpoint.
/// `spawnArg1.value` is the nonzero signed-16-bit age at which to finish;
/// zero leaves lifetime to external teardown. Age advances with room effect
/// control below 2, including initialization; allocation failure resets it
/// for retry. Teardown releases both allocations.
void shelterB6NurseryRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the nursery's impact flash with smoke or fading rings and bouncing sparks.
///
/// Requires a coordinate body and counted, zero-aged `EffectWork` in
/// `spawnArg2.pointer`. Nonzero `spawnArg1.value` selects smoke; zero selects
/// rings and sparks. Enters release at active age 7 and frees work on the next
/// active tick. Nonzero room control below 4 pauses; 4 or above cancels.
/// Spawned effects are independent tasks and keep their own lifetimes.
void shelterB6NurseryRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs the nursery telephone's save menu and optional statistics panels.
///
/// Requires a live telephone `UiObject` in `task->spawnArg2.pointer` and this
/// overlay to stay loaded. Normal uncleared play opens saving directly; a
/// cleared game or attract-demo scene 1 enables the four-row menu. Save results
/// open a saved/cancelled notice; dismissal returns to the menu or closes the
/// save-only telephone. Cancel publishes the No result and clears UI-open state.
void shelterB6NurseryTelephoneMenuTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B6_NURSERY_H
