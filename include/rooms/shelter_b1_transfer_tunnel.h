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

/// Transfer Tunnel actor layouts, indexed by the saved area variant (0..21).
///
/// Variant 1 places six Watchers and actor_200300; variant 11 replaces that
/// actor with a Rook GOLEM. Other slots have no placements or resources.
/// Loading and actors borrow the selected tables while this overlay is loaded.
extern AreaVariant gShelterB1TransferTunnelAreaVariants[22];

/// Room 1's logical-view map, borrowing gameplay's identity map for views 1..4.
///
/// Maps view minus one to a 1-based camera, image and sprite index. Keep the
/// gameplay and room overlays loaded during lookups; there is no terminator.
extern u8* gShelterB1TransferTunnelRoomViewMaps[1];

/// Room 1's four logical views, stored as a signed 16-bit element count.
extern ViewCount gShelterB1TransferTunnelRoomViewCounts[1];

/// Two warp endpoints: 1 toward the B1 Main Corridor, 2 toward the B1 Access Tunnel.
///
/// Warp IDs are 1-based; arrivals select views 2 and 4 respectively. Records
/// supply player/companion world placements and door sounds without a terminator.
/// The main-corridor endpoint also marks the R47 map flag. Keep this overlay loaded.
extern DirectionWarpEntry gShelterB1TransferTunnelWarpEntries[2];

/// Transfer Tunnel wall, ceiling and floor collision mesh and its 3-by-2 cell index.
///
/// Cells span 4000 world units, with grid origin (-642, -1736) in X/Z.
/// Six quads use surface class 0 for walls/ceiling and class 1 for the floor.
/// Activation binds the mutable grid to the view; all pools belong to this overlay.
extern WorldCollisionGrid gShelterB1TransferTunnelRoomCollision;

/// Four fixed cameras, indexed by the mapped 1-based view minus one.
///
/// Translations are negated world origins, rotations have 12 fractional bits,
/// and projection distances are pixels. View application borrows each record.
extern ViewCamera gShelterB1TransferTunnelViewCameras[4];

/// Four sprite-view records selecting decoded image strips without source sprites.
///
/// Indexed by the mapped view minus one. Each retains its own empty batch list,
/// ending at SPRITE_BATCH_END; clipping lists are NULL. Keep this overlay loaded.
extern SpriteView gShelterB1TransferTunnelSpriteViews[4];

/// The room's four point lights, with no directional or cone lights.
///
/// All views share the mutable lights; coordinate binding and lighting queries
/// update their transforms and attenuation. The collection borrows overlay storage.
extern WorldCoordRoomLights gShelterB1TransferTunnelRoomLights;

/// Four directed camera boundaries implementing 2-to-3, 3-to-2, 4-to-3 and 3-to-4.
///
/// The final live record carries WORLD_COLLISION_TRIGGER_LAST. Room setup
/// parents and links these mutable quads; keep them loaded until unlinked.
extern WorldCollisionTrigger gShelterB1TransferTunnelViewBoundaryTriggers[4];

/// Facing-gated door quads to the B1 Main Corridor and B1 Access Tunnel.
///
/// Both arrive at destination endpoint 4, departing through local endpoints
/// 1 and 2 respectively. The last live record carries WORLD_COLLISION_TRIGGER_LAST.
/// Room setup parents and links the mutable quads; keep them loaded until unlinked.
extern WorldCollisionTrigger gShelterB1TransferTunnelWarpTriggers[2];

/// Collision policies for surface classes 0..7; only floor class 1 has footstep cues.
///
/// All classes block probes, allow weapon impacts and apply pushback. Entries
/// borrow two policy records in this overlay, which must stay loaded during use.
extern WorldCollisionSurfaceProperties* gShelterB1TransferTunnelSurfacePropertiesByClass[8];

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
