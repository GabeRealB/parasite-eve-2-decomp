#ifndef SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_DRYFIELD_NIGHT_MOTEL_BALCONY_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_DRYFIELD_NIGHT_MOTEL_BALCONY_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

extern TaskDesc gRoomEventTaskDesc;

extern TaskMessageEntry D_dryfield_night_motel_balcony_80182804[6];

extern SVECTOR D_dryfield_night_motel_balcony_80182C60[2];

extern SVECTOR D_dryfield_night_motel_balcony_80182C70;

extern SVECTOR D_dryfield_night_motel_balcony_80182C80;

extern SVECTOR D_dryfield_night_motel_balcony_80182C90;

// Callbacks referenced by the overlay's shared data tables.

/// Resolves balcony departures and starts the motel doors' unlock events.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; `task` and `messageId` are unused.
/// Borrows a complete request and writable reply, which may alias. Copies the
/// request and resolves room 5, parking lot and loft only on execution.
/// Unlocking room 5 and loft requires the Bronco masterkey; room 6 uses its own key.
/// A started masterkey event retires the individual keys; a started room-6
/// event arms its scene and objective. Queries leave those effects untouched.
/// The gate resets its event-start indication even on queries.
/// Returns 1 for ordinary departure and 2 for an eligible or refused door event.
s32 roomVariantMotelBalconyDoorsMsg(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);

/// Queues the balcony sound script selected by CAP cue 8 or 9.
///
/// Handles `ROOM_MESSAGE_SOUND`; other cue keys do nothing. The current
/// session supplies the stage for balcony-bank entries 8 and 9. Returns 0;
/// `task`, `messageId` and `secondArg` are unused. Requires gameplay and the
/// loaded balcony sound bank; queue admission follows the sound request API.
s32 motelBalconyCueSoundMsg(Task* task, s32 messageId, s32 cueKey, s32 secondArg);

s32 func_dryfield_night_motel_balcony_8017DC18(Task*, s32, s32, s32);

s32 func_dryfield_night_motel_balcony_8017DC20(Task*, s32, s32, s32);

s32 func_dryfield_night_motel_balcony_8017DC28(Task*, s32, s32, s32);

void func_dryfield_night_motel_balcony_8017E0C8(Task*);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_DRYFIELD_NIGHT_MOTEL_BALCONY_PRIVATE_H
