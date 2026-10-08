#ifndef SRC_ROOMS_DRYFIELD_BREEZEWAY_DRYFIELD_BREEZEWAY_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_BREEZEWAY_DRYFIELD_BREEZEWAY_PRIVATE_H

#include "types.h"

#include "gameplay/gpu_image_upload.h"
#include "gameplay/message.h"

#include "main/task_types.h"

extern GpuImageUpload D_dryfield_breezeway_80183144[2];

extern SVECTOR D_dryfield_breezeway_80183164;

extern Task* D_dryfield_breezeway_801843C0;

extern TaskDesc gRoomEventTaskDesc;

extern TaskMessageEntry D_dryfield_breezeway_80181DE0[6];

extern TaskDesc D_dryfield_breezeway_80181E10[2];

extern TaskDesc D_dryfield_breezeway_801820B0[2];

extern TaskDesc D_dryfield_breezeway_80182E18;

void func_dryfield_breezeway_8017DC3C(Task* arg0);

// Callbacks referenced by the overlay's shared data tables.

/// Forwards a key-item-use query to the live model event, or refuses it while absent.
///
/// `messageId` is `ROOM_MESSAGE_USE_KEY_ITEM`; `itemId` is a collected-item id
/// and `secondArg` is forwarded unchanged (the item menu sends zero). Returns
/// the event's item-menu reply. The receiver handle is borrowed through dispatch.
s32 dryfieldBreezewayForwardKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 secondArg);

s32 func_dryfield_breezeway_8017D940(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_dryfield_breezeway_8017DA48(Task*, s32, s32, s32);

/// Starts breezeway sound-bank entry 7 for sound cue 7; other cues do nothing.
///
/// Handles `ROOM_MESSAGE_SOUND` and always returns zero. The sound's further
/// role is unproven; the task, message id and second payload are unused.
s32 dryfieldBreezewayHandleSoundMessage(Task* unusedTask, s32 unusedMessageId, s32 soundCue, s32 unusedSecondArg);

s32 func_dryfield_breezeway_8017DBD8(Task*, s32, RoomEventMsg*, RoomEventMsg*);

void func_dryfield_breezeway_8017DCE4(Task*);

#endif // SRC_ROOMS_DRYFIELD_BREEZEWAY_DRYFIELD_BREEZEWAY_PRIVATE_H
