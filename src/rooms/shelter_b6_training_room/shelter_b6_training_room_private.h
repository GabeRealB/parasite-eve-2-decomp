#ifndef SRC_ROOMS_SHELTER_B6_TRAINING_ROOM_SHELTER_B6_TRAINING_ROOM_PRIVATE_H
#define SRC_ROOMS_SHELTER_B6_TRAINING_ROOM_SHELTER_B6_TRAINING_ROOM_PRIVATE_H

#include "types.h"

#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/coord.h"
#include "main/task_types.h"

extern u8 D_shelter_b6_training_room_80185C60[3][16];

extern GfxCoord* D_shelter_b6_training_room_80185C90;

extern GfxCoord* D_shelter_b6_training_room_80185C94;

extern u16 D_shelter_b6_training_room_80185C98;

extern TaskMessageEntry D_shelter_b6_training_room_80182AF4[6];

extern s32 D_shelter_b6_training_room_80182B24;

extern TaskDesc D_shelter_b6_training_room_801839A8;

extern EvsCommand D_shelter_b6_training_room_80183BB4[58];

extern EvsCommand D_shelter_b6_training_room_80184124[14];

extern EvsCommand D_shelter_b6_training_room_80184274[7];

extern TaskDesc D_shelter_b6_training_room_8018431C[2];

/// Refuses every selected key item without changing inventory or starting a scene.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM` and returns `ROOM_KEY_ITEM_USE_REFUSED`.
/// The selected collected-item `itemId` and all other callback arguments are unused.
s32 shelterB6TrainingRoomRefuseKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);

/// Permits a room transition after resolving its destination from Neo Ark progress.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`: copies the complete eight-byte request
/// to the reply, then resolves its room on execution. Queries echo the request.
/// Both records must be live and two-byte-aligned, and the reply writable;
/// they may alias. Retains neither pointer and always returns 1 (allowed).
/// The receiver and message ID are unused callback arguments.
s32 shelterB6TrainingRoomResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);

s32 func_shelter_b6_training_room_8017D684(Task*, s32, s32, s32);

/// Ignores trigger requests delivered through `DIRECTION_MESSAGE_ROOM_ACTION`.
///
/// Returns zero without reading either payload word or changing room state.
/// All callback arguments are unused.
s32 shelterB6TrainingRoomIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

/// Starts the skippable defeat scene and latches its pending departure.
///
/// Handles `ROOM_MESSAGE_ACTOR_EVENT` from the defeated enemy. Marks the weapon
/// for re-equipping, starts the normal/skip script pair with HUD hide/restore,
/// and sends the scene command to placed actor 3, which must be live.
/// Payload words and receiver are unused; returns zero. Script and actor-command
/// storage remain borrowed from the loaded room overlay.
s32 shelterB6TrainingRoomStartDefeatScene(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

/// Starts and tracks the player head-aim controller for the defeat scene.
///
/// The script calls this once before fading head aim in. Publishes the spawned
/// priority-96, zero-body task handle; allocation failure publishes NULL.
/// Repeated calls replace the handle without releasing its previous task.
/// The player, target actor and room resources must stay live through head aim;
/// `shelterB6TrainingRoomControlPlayerHeadAim` fades or releases the controller.
void shelterB6TrainingRoomStartPlayerHeadAim(void);

/// Script commands for fading player head aim or releasing its controller task.
enum {
    SHELTER_B6_TRAINING_ROOM_HEAD_AIM_RELEASE  = -1,
    SHELTER_B6_TRAINING_ROOM_HEAD_AIM_FADE_OUT = 0,
    SHELTER_B6_TRAINING_ROOM_HEAD_AIM_FADE_IN  = 1
};

/// Fades player head aim in or out, or releases the tracked head-aim task.
///
/// Commands 0 and 1 set the existing task's fade direction; every other word
/// kills it and clears its borrowed handle. Does nothing when no task is tracked.
/// Does not spawn a task or reset its current interpolation weight.
void shelterB6TrainingRoomControlPlayerHeadAim(s32 command);

/// Blends the player's head toward placed actor 1 during the defeat scene.
///
/// State 0 uses `killCountdown` as a signed 1/4096 weight, stepping by 256
/// toward 0 for a zero `spawnArg1.value`, or toward 4096 otherwise. Halfword
/// truncation precedes signed clamping; normal operation keeps the weight in
/// 0..4096. Limits yaw to 512 and pitch to 256 angle units (4096 per turn),
/// subject to the animation helper's existing-pose limits.
/// Requires the player and target to own live TMD models with at least five
/// coordinates in the expected root-to-head order. Borrows them for each call.
/// Holds while the event-script run gate is closed; any nonzero task state
/// kills the controller once that gate opens. Allocates no work or model body.
void shelterB6TrainingRoomPlayerHeadAimTask(Task* task);

void func_shelter_b6_training_room_8017DAC8(void);

/// Requests weapon re-equipping and optionally replaces the battle-end hold.
///
/// A zero `endDelayFrames` preserves the existing delay. Every nonzero word
/// stores its low eight bits as remaining actor-update frames; the scripts
/// use 100 normally and 8 when skipped. Does not change the battle phase.
void shelterB6TrainingRoomPrepareDefeatScene(s32 endDelayFrames);

void func_shelter_b6_training_room_8017DB28(void);

/// Locks attachment changes and stops battle effects and non-ambient sound scripts.
///
/// Shared by the normal and skipped defeat scenes. Requests cancellation of
/// all room effects and sound-script stops that retain release behavior.
/// Ambient scripts and the other attachment flags are preserved.
void shelterB6TrainingRoomStopBattlePresentation(void);

void func_shelter_b6_training_room_8017DD98(Task*);

#endif // SRC_ROOMS_SHELTER_B6_TRAINING_ROOM_SHELTER_B6_TRAINING_ROOM_PRIVATE_H
