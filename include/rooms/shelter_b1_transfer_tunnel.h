#ifndef INCLUDE_ROOMS_SHELTER_B1_TRANSFER_TUNNEL_H
#define INCLUDE_ROOMS_SHELTER_B1_TRANSFER_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b1_transfer_tunnel_801830B8[22];

// shelter_b1_transfer_tunnel
extern u8* D_shelter_b1_transfer_tunnel_80182954[];

extern ViewCount D_shelter_b1_transfer_tunnel_80182958[];

extern DirectionWarpEntry D_shelter_b1_transfer_tunnel_8018295C[];

extern WorldCollisionGrid D_shelter_b1_transfer_tunnel_80182AEC;

extern ViewCamera D_shelter_b1_transfer_tunnel_80182B10[];

extern SpriteView D_shelter_b1_transfer_tunnel_80182BE0[];

extern WorldCoordRoomLights D_shelter_b1_transfer_tunnel_80182D90;

extern WorldCollisionTrigger D_shelter_b1_transfer_tunnel_80182DA8[];

extern WorldCollisionTrigger D_shelter_b1_transfer_tunnel_80182ED8[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_transfer_tunnel_80183184[];

/// Runs the Transfer Tunnel room controller for synchronous room messages.
///
/// Requires a live task with state 0 (register handlers), 1 (idle), or 2 (kill).
/// Keep the room and Shelter map overlays loaded while its task and borrowed
/// message table remain available.
void shelterB1TransferTunnelRoomTask(Task* task);

/// Runs a pink charge flash, a screen tint at its peak, and a fading star.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `effectSpawn`. Start in state 0 with a
/// positive charge duration in `spawnArg1.value`, in active ticks; that word
/// becomes a countdown. Nonzero room effect control pauses updates; values
/// at least 4 cancel. Completion or state 3 releases the work and task.
/// The room overlay and effect controller must remain live while it runs.
void shelterB1TransferTunnelRoomVisualEffectsFlashTask(Task* task);

/// Records and draws two fading trails between parent-relative endpoints.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`; its borrowed parent coordinate must remain live.
/// State 0 allocates two eight-slot histories in `Task::work`, seeding them at
/// offsets (0, 190, -15) and (0, 1085, 180) in parent-space world units.
/// Allocation failure resets age and retries on the next eligible tick.
/// State 1 stores world-space snapshots and draws seven quads with RGB weights
/// 1:2:3. `spawnArg1.value` is an exact expiry age, compared only in state 1;
/// zero never expires, and age is signed 16-bit. Expiry releases both histories,
/// the counted work and the task. Control values below 2 allow updates,
/// including pause value 1; values at least 2 freeze this task.
/// The room overlay and effect controller must remain live while it runs.
void shelterB1TransferTunnelRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs an impact flash followed by smoke or orange rings and bouncing sparks.
///
/// Start in state 0 with the coordinate body and zeroed, counted `EffectWork`
/// in `spawnArg2.pointer` supplied by `effectSpawn`. Nonzero `spawnArg1.value`
/// selects smoke; zero selects two sparks and fading rings. Active age seven
/// enters release; the next active tick frees work and kills the task.
/// Nonzero room effect control pauses it; four or above cancels it. Child effects
/// live independently. Keep the coordinate ancestors, effect controller, room
/// overlay and current graphics workspace live through teardown.
void shelterB1TransferTunnelRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs an animated vertical mote that brightens or starts steady, then fades.
///
/// Requires a coordinate body and a counted, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `effectSpawn`, with task state 0.
/// `spawnArg1` packs a world-unit half-extent in bits 0..11, a palette selector
/// in 12..15, unsigned speed in 16..23, and signed lifetime in 24..31, in active
/// ticks. Motion bits 0..1 overlap the half-extent: either selects steady
/// brightness; bit 1 selects upward motion, otherwise downward. With neither
/// bit set, upward speed adds a random 0..63 units per tick while brightening.
/// Initialization neither moves nor draws; later active ticks move in local Y
/// and draw on odd ages. Fade starts after age exceeds lifetime minus eight.
/// Nonzero room effect control pauses updates; values at least 4 cancel.
/// Completion and cancellation release the work and task. The room overlay
/// and effect controller must remain live while it runs.
void shelterB1TransferTunnelRoomVisualEffectsMoteTask(Task* task);

/// Runs an expanding tinted halo and shrinking ring, followed by a fading star.
///
/// Requires a coordinate body and a counted, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`, with task state 0. The work's parent must remain live;
/// `pos` is a parent-space offset in world units. The signed halves of
/// `spawnArg1` supply a positive expansion duration in active ticks (low) and
/// tint index 0..2 (high); initialization replaces the word with a countdown.
/// Nonzero room effect control pauses updates; values at least 4 cancel.
/// Completion or state 3 releases the work and task. The room overlay and
/// effect controller must remain live while it runs.
void shelterB1TransferTunnelRoomVisualEffectsHaloTask(Task* task);

/// Runs an expanding orange disc and layered glow inside a fading ring.
///
/// Requires a coordinate body and a counted, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`, with task state 0. `spawnArg1` is unused. Sizes are
/// world units; the ring fades before the centre, and the glow refreshes an
/// orange point light and a ground glow. Nonzero room effect control pauses
/// updates; values at least 4 cancel. Completion and cancellation release the
/// work and task. The room overlay and effect controller must remain live.
void shelterB1TransferTunnelRoomVisualEffectsHaloOrangeBurstTask(Task* task);

/// Emits twenty descending motes around its spawn coordinate.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; `spawnArg1` is unused. Active ages
/// 1..20 emit at turning radial offsets with local Y = -128 times age in
/// coordinate units. Motes descend eight units per active motion tick and
/// live independently after age 21 releases the emitter's work and task.
/// Nonzero room effect control pauses it; four or above cancels it. Requires
/// the installed mote effect; keep coordinate ancestors, controller and room
/// overlay live through the emitted effects' lifetimes.
void shelterB1TransferTunnelRoomVisualEffectsSparkEmitterTask(Task* task);

/// Registers the room's actor effects once and draws glows for the mapped view.
///
/// Start in state 0 to install the seven effect IDs; subsequent ticks retain
/// those bindings. View 2 draws one dim grey capsule and a red disc; view 3
/// draws two capsules and the same disc; view 4 draws two capsules. Other views
/// draw nothing. Radius scales are 512 world units; capsule angles are zero
/// and a half-turn, in 4096 units per turn. Requires the current view transform,
/// scratch stack, ordering table and packet arena, with this overlay loaded.
void shelterB1TransferTunnelDrawGlowsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_TRANSFER_TUNNEL_H
