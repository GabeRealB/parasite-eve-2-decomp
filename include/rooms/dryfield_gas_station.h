#ifndef INCLUDE_ROOMS_DRYFIELD_GAS_STATION_H
#define INCLUDE_ROOMS_DRYFIELD_GAS_STATION_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_gas_station_80184A38[12];

// dryfield_gas_station
extern WorldCollisionRoomResources D_dryfield_gas_station_8018314C[];

extern u8* D_dryfield_gas_station_8018315C[];

extern WorldCoordRoomLighting D_dryfield_gas_station_80183160[];

extern ViewCount D_dryfield_gas_station_80183168[];

extern DirectionWarpEntry D_dryfield_gas_station_8018316C[];

extern ViewCamera D_dryfield_gas_station_80183EC8[];

extern SpriteView D_dryfield_gas_station_801842A8[];

extern WorldCollisionSurfaceProperties* D_dryfield_gas_station_80184BAC[];

/// Draws the gas station's pulsing cyan glow and permits footstep dust.
///
/// Bank-6 effect 0xC2 requires a live `TASK_BODY_COORD` task and the gas-station
/// overlay. The current logical view must be 1..14: views 4, 6, 11 and 12 use
/// a diamond with crossing rays; the others use a disc with four rays.
/// Both shapes pulse at 96 angle units per animation frame (4096 per turn),
/// with a nominal radius of 4096 / (camera Z / 4) pixels. Depth below 17 emits
/// no glow, but still permits dust.
///
/// Requires live `gRoomEffectState`, initialized view matrices and scratch
/// stack, and a current frame arena/ordering table with room for up to twenty
/// additive quads and their blend commands. Borrows the task's coordinate for
/// the call; the drawers update its composed transform. Allocates no task work.
void dryfieldGasStationCyanGlowTask(Task* task);

void func_dryfield_gas_station_8017EA90(Task* task);

/// Runs the gas station's room receiver through entry setup, idle and teardown.
///
/// Requires a live bodyless task with state 0 (entry), 1 (idle) or 2 (kill).
/// Entry registers `GAME_TASK_SLOT_ROOM`, starts the arrival scene or area music,
/// and permits post-CAP sound messages. Keep the gas-station overlay loaded
/// for the task's lifetime.
void dryfieldGasStationRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_GAS_STATION_H
