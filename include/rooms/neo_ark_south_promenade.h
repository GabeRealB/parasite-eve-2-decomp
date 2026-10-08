#ifndef INCLUDE_ROOMS_NEO_ARK_SOUTH_PROMENADE_H
#define INCLUDE_ROOMS_NEO_ARK_SOUTH_PROMENADE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_neo_ark_south_promenade_801808E4[13];

// neo_ark_south_promenade
extern WorldCoordRoomLighting D_neo_ark_south_promenade_8017F6EC[];

extern WorldCollisionRoomResources D_neo_ark_south_promenade_8017F6F4[];

extern u8* D_neo_ark_south_promenade_8017F704[];

extern ViewCount D_neo_ark_south_promenade_8017F708[];

extern DirectionWarpEntry D_neo_ark_south_promenade_8017F70C[];

extern ViewCamera D_neo_ark_south_promenade_8017FDB0[];

extern SpriteView D_neo_ark_south_promenade_801803E4[];

extern WorldCollisionSurfaceProperties* D_neo_ark_south_promenade_801809AC[];

/// Records two offset endpoints of a moving anchor and draws their fading beam.
///
/// Requires a coordinate body, initialized room-effect state and an owned
/// `EffectWork` in `spawnArg2.pointer`; its borrowed parent must outlive the task.
/// `spawnArg1.value` is a lifetime compared with the wrapping signed 16-bit age
/// after initialization (0 disables timed teardown). Two eight-entry histories
/// belong to `Task::work` and are freed with the task. Control values below 2
/// advance the beam; higher values suspend it without cancelling it.
void neoArkSouthPromenadeRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs an impact flash followed by smoke puffs or expanding orange rings and sparks.
///
/// Starts in state 0 with a coordinate body and owned, zero-aged `EffectWork`
/// in `spawnArg2.pointer`. Nonzero `spawnArg1.value` selects smoke; zero selects
/// rings. Active age 7 enters release; age 8 releases work and the task.
/// Room effect control 1..3 pauses and 4 or above cancels. Spawned effects
/// remain independent. Keep this overlay and the effect controller loaded.
void neoArkSouthPromenadeRoomVisualEffectsSparkBurstTask(Task* task);

/// Registers this room's golem scream flash, sword trail and projectile burst once.
///
/// State 0 publishes the effect IDs and advances to state 1, which does no work.
/// Keep this room overlay loaded while the registered effect tasks can run.
void neoArkSouthPromenadeRegisterGolemEffectsTask(Task* task);

/// Runs the room's golem scream flash through charging, screen tint and star fade.
///
/// Requires a coordinate body, initialized room-effect state and an owned
/// `EffectWork` in `spawnArg2.pointer`. `spawnArg1.value` must start as a positive
/// charge duration in active ticks and is consumed as a countdown. Nonzero room
/// effect control pauses it; values >= 4 cancel it. State 3 releases the effect
/// and task on the next running tick.
void neoArkSouthPromenadeRoomVisualEffectsFlashTask(Task* task);

/// Runs the South Promenade room controller for synchronous room messages.
///
/// Requires a live task with state 0 (register handlers), 1 (idle), or 2 (kill).
/// Keep the room and Neo Ark map overlays loaded while its task and borrowed
/// message table remain available.
void neoArkSouthPromenadeRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SOUTH_PROMENADE_H
