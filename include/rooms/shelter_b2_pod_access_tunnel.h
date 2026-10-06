#ifndef INCLUDE_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_H
#define INCLUDE_ROOMS_SHELTER_B2_POD_ACCESS_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b2_pod_access_tunnel_801855AC[23];

// shelter_b2_pod_access_tunnel
extern WorldCollisionRoomResources D_shelter_b2_pod_access_tunnel_80183DEC[];

extern WorldCoordRoomLighting D_shelter_b2_pod_access_tunnel_80183E0C[];

extern u8* D_shelter_b2_pod_access_tunnel_80183E24[];

extern ViewCount D_shelter_b2_pod_access_tunnel_80183E2C[];

extern DirectionWarpEntry D_shelter_b2_pod_access_tunnel_80183E30[];

extern ViewCamera D_shelter_b2_pod_access_tunnel_801841D8[];

extern SpriteView D_shelter_b2_pod_access_tunnel_80184C6C[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_pod_access_tunnel_801856D8[];

void func_shelter_b2_pod_access_tunnel_8017DC14(Task* task);

/// Advances the pod access tunnel's drifting animated sprite and releases it at completion.
///
/// `task` owns initialized `EffectWork` in `spawnArg2.pointer` and a coordinate
/// body, normally supplied by `Gp_SpawnEff` with state 0 and cell index 0.
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
/// `EffectWork` owned by `spawnArg2.pointer`, as supplied by `Gp_SpawnEff`.
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

void func_shelter_b2_pod_access_tunnel_80182F78(Task* task);

/// Runs an animated vertical mote that brightens or starts steady, then fades.
///
/// Start in state 0 with a coordinate body and counted, zero-initialized
/// `EffectWork` owned by `spawnArg2.pointer`, as supplied by `Gp_SpawnEff`.
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

void func_shelter_b2_pod_access_tunnel_80181AF8(Task* arg0);

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
