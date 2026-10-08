#ifndef SRC_ROOMS_DRYFIELD_WAREHOUSE_DRYFIELD_WAREHOUSE_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_WAREHOUSE_DRYFIELD_WAREHOUSE_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"

#include "main/task_types.h"

/// The descriptors of the warehouse's cutscene tasks, spawned by index: the
/// cutscene task itself, the screen fade that ramps its channels up from 0, and
/// the one that ramps them down from 0xFF.
extern TaskDesc D_dryfield_warehouse_8017FB08[];

extern WorldCollisionTrigger D_dryfield_warehouse_801816A4[4];

extern WorldCollisionTrigger D_dryfield_warehouse_801817D4[13];

extern WorldCollisionTrigger D_dryfield_warehouse_80181BB0[10];

extern WorldCoordRoomLights D_dryfield_warehouse_801820E8[1];

extern Task* D_dryfield_warehouse_801821BC;

extern Task* D_dryfield_warehouse_801821C0;

extern s16 D_dryfield_warehouse_801821C4;

extern TaskMessageEntry D_dryfield_warehouse_8017F554[3];

extern TaskDesc D_dryfield_warehouse_8017F56C[2];

extern SpriteBatch D_dryfield_warehouse_801811A0[2];

extern SpriteSource D_dryfield_warehouse_801811B0[21];

extern SpriteBatch D_dryfield_warehouse_80181354[6];

extern SpriteSource D_dryfield_warehouse_80181384[25];

extern SpriteBatch D_dryfield_warehouse_80181578[6];

extern SpriteSource D_dryfield_warehouse_801815A8[2];

extern SpriteBatch D_dryfield_warehouse_801815D0[3];

extern SpriteBatch D_dryfield_warehouse_801815E8[2];

// Callbacks referenced by the overlay's shared data tables.
/// Adjusts warehouse ambience to the current view while room events are idle.
///
/// State 0 clears the requested-volume cache and enters state 1; other states
/// do nothing. State 1 requests 50%, 60% or 100% for views 2, 3 or 4, and silence
/// elsewhere or during an event. Changes queue a start, mix or 30-audio-update
/// stop; the cache records the request even when sound admission fails.
/// Requires a live task and loaded warehouse sound resources while playback runs.
void dryfieldWarehouseAmbienceTask(Task* task);

/// Starts the warehouse event when the Monkey Wrench is used at its active trigger.
///
/// `itemId` is a collected-item ID from `ROOM_MESSAGE_USE_KEY_ITEM`; the second
/// payload and other callback arguments are ignored. Scans the live trigger
/// list for a hit room-action trigger with the room-event sentinel. A match
/// sets the seen flag and session event state, queues the event task and returns
/// `ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE`, even if spawning fails. Other requests
/// return `ROOM_KEY_ITEM_USE_REFUSED`. No inventory item is removed here.
s32 dryfieldWarehouseUseKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);

/// Resolves warehouse departures, blocking the house route until the event is seen.
///
/// Borrows a readable eight-byte `request` and writable eight-byte `reply` for
/// synchronous `ROOM_EVENT_MESSAGE_RESOLVE`; they may alias. Copies the whole
/// record before checking it. The dilapidated-house route returns 1 once the
/// event is seen; before that it returns 0, and execution additionally starts
/// CAP command 3 and sets the optional map flag to 2. Other destinations return
/// 1 and execution requests a 15-audio-update ambience stop. Queries suppress
/// these effects. Neither pointer is retained and no destination is changed.
s32 dryfieldWarehouseResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);

/// Starts the warehouse event cutscene and waits for its requested exit.
///
/// States 0..2 start, wait and exit. State 0 disables display and publishes the
/// new cutscene task; state 1 polls its requested exit and ignores its result;
/// state 2 releases this gate. The cutscene owns display restoration and its
/// work. Requires a successful child spawn before polling and the room overlay
/// to remain loaded. This gate does not allocate work or adopt the child task.
void dryfieldWarehouseEventTask(Task* task);

#endif // SRC_ROOMS_DRYFIELD_WAREHOUSE_DRYFIELD_WAREHOUSE_PRIVATE_H
