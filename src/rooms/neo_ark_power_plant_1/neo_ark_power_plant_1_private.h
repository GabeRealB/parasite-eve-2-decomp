#ifndef SRC_ROOMS_NEO_ARK_POWER_PLANT_1_NEO_ARK_POWER_PLANT_1_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_POWER_PLANT_1_NEO_ARK_POWER_PLANT_1_PRIVATE_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

#include "main/task_types.h"

extern TaskMessageEntry D_neo_ark_power_plant_1_8017EB18[5];

extern EvsCommand D_neo_ark_power_plant_1_8017EB7C[24];

extern EvsCommand D_neo_ark_power_plant_1_8017EDBC[10];

extern EvsCommand D_neo_ark_power_plant_1_8017EEE4[13];

extern s32 D_neo_ark_power_plant_1_8017F01C;

extern WorldCollisionFootstepSounds D_neo_ark_power_plant_1_80181B9C;

extern WorldCollisionFootstepSounds D_neo_ark_power_plant_1_80181BA8;

// Callbacks referenced by the overlay's shared data tables.

/// Refuses key-item use in Power Plant 1 without consuming the item or starting an event.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are ignored. Returns
/// `ROOM_KEY_ITEM_USE_REFUSED` for the item menu's unavailable-use notice.
s32 neoArkPowerPlant1RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);

/// Resolves a Neo Ark destination room for a Power Plant 1 transition request.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`. Copies the complete eight-byte request
/// into writable reply storage before resolving its room from game progress.
/// Query mode preserves the copied record. The pointers may alias and are
/// borrowed only for this call; the Neo Ark map overlay must be loaded.
/// Always returns 1 to allow the transition; task and message ID are unused.
s32 neoArkPowerPlant1ResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);

s32 func_neo_ark_power_plant_1_8017D7F8(Task*, s32, s32, s32);

/// Ignores trigger-driven room actions in Power Plant 1 and returns zero.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`. The borrowed request is neither
/// read nor retained; the zero second payload and receiver are also unused.
s32 neoArkPowerPlant1IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);

/// Cancels active effects and blocks new ability requests for the generator-clear scene.
///
/// Called at the scene script's start with live room-effect and attachment state.
/// Cancellation is deferred to the effect update; the attachment event lock
/// remains set for the ordinary attachment controller to release.
void neoArkPowerPlant1PrepareGeneratorClearScene(void);

/// Stops scene vibration when the generator-clear event is skipped.
///
/// Requests vibration-script and motor-task teardown and clears port 0's pending
/// vibration requests. Requires a live game session; teardown can be deferred
/// while actor updates are frozen.
void neoArkPowerPlant1StopSkippedSceneVibration(void);

#endif // SRC_ROOMS_NEO_ARK_POWER_PLANT_1_NEO_ARK_POWER_PLANT_1_PRIVATE_H
