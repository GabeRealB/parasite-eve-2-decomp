#ifndef SRC_ROOMS_SHELTER_B1_STERILIZATION_ROOM_SHELTER_B1_STERILIZATION_ROOM_PRIVATE_H
#define SRC_ROOMS_SHELTER_B1_STERILIZATION_ROOM_SHELTER_B1_STERILIZATION_ROOM_PRIVATE_H

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

#include "rooms/room.h"

/// The room's task descriptor table; its spawners pick an entry by index.
extern TaskDesc D_shelter_b1_sterilization_room_80188504[];

/// One bit per entry of `D_shelter_b1_sterilization_room_80188504` already
/// spawned, so each is spawned only once until the mask is cleared.
extern s32 D_shelter_b1_sterilization_room_8018C340;

extern AnimationBankCopyRequest D_shelter_b1_sterilization_room_80188590;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_801885AC;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_801885C0;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_801885D4;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_801885E8;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_801885FC;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_80188610;

extern ActorTransform D_shelter_b1_sterilization_room_80188638;

extern ActorTransform D_shelter_b1_sterilization_room_80188650;

extern DamageAttack D_shelter_b1_sterilization_room_80188738;

extern EvsCommand D_shelter_b1_sterilization_room_8018873C[37];

extern EvsCommand D_shelter_b1_sterilization_room_80188AB4[20];

extern EvsCommand D_shelter_b1_sterilization_room_80188ED4[11];

extern EvsCommand D_shelter_b1_sterilization_room_80188FDC[8];

extern WorldCollisionGrid D_shelter_b1_sterilization_room_80189E44;

extern WorldCollisionTrigger D_shelter_b1_sterilization_room_8018B8A8[28];

extern AreaApplyRec D_shelter_b1_sterilization_room_8018C334[2];

extern Task* gRoomCutsceneSoundTask;

extern RoomCutsceneRec D_shelter_b1_sterilization_room_8018C344;

// Callbacks referenced by the overlay's shared data tables.
void func_shelter_b1_sterilization_room_8018118C(s32);

void func_shelter_b1_sterilization_room_801813A0(Task*);

/// Selects room 2 after requesting a view refresh on the preceding task tick.
///
/// Updates both the live save and session room selectors, requests room-object
/// relinking, then releases the task on its next tick. Spawn arguments are unused.
void shelterB1SterilizationRoomSwitchRoomTask(Task* task);

void func_shelter_b1_sterilization_room_80181588(Task*);

void func_shelter_b1_sterilization_room_80181634(Task*);

void func_shelter_b1_sterilization_room_801816E0(Task*);

void func_shelter_b1_sterilization_room_801817EC(Task*);

#endif // SRC_ROOMS_SHELTER_B1_STERILIZATION_ROOM_SHELTER_B1_STERILIZATION_ROOM_PRIVATE_H
