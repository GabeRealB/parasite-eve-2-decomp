#ifndef INCLUDE_ROOMS_SHELTER_B1_STERILIZATION_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B1_STERILIZATION_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gShelterB1SterilizationRoomAcropolisSanctuaryModel090F0;

extern AreaVariant D_shelter_b1_sterilization_room_8018C14C[11];

// shelter_b1_sterilization_room
extern WorldCoordRoomLighting D_shelter_b1_sterilization_room_80189354[];

extern WorldCollisionRoomResources D_shelter_b1_sterilization_room_8018936C[];

extern u8* D_shelter_b1_sterilization_room_801893CC[];

extern ViewCount D_shelter_b1_sterilization_room_801893D8[];

extern DirectionWarpEntry D_shelter_b1_sterilization_room_801893E0[];

extern ViewCamera D_shelter_b1_sterilization_room_80189E68[];

extern SpriteView D_shelter_b1_sterilization_room_8018B00C[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_sterilization_room_8018C314[];

void func_shelter_b1_sterilization_room_80180518(Task* task);

void func_shelter_b1_sterilization_room_8018188C(Task* task);

/// Animates one drifting, expanding sterilization puff through ten sprite frames.
///
/// Bank-6 effect 0x17D requires its counted, zeroed `EffectWork` allocation in
/// `spawnArg2.pointer` and a coordinate body. The low 12 bits of `spawnArg1`
/// must be a source index 0..63; each group of 16 selects one of four Q12
/// directions. The signed high halfword biases the initial perspective size.
/// `index` holds ticks per sprite frame (1..4), `period` the initial velocity
/// magnitude, and `step` the size growth per tick. Movement, size and age pause
/// while room effects are paused; drawing continues. Initialization while paused
/// leaves age zero, drawing signed frame -1 when the cadence is one tick.
/// Releases the work and task after the last frame.
void shelterB1SterilizationRoomPuffTask(Task* task);

void func_shelter_b1_sterilization_room_8017EB2C(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_STERILIZATION_ROOM_H
