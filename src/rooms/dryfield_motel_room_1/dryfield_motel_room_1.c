#include "rooms/dryfield_motel_room_1.h"

#include "types.h"

#include "dryfield_motel_room_1_private.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

extern TaskMessageEntry D_dryfield_motel_room_1_8017E0A8[];

static s32 _dryfieldMotelRoom1RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldMotelRoom1ResolveTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldMotelRoom1IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedCommandArg);
s32        func_dryfield_motel_room_1_8017D624(Task*, s32, RoomEventMsg*, RoomEventMsg*);

TaskMessageEntry D_dryfield_motel_room_1_8017E0A8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldMotelRoom1ResolveTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldMotelRoom1RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_motel_room_1_8017D624 },
    { ROOM_MESSAGE_COMMAND, _dryfieldMotelRoom1IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void func_dryfield_motel_room_1_8017D69C(Task* arg0);
static void _dryfieldMotelRoom1IdleRoomTask(Task* unusedTask);

/// Refuses every request to use a collected key item in motel room 1.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`. Ignores both integer payload words
/// and the receiver; returns `ROOM_KEY_ITEM_USE_REFUSED` without consuming an item.
static s32 _dryfieldMotelRoom1RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Allows a room transition without changing its requested destination.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`. Borrows an eight-byte readable request
/// and writable reply for synchronous dispatch; both may be the same record.
/// Copies the complete request and returns 1 to permit the ordinary transition.
/// Queries and execution requests have the same result and no room side effects.
static s32 _dryfieldMotelRoom1ResolveTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_MOTEL_ROOM_1_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_MOTEL_ROOM_1_TRANSITION_ALLOWED;
}

/// Ignores every CAP room command and returns zero.
///
/// Handles `ROOM_MESSAGE_COMMAND`; neither integer payload word nor the receiver
/// is accessed, and no command is deferred.
static s32 _dryfieldMotelRoom1IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedCommandArg)
{
    return 0;
}
/// Message gate for the room's hotspot registered under id 0x13EF - the id the
/// sanctuary's own gate uses. On the phase-3 visit (`gGameSession::location.loc.variant`)
/// whose sub-id (`RoomEventMsg::field_2`) is 1 and that has not yet latched
/// nibble 0x5C, it arms the room's script task and latches the nibble. The
/// record is not copied to the outgoing one: this handler only ever consumes
/// the message (returns 0).
///
/// GCC hoists the `gGameSession` load above the `addiu $sp` prologue, which is
/// why the function starts two instructions before its frame setup.
s32 func_dryfield_motel_room_1_8017D624(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if (gGameSession->location.loc.variant == 3 && gameFlagGetNibble(GAME_FLAG_MOTEL_ROOM_1_EVENT_SEEN) == 0 && in->warp == 1) {
        gameFlagSetNibble(GAME_FLAG_MOTEL_ROOM_1_EVENT_SEEN, 1);
        taskSpawnFromTable(&D_dryfield_motel_room_1_8017E478, 0, 0, 0);
    }
    return 0;
}

/// Room entry task tick: park the room's hotspot table in `Task::msgTable` -
/// the table whose 0x13EF entry is the gate `func_dryfield_motel_room_1_8017D624`
/// matches - register the task in pointer slot 7, then, on the phase-3 visit
/// whose nibble 0x5C is still clear, announce the room to the slot-4 task with
/// message 0x7DA carrying the session's two id bytes and a zero halfword. Then
/// advance state.
static void func_dryfield_motel_room_1_8017D69C(Task* arg0)
{
    ActorCommand msg;

    arg0->msgTable = D_dryfield_motel_room_1_8017E0A8;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == 3 && gameFlagGetNibble(GAME_FLAG_MOTEL_ROOM_1_EVENT_SEEN) == 0) {
        msg.context.loc.stage = gGameSession->location.loc.stage;
        msg.context.loc.area  = gGameSession->location.loc.area;
        msg.command           = 0;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
    }
    arg0->state = arg0->state + 1;
}
/// Keeps the initialized motel room 1 task idle in state 1.
///
/// Ignores its argument; messages continue through the installed room table.
static void _dryfieldMotelRoom1IdleRoomTask(Task* unusedTask)
{
}

/// State handlers of the room entry task `func_dryfield_motel_room_1_8017D754`,
/// indexed by `Task::state`: the set-up tick, which advances the state, the
/// idle tick, and `taskKill`.
static const TaskFuncTable3 D_dryfield_motel_room_1_8017D5C4 = {
    { func_dryfield_motel_room_1_8017D69C, _dryfieldMotelRoom1IdleRoomTask, taskKill },
};

/// Room entry task: runs the state handler `D_dryfield_motel_room_1_8017D5C4`
/// names for `Task::state`, through a copy of the table taken onto the stack.
void func_dryfield_motel_room_1_8017D754(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_room_1_8017D5C4;
    sp.funcs[task->state](task);
}
