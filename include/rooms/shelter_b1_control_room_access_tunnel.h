#ifndef INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_H
#define INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b1_control_room_access_tunnel_801825AC[22];

// shelter_b1_control_room_access_tunnel
extern u8* D_shelter_b1_control_room_access_tunnel_80181F00[];

extern ViewCount D_shelter_b1_control_room_access_tunnel_80181F04[];

extern DirectionWarpEntry D_shelter_b1_control_room_access_tunnel_80181F08[];

extern WorldCollisionGrid D_shelter_b1_control_room_access_tunnel_80182070;

extern ViewCamera D_shelter_b1_control_room_access_tunnel_80182094[];

extern SpriteView D_shelter_b1_control_room_access_tunnel_80182130[];

extern WorldCoordRoomLights D_shelter_b1_control_room_access_tunnel_801822D4;

extern WorldCollisionTrigger D_shelter_b1_control_room_access_tunnel_801822EC[];

extern WorldCollisionTrigger D_shelter_b1_control_room_access_tunnel_80182384[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_control_room_access_tunnel_80182678[];

void func_shelter_b1_control_room_access_tunnel_8017D68C(Task* task);

/// Binds the room's actor effects once and draws its glows for the mapped view.
///
/// A fresh task starts in state 0; later ticks keep the bindings. Views 2 and 3
/// each draw two dim grey capsules, and view 2 also draws a red disc. Radius
/// scales are 512 world units; capsule angles are +1024 in view 2 and -1024 in
/// view 3, in 4096 units per turn. Requires the current view transform, scratch
/// stack, ordering table and packet arena. Other views draw nothing.
void shelterB1ControlRoomAccessTunnelDrawGlowsTask(Task* task);

/// Runs the room's counted pink charge flash, peak screen tint and fading star.
///
/// Requires a coordinate body and an owned `EffectWork` in `spawnArg2.pointer`.
/// Start in state 0 with a positive charge duration in `spawnArg1.value`, in
/// ticks; the task consumes that countdown. Nonzero room effect control pauses
/// updates, and values at least 4 cancel. Completion or state 3 releases the
/// counted work and kills the task. The room overlay must remain loaded.
void shelterB1ControlRoomAccessTunnelRoomVisualEffectsFlashTask(Task* task);

/// Records and draws the room's counted pair of fading endpoint trails.
///
/// Requires a coordinate body and owned `EffectWork` in `spawnArg2.pointer`;
/// its borrowed parent coordinate must remain live. State 0 allocates two
/// eight-frame histories owned by `Task::work` and seeds them at the parent-
/// relative offsets (0, 190, -15) and (0, 1085, 180), in world units. State 1
/// records world-space snapshots and draws seven quads with R:G:B = 1:2:3.
/// Allocation failure resets the age and retries on the next eligible tick.
/// `spawnArg1.value` is an age in ticks: an exact nonzero match during state 1
/// releases both histories and counted work. Zero never expires; age is signed
/// 16-bit. Control values below 2 allow updates, including pause value 1;
/// values at least 2 freeze this task. The room overlay must remain loaded.
void shelterB1ControlRoomAccessTunnelRoomVisualEffectsTwinTrailTask(Task* task);

void func_shelter_b1_control_room_access_tunnel_8017F624(Task* task);

void func_shelter_b1_control_room_access_tunnel_8018026C(Task* arg0);

/// Runs the tunnel's animated spark along a fixed step toward its initial target position.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`, initialized by `Gp_SpawnEff`, with task state zero.
/// `spawnArg1.pointer` borrows a target `GfxCoord`; both world matrices must be
/// composed on the first active tick. The target is sampled only on that tick.
/// Initialization transforms the displacement into parent axes, with signed
/// 16-bit intermediate components, and scales it by 204/4096. Later active ticks
/// move by this fixed step every tick and draw on odd ages;
/// age 20 releases the work and task. Nonzero room effect control pauses updates;
/// control 4 or above cancels and releases them. The room overlay and controller
/// must remain live while the task runs.
void shelterB1ControlRoomAccessTunnelRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs the tunnel's expanding orange disc, layered glow and fading ring.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`, initialized by `Gp_SpawnEff`, with task state zero.
/// `spawnArg1` is unused. Sizes are world units; the ring fades before the disc,
/// while the glow refreshes a flickering orange point light and a ground glow.
/// Nonzero room effect control pauses updates; control 4 or above cancels.
/// Completion and cancellation release the work and task. The room overlay and
/// controller must remain live while the task runs.
void shelterB1ControlRoomAccessTunnelRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_H
