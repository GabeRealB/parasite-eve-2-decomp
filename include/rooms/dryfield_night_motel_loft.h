#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern AreaVariant D_dryfield_night_motel_loft_80180888[13];

extern TmdSource gDryfieldNightMotelLoftActor135400Model071AC;

// dryfield_night_motel_loft
extern WorldCoordRoomLighting D_dryfield_night_motel_loft_8017EDB0[];

extern WorldCollisionRoomResources D_dryfield_night_motel_loft_8017EDC0[];

extern u8* D_dryfield_night_motel_loft_8017EDF0[];

extern ViewCount D_dryfield_night_motel_loft_8017EDF8[];

extern DirectionWarpEntry D_dryfield_night_motel_loft_8017EDFC[];

extern ViewCamera D_dryfield_night_motel_loft_8017F144[];

extern SpriteView D_dryfield_night_motel_loft_8017FBE4[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_motel_loft_8018090C[];

/// Updates a tumbling grey shard that bounces on Y=0 and fades after its first bounce.
///
/// Handles `EFFECT_NIGHT_MOTEL_LOFT_FALLING_SHARD`. The effect spawner supplies
/// an owned `EffectWork` in `spawnArg2.pointer` and a coordinate body already
/// positioned at the spawn offset. The low twelve bits of `spawnArg1.value`
/// give the radius in coordinate units; the room spawns radii 16..79.
/// Positive local Y falls toward the floor. Each crossing rolls back the
/// movement and halves velocity, reflecting Y; subsequent bounces continue
/// during the fade. Drawing uses the coordinate's existing cached transform.
/// Control values 0 and 1 advance the task, 2 and 3 suppress update and draw,
/// and values >=4 release both the counted effect work and the task.
void dryfieldNightMotelLoftFallingShardTask(Task* task);

void func_dryfield_night_motel_loft_8017DB64(Task* arg0);

void func_dryfield_night_motel_loft_8017D964(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_H
