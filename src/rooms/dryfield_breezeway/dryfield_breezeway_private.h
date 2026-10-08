#ifndef SRC_ROOMS_DRYFIELD_BREEZEWAY_DRYFIELD_BREEZEWAY_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_BREEZEWAY_DRYFIELD_BREEZEWAY_PRIVATE_H

#include "types.h"

#include "gameplay/gpu_image_upload.h"
#include "gameplay/message.h"
#include "gameplay/direction.h"

#include "main/task_types.h"

extern GpuImageUpload D_dryfield_breezeway_80183144[2];

extern SVECTOR D_dryfield_breezeway_80183164;

extern Task* D_dryfield_breezeway_801843C0;

extern TaskDesc gRoomEventTaskDesc;

extern TaskMessageEntry D_dryfield_breezeway_80181DE0[6];

extern TaskDesc D_dryfield_breezeway_80181E10[2];

extern TaskDesc D_dryfield_breezeway_801820B0[2];

extern TaskDesc D_dryfield_breezeway_80182E18;

/// Holds and hides the player while the bottlecap-magnet interaction runs.
///
/// Starts the model event in state 0 and publishes its borrowed handle for
/// key-item messages. State 1 polls the child's requested exit, clears the
/// handle and ends. Requires a successful spawn and loaded room resources;
/// the child restores player presentation. Its exit result is unused.
void dryfieldBreezewayKeyItemSessionTask(Task* task);

// Callbacks referenced by the overlay's shared data tables.

/// Forwards a key-item-use query to the live model event, or refuses it while absent.
///
/// `messageId` is `ROOM_MESSAGE_USE_KEY_ITEM`; `itemId` is a collected-item id
/// and `secondArg` is forwarded unchanged (the item menu sends zero). Returns
/// the event's item-menu reply. The receiver handle is borrowed through dispatch.
s32 dryfieldBreezewayForwardKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 secondArg);

/// Resolves factory room selection and the factory-key door event.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`, borrowing complete eight-byte request
/// and reply records, which may alias. Copies the request; factory execution
/// selects room 1/2 from the barrier flag, then queries or starts the key gate.
/// Returns the gate's 0/1/2 result, or 1 for other areas. A latched start freezes
/// door progress at 4 and records objective 0x38. The request is not retained;
/// the event gate copies the records needed by its deferred task.
s32 dryfieldBreezewayResolveRoomEventMessage(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);

/// Routes factory-door dialogue and the bottlecap-magnet interaction from CAP.
///
/// `ROOM_MESSAGE_COMMAND` uses integer command 1 for dialogue and 3 for the
/// key-item session. Both refuse interaction during battle; command 3 also
/// requires door progress >=2 and available room object 6. Dialogue records
/// inventory-dependent progress and holds player control until its task ends.
/// The second word is ignored; every path returns zero.
s32 dryfieldBreezewayHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);

/// Starts breezeway sound-bank entry 7 for sound cue 7; other cues do nothing.
///
/// Handles `ROOM_MESSAGE_SOUND` and always returns zero. The sound's further
/// role is unproven; the task, message id and second payload are unused.
s32 dryfieldBreezewayHandleSoundMessage(Task* unusedTask, s32 unusedMessageId, s32 soundCue, s32 unusedSecondArg);

/// Latches and starts the first encounter once directed room action 1 arrives.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows the four-byte request for this call;
/// only its unsigned action byte is read, after the unseen-encounter flag test.
/// The zero second word is ignored. The flag is latched before spawning with
/// no rollback on allocation failure. Always returns zero.
s32 dryfieldBreezewayHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Runs factory-door dialogue and commits its progress before resuming the player.
///
/// `spawnArg1.value` is a loaded CAP command index (the room sends 1). State 0
/// starts in-place playback; state 1 waits for CAP to become idle, applies
/// variant 11 and missing-magnet progress unless unlocked, then resumes player
/// control and ends. Requires the room and CAP resources through playback.
void dryfieldBreezewayFactoryDoorDialogueTask(Task* task);

#endif // SRC_ROOMS_DRYFIELD_BREEZEWAY_DRYFIELD_BREEZEWAY_PRIVATE_H
