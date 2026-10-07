#ifndef INCLUDE_ROOMS_SHELTER_B1_POD_ACCESS_TUNNEL_H
#define INCLUDE_ROOMS_SHELTER_B1_POD_ACCESS_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b1_pod_access_tunnel_80184C00[23];

// shelter_b1_pod_access_tunnel
extern u8* D_shelter_b1_pod_access_tunnel_80183A14[];

extern ViewCount D_shelter_b1_pod_access_tunnel_80183A18[];

extern DirectionWarpEntry D_shelter_b1_pod_access_tunnel_80183A1C[];

extern WorldCollisionGrid D_shelter_b1_pod_access_tunnel_80183C24;

extern ViewCamera D_shelter_b1_pod_access_tunnel_80183C48[];

extern SpriteView D_shelter_b1_pod_access_tunnel_8018462C[];

extern WorldCoordRoomLights D_shelter_b1_pod_access_tunnel_80184734;

extern WorldCollisionTrigger D_shelter_b1_pod_access_tunnel_8018474C[];

extern WorldCollisionOccluder D_shelter_b1_pod_access_tunnel_8018487C[];

extern WorldCollisionTrigger D_shelter_b1_pod_access_tunnel_801848B8[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_pod_access_tunnel_80184CDC[];

void func_shelter_b1_pod_access_tunnel_8017DEE8(Task* task);

/// Selects this room's combat effects once and draws the lights visible in the mapped view.
///
/// Start in state 0; initialization advances to state 1. Draws two white
/// capsule glows in views 2 and 4, four in views 3 and 7, and none elsewhere.
/// Radius is 384 world units; RGB bytes flicker between 16 and 24 on alternate
/// frames. Requires current view matrices, scratch stack, ordering table and
/// packet arena, and the room overlay must remain loaded.
void shelterB1PodAccessTunnelDrawGlowsTask(Task* task);

/// Runs this room's counted pink charge flash, peak screen tint and fading star.
///
/// Requires a coordinate body and owned `EffectWork` in `spawnArg2.pointer`,
/// initialized by `effectSpawn`. Start in state 0 with a positive charge
/// duration in ticks in `spawnArg1.value`; the task consumes that countdown.
/// Nonzero room effect control pauses it, and values at least 4 cancel it.
/// Completion or state 3 releases counted work and kills the task. Requires
/// current graphics workspace and the room overlay to remain loaded.
void shelterB1PodAccessTunnelRoomVisualEffectsFlashTask(Task* task);

/// Records and draws this room's counted pair of fading endpoint trails.
///
/// Requires a coordinate body and owned `EffectWork` in `spawnArg2.pointer`,
/// initialized by `effectSpawn`; its parent coordinate must remain live.
/// State 0 allocates two eight-frame histories in `Task::work` and seeds
/// parent-relative offsets (0, 190, -15) and (0, 1085, 180), in world units.
/// Allocation failure resets the age and retries on the next eligible tick.
/// State 1 records world-space snapshots and draws seven quads with R:G:B
/// multipliers 1:2:3. An exact nonzero age match to `spawnArg1.value`, in ticks,
/// releases both histories and counted work; zero never expires. Age is signed
/// 16-bit. Control values below 2 update, including pause value 1; values at
/// least 2 freeze it. Requires current graphics workspace and the room overlay
/// to remain loaded.
void shelterB1PodAccessTunnelRoomVisualEffectsTwinTrailTask(Task* task);

void func_shelter_b1_pod_access_tunnel_80180484(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_POD_ACCESS_TUNNEL_H
