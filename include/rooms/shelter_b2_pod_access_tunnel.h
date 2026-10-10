#ifndef INCLUDE_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_H
#define INCLUDE_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Enemy placement and resource pairs indexed by the saved area variant, 0..22.
///
/// Populated variants are 1..4, 7, 11, 21 and 22; every other slot is empty.
/// Variant 22 contains a Bishop GOLEM. Each populated pair has terminated
/// tables. Gameplay borrows this directory during loading and spawning, and
/// spawned enemies borrow its placements until release; keep this room loaded.
extern AreaVariant gShelterB2PodAccessTunnelAreaVariants[23];

/// Collision resources for logical rooms 1 and 2, indexed by room minus one.
///
/// Both share the tunnel grid, view boundaries and sight occluder; each room
/// has its own mutable action-trigger array. Setup binds and links the borrowed
/// records. Keep this overlay loaded until the resources have been unlinked.
extern WorldCollisionRoomResources gShelterB2PodAccessTunnelRoomCollisionResources[2];

/// Lighting records for logical rooms 1 and 2 of the pod access tunnel.
///
/// Both borrow the same eight-point-light collection and use default ambient
/// minima. Index by room minus one; pointers require this overlay loaded.
extern WorldCoordRoomLighting gShelterB2PodAccessTunnelRoomLighting[2];

/// Logical-view maps for rooms 1 and 2 of the pod access tunnel.
///
/// Index by room minus one, then logical view minus one (views 1..7).
/// Room 1 borrows the gameplay identity map; room 2 swaps 3/4 with 6/7.
/// Values are 1-based camera, image and sprite indices. Keep gameplay and
/// this overlay loaded while the directory and its maps are borrowed.
extern u8* gShelterB2PodAccessTunnelRoomViewMaps[2];

/// Seven logical views in each of rooms 1 and 2, indexed by room minus one.
///
/// Signed element counts bound searches in `gShelterB2PodAccessTunnelRoomViewMaps`;
/// the arrays have no terminator. Borrow while this overlay is loaded.
extern ViewCount gShelterB2PodAccessTunnelRoomViewCounts[2];

/// Arrival placements and departure options for the tunnel's three warp slots.
///
/// Index by warp minus one (1..3): septic-tank door, R48 door and pod arrival.
/// Initial views are 2, 4 and 4. Slots 2/3 update the gantry/pod map markers.
/// Positions use game-coordinate units and yaw uses 4096 units per turn.
/// Consumers copy complete entries; keep this overlay loaded through the copy.
extern DirectionWarpEntry gShelterB2PodAccessTunnelWarpEntries[3];

/// Seven cameras indexed by the pod access tunnel's mapped view minus one.
///
/// Cameras 6/7 repeat 3/4 for alternate images. Translation uses world units,
/// rotation uses 12 fractional bits, and projection distance is in pixels.
/// Queued camera applications borrow the record until dispatch.
extern ViewCamera gShelterB2PodAccessTunnelViewCameras[7];

/// Sprite resources for the pod access tunnel's seven mapped views.
///
/// Index by mapped view minus one. Views 3/6 select 60/54 source rectangles;
/// views 1/2/4/5/7 retain empty batch lists. All use decoded image strips
/// and no clipping-area list. Keep this overlay loaded through packet setup
/// and drawing; the source arrays and batch lists remain writable.
extern SpriteView gShelterB2PodAccessTunnelSpriteViews[7];

/// Pod access tunnel collision and footstep policies for surface classes 0..7.
///
/// Class 1 passes probes and ignores weapon impacts; every other class blocks
/// probes and allows impacts. All apply pushback. Class 3 supplies footstep cues,
/// while the other classes are silent. Classes 0 and 4..7 share one record;
/// class 2 has a distinct record with the same proven policies. Setup reads all
/// eight entries. Borrow the directory, records and cues while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gShelterB2PodAccessTunnelSurfacePropertiesByClass[8];

/// Runs the pod access tunnel's room setup, idle or teardown state.
///
/// The map overlay spawns this bodyless task at state 0. State 0 registers it
/// for room messages, state 1 idles, and state 2 kills it; only 0..2 are valid.
/// Spawn arguments are unused. Keep the room overlay and its message table
/// loaded until teardown; leaving the room releases the registered task.
void shelterB2PodAccessTunnelRoomTask(Task* task);

/// Advances the pod access tunnel's drifting animated sprite and releases it at completion.
///
/// `task` owns initialized `EffectWork` in `spawnArg2.pointer` and a coordinate
/// body, normally supplied by `effectSpawn` with state 0 and cell index 0.
/// The first running update initializes without drawing; later updates draw
/// before moving, accelerating and advancing through 12 banked or 10 alternate
/// cells. Nonzero `RoomEffectState::effectControl` freezes updates and redraws
/// the current drawer and palette, including hidden control. Values >= 4 free
/// work and task after that final draw; borrowed work pointers then expire.
/// Drawing requires a composed coordinate, initialized scratch and packet space.
///
/// `spawnArg1.value` packs perspective size in bits 0..11, frames per cell in
/// 12..14, speed in 16..23 (0 means 64 coordinate units per running update),
/// movement kind in 24..27, palette bank in 28..30, and alternate drawer in 31.
/// Frames per cell default to 1 only when bits 12..15 are all zero; otherwise
/// bits 12..14 must encode 1..7. Bit 15 alone encodes an invalid zero divisor.
/// Initial spin uses 4096 units per turn.
///
/// Existing nonzero velocity is retained. For initially zero velocity, kind 0
/// disables movement; kinds 1/2/3/6 select random upward/all-axis/narrow-upward/
/// planar directions and normalize to the selected speed. Kind 5 takes the
/// offset's Y/Z and the saved palette bits as X. Moving updates subtract 2/1
/// from banked/alternate Y velocity; kind 7 instead adds age/10. Signed-halfword
/// velocity components wrap on assignment; positions use coordinate units.
void shelterB2PodAccessTunnelEffectSpriteDriftTask(Task* task);

/// Runs a pink charge flash, a peak screen tint and a fading star.
///
/// Start in state 0 with a coordinate body and counted, zero-initialized
/// `EffectWork` owned by `spawnArg2.pointer`, as supplied by `effectSpawn`.
/// `spawnArg1.value` is a positive charge duration in active ticks, consumed
/// as a countdown. Nonzero room effect control pauses updates; values >= 4
/// cancel. Completion or state 3 releases work and task. The room overlay,
/// effect controller, current view, scratch stack and packet arena must be live.
void shelterB2PodAccessTunnelRoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws their fading blue ribbon trail.
///
/// Start in state 0 with a coordinate body and counted `EffectWork` owned by
/// `spawnArg2.pointer`, with age 0 and a live borrowed parent coordinate.
/// Parent-space endpoints are (0, 190, -15) and (0, 1085, 180), in game units.
/// Initialization owns two eight-coordinate histories in `Task::work`; failed
/// allocation resets age to 0 and retries. State 1 records world-space snapshots
/// and draws seven quads through the current view, with RGB weights 1:2:3.
/// `spawnArg1.value` is an exact expiry age, tested only in state 1 against the
/// signed 16-bit age; 0 never expires. Expiry releases histories, work and task.
/// Control values < 2 advance and draw, including pause value 1; values >= 2
/// freeze without cancelling. The room overlay, effect controller, view,
/// scratch stack and packet arena must remain live while the task runs.
void shelterB2PodAccessTunnelRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs this room's impact flash followed by smoke or fading orange spark rings.
///
/// Start at state 0 with a coordinate body and counted, zero-aged `EffectWork`
/// owned through `spawnArg2.pointer`. Nonzero `spawnArg1.value` emits seven
/// smoke puffs; zero emits two bouncing sparks and draws fixed/expanding rings.
/// Active age 7 enters release; age 8 releases work and task. Children are
/// independent. Room-effect control 1..3 pauses and 4 or above cancels. Keep
/// this room, coordinate ancestors, effect resources, scratch and GPU arena live
/// until teardown; caller work pointers become invalid when the effect ends.
void shelterB2PodAccessTunnelRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs an animated vertical mote that brightens or starts steady, then fades.
///
/// Start in state 0 with a coordinate body and counted, zero-initialized
/// `EffectWork` owned by `spawnArg2.pointer`, as supplied by `effectSpawn`.
/// `spawnArg1` packs half-extent in game units in bits 0..11, palette in 12..15
/// (0 default), unsigned speed in 16..23 and signed lifetime in 24..31, in
/// active ticks. Motion bits 0..1 overlap the extent: either selects steady
/// brightness, and bit 1 selects upward motion, otherwise downward. With both
/// clear, the mote brightens while rising at speed plus a random 0..63.
/// Initialization neither moves nor draws. Later active ticks move along local
/// Y and draw on odd ages; fading starts after age exceeds lifetime minus eight.
/// Nonzero room effect control pauses updates; values >= 4 cancel. Completion
/// and cancellation release work and task. The room overlay, effect controller,
/// coordinate ancestors, view, scratch stack and packet arena must remain live.
void shelterB2PodAccessTunnelRoomVisualEffectsMoteTask(Task* task);

/// Runs an expanding tinted halo and shrinking ring, then a fading star.
///
/// Start in state 0 with a coordinate body and counted, zero-initialized
/// `EffectWork` owned by `spawnArg2.pointer`. Its borrowed parent must stay
/// live; `pos` is the parent-space offset in game units. The unsigned low half
/// of `spawnArg1` holds expansion duration 1..65535 in active ticks; its signed
/// high half holds tint index 0..2. Initialization consumes the tint and replaces the
/// word with a countdown. Nonzero room effect control pauses every phase;
/// values >= 4 cancel. Completion or state 3 releases work and task. The room
/// overlay, effect controller, view, scratch stack and packet arena must be live.
void shelterB2PodAccessTunnelRoomVisualEffectsHaloTask(Task* task);

/// Runs an expanding orange disc and layered glow inside a fading ring.
///
/// Start in state 0 with a coordinate body and counted, zero-initialized
/// `EffectWork` owned by `spawnArg2.pointer`. `spawnArg1` is unused. Sizes are
/// game units; the ring fades before the centre, while the glow refreshes a
/// transient orange point light and draws a ground glow on a collision hit.
/// Nonzero room effect control pauses updates; values >= 4 cancel. Completion
/// and cancellation release work and task. The room overlay, effect controller,
/// coordinate ancestors, lighting, collision, view, scratch stack and packet
/// arena must remain live while it runs.
void shelterB2PodAccessTunnelRoomVisualEffectsHaloOrangeBurstTask(Task* task);

/// Emits twenty motes at rotating, successively higher offsets around its coordinate.
///
/// Start at state 0 with a coordinate body and counted, zero-aged `EffectWork`
/// owned through `spawnArg2.pointer`; `spawnArg1` is unused. Active ages 1..20
/// emit one mote at local Y = -128*age in coordinate units; age 21 frees the
/// emitter. Motes are independent, descend eight units per active tick and can
/// outlive it. Requires this room's installed mote callback and live coordinate
/// ancestors/effect resources. Room-effect control 1..3 pauses, 4 or above cancels.
/// Keep the room, scratch and GPU arena live through emitter/child teardown.
void shelterB2PodAccessTunnelRoomVisualEffectsSparkEmitterTask(Task* task);

/// Registers the room's actor effects once and draws visible light-fixture beams.
///
/// Start in state 0 to install seven room effect IDs; later ticks retain them.
/// Mapped view 2 draws sixteen dim grey capsules, views 3/6 draw twelve, and
/// views 4/7 draw four; other views draw nothing. Endpoint positions are world
/// game units. Every capsule uses radius scale 512; screen angles are zero,
/// +/- one quarter-turn or one half-turn, in 4096 units per turn.
/// Requires this room overlay, current view transform, scratch stack, ordering
/// table and packet arena. The task does not consult room effect control.
void shelterB2PodAccessTunnelDrawLightBeamsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_H
