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

/// Number of mapped camera/image/sprite views in the pod access tunnel.
enum { SHELTER_B1_POD_ACCESS_TUNNEL_VIEW_COUNT = 14 };

/// The single room's logical-view map directory, borrowing `gViewIdentityMap`.
///
/// Room 1 maps logical views 1..14 to the same 1-based camera/image/sprite
/// indices. No terminator; the directory requires this room overlay to stay loaded.
extern u8* gShelterB1PodAccessTunnelRoomViewMaps[1];

extern ViewCount D_shelter_b1_pod_access_tunnel_80183A18[];

extern DirectionWarpEntry D_shelter_b1_pod_access_tunnel_80183A1C[];

/// Walls, floor and ceiling of the L-shaped tunnel, indexed by a 2x2 XZ grid.
///
/// Ten quads borrow six Q12 normals and twelve vertices in game units. The
/// grid starts at X=750, Z=-6500 with 4000-unit cells; class 2 is the floor,
/// class 0 the walls and ceiling. Activation binds the view coordinate.
/// The descriptor and its writable pools require this room overlay to stay loaded.
extern WorldCollisionGrid gShelterB1PodAccessTunnelCollisionGrid;

extern ViewCamera D_shelter_b1_pod_access_tunnel_80183C48[];

/// Sprite sources and batches for mapped views 1..14, with no clipping areas.
///
/// Indexed by mapped view minus one; only views 3 and 4 have source sprites.
/// Each batch list retains image strips through its zero-count first record
/// and ends at SPRITE_BATCH_END. Descriptors and their mutable arrays borrow
/// this room overlay's lifetime.
extern SpriteView gShelterB1PodAccessTunnelSpriteViews[SHELTER_B1_POD_ACCESS_TUNNEL_VIEW_COUNT];

/// The tunnel's model-shading lights: one white point light in every view.
///
/// No directional or cone lights. The point array stays writable for transform
/// composition and attenuation queries and requires this room overlay to stay loaded.
extern WorldCoordRoomLights gShelterB1PodAccessTunnelRoomLights;

extern WorldCollisionTrigger D_shelter_b1_pod_access_tunnel_8018474C[];

extern WorldCollisionOccluder D_shelter_b1_pod_access_tunnel_8018487C[];

extern WorldCollisionTrigger D_shelter_b1_pod_access_tunnel_801848B8[];

/// Collision and footstep policies indexed by room-local surface class 0..7.
///
/// Class 1 passes probes and suppresses weapon impacts; class 2 is the floor
/// with walk/run/jump cues. Other classes block probes, allow weapon impacts
/// and have no footstep cues. All classes apply pushback. No terminator;
/// records and sounds require this room overlay to stay loaded.
extern WorldCollisionSurfaceProperties* gShelterB1PodAccessTunnelSurfacePropertiesByClass[8];

/// Initializes the pod access tunnel's room controller and keeps it available for messages.
///
/// Start in state 0 with a bodyless task. Installs the room message handlers,
/// registers the room slot and starts the entry scene or post-gantry dialogue
/// selected by saved progress. State 1 idles; state 2 tears down the task.
/// States must be in 0..2 and this room overlay must remain loaded throughout.
void shelterB1PodAccessTunnelRoomTask(Task* task);

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

/// Runs this room's counted impact flash followed by smoke or orange rings and sparks.
///
/// Requires a coordinate body and owned zero-aged `EffectWork` in
/// `spawnArg2.pointer`, initialized by `effectSpawn`. Start in state 0;
/// nonzero `spawnArg1.value` selects smoke, zero selects rings and two sparks.
/// Active age seven enters release; the next active tick frees counted work
/// and kills the task. Nonzero room control pauses it; values at least 4 cancel
/// it. Child effects live independently. Requires current graphics workspace
/// and the room overlay to remain loaded until teardown.
void shelterB1PodAccessTunnelRoomVisualEffectsSparkBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_POD_ACCESS_TUNNEL_H
