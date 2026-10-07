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

/// Initializes the sterilization room and updates its event actor each tick.
///
/// The map's room-task descriptor supplies a live task starting at state 0.
/// Dispatch states are 0 initialize, 1 update and 2 teardown; other indices are
/// invalid. Initialization registers `GAME_TASK_SLOT_ROOM`, installs the room's
/// message handlers, selects entry events and rebuilds the actor obstacle.
/// Spawn arguments are unused; the task and room resources live with the overlay.
void shelterB1SterilizationRoomTask(Task* task);

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

/// Runs this room's telephone save menu and optional play-data statistics panels.
///
/// Gameplay selects this export for Mine/Shelter area 16. The live menu task
/// starts at state 0 and borrows a writable `UiObject` in `spawnArg2.pointer`
/// until the menu closes. Requires loaded telephone text/UI resources; save
/// and statistics dialogs are child tasks. Normal play before a clear enters
/// the save dialog directly; clear data or the statistics demo enables the list.
/// Completion reports the menu result through the object for its owner to close.
void shelterB1SterilizationRoomTelephoneMenuTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_STERILIZATION_ROOM_H
