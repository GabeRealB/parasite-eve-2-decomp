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

/// Draws view-dependent sterilization glows and emits smoke or trap puffs.
///
/// Gameplay effect 0x12F requires a live coordinate body. State 0 draws glows
/// and emits smoke in mapped views 20..24; mapped view 14 permanently enters
/// the trap-puff phase. A nonzero spawnArg1 arms one cancellation in view 6,
/// and the smoke views re-arm it. Other spawn arguments and Task::work are unused.
/// New particles require running room effects; glows continue while paused.
///
/// Puff sources are indices 0..63, grouped in four Q12 directions. Their packed
/// high halfword adjusts perspective size by -128, 96, 256 or 384 by view.
/// Smoke points occupy writable entries 81/82 of the room's 87-vector table;
/// both use the same random angle (4096 units/turn) and radius 256..767 game
/// units in the task coordinate's local space. Spawns copy their placement.
/// Requires loaded room resources, composed view transforms, a scratch stack
/// and the frame's GPU arena/table. It never retires itself; room teardown owns
/// its lifetime and releases the unused counted effect work through its exit callback.
void shelterB1SterilizationRoomEffectsTask(Task* task);

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
