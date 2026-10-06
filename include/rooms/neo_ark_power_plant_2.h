#ifndef INCLUDE_ROOMS_NEO_ARK_POWER_PLANT_2_H
#define INCLUDE_ROOMS_NEO_ARK_POWER_PLANT_2_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_neo_ark_power_plant_2_80182DE0[19];

// neo_ark_power_plant_2
extern WorldCollisionRoomResources D_neo_ark_power_plant_2_80180690[];

extern WorldCoordRoomLighting D_neo_ark_power_plant_2_801806A0[];

extern u8* D_neo_ark_power_plant_2_801806A8[];

extern ViewCount D_neo_ark_power_plant_2_801806AC[];

extern DirectionWarpEntry D_neo_ark_power_plant_2_801806B0[];

extern ViewCamera D_neo_ark_power_plant_2_80180DE8[];

extern SpriteView D_neo_ark_power_plant_2_8018205C[];

extern WorldCollisionSurfaceProperties* D_neo_ark_power_plant_2_80182F50[];

/// Sets visibility of view 6's five-sprite batch for the generator Life Support task.
///
/// `hidden` is 0 to show or 1 to hide; other byte values leave it unchanged.
/// The active session must select this room's sprite tables, with its map and
/// room overlays loaded. Updates view 6 regardless of the current view.
void neoArkPowerPlant2SetView6SpritesHidden(u8 hidden);

/// Runs this room's charging pink flash, peak screen tint and fading star.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `Gp_SpawnEff`. `spawnArg1.value` is a positive
/// charge duration in active ticks, consumed as a countdown. Nonzero room
/// effect control pauses it; control 4 or above, state 3, or completion
/// releases the work and task. The effect controller and room overlay must
/// remain live until teardown.
void neoArkPowerPlant2RoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws their fading trail as a blue beam.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `Gp_SpawnEff`; its parent coordinate must stay live.
/// Owns two eight-coordinate histories in `Task::work`, freed by task teardown.
/// Allocation failure resets age to zero for retry. Initialization is the first
/// active tick; later ticks draw seven quads tinted 1:2:3 in R:G:B.
/// `spawnArg1.value` is 0 for external teardown or 2..32767 for a first-cycle
/// active-age limit. Room effect control at two or above pauses recording and
/// drawing without cancellation. Requires a live effect controller and room overlay.
void neoArkPowerPlant2RoomVisualEffectsTwinTrailTask(Task* task);

void func_neo_ark_power_plant_2_8017F140(Task* task);

void func_neo_ark_power_plant_2_8017D8AC(Task* arg0);

void func_neo_ark_power_plant_2_8017D854(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_POWER_PLANT_2_H
