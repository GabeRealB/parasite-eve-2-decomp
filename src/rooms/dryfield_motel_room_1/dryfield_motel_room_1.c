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

s32 func_dryfield_motel_room_1_8017D5EC(Task*, s32, s32, s32);
s32 func_dryfield_motel_room_1_8017D5F4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_motel_room_1_8017D61C(Task*, s32, s32, s32);
s32 func_dryfield_motel_room_1_8017D624(Task*, s32, RoomEventMsg*, RoomEventMsg*);

TaskMessageEntry D_dryfield_motel_room_1_8017E0A8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_motel_room_1_8017D5F4 },
    { 5105, func_dryfield_motel_room_1_8017D5EC },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_motel_room_1_8017D624 },
    { ROOM_MESSAGE_COMMAND, func_dryfield_motel_room_1_8017D61C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void func_dryfield_motel_room_1_8017D69C(Task* arg0);
static void func_dryfield_motel_room_1_8017D74C(Task* task);

s32 func_dryfield_motel_room_1_8017D5EC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Fallback entry of the room's message table: copies the incoming location
/// record onto the outgoing one and answers 1, leaving the decision to whoever
/// reads the reply.
s32 func_dryfield_motel_room_1_8017D5F4(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    return 1;
}

s32 func_dryfield_motel_room_1_8017D61C(Task* task, s32 msgId, s32 arg2, s32 arg3)
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
        Task_SpawnFromTable(&D_dryfield_motel_room_1_8017E478, 0, 0, 0);
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
/// Second state of the room entry task: nothing left to do but idle.
static void func_dryfield_motel_room_1_8017D74C(Task* task)
{
}

/// State handlers of the room entry task `func_dryfield_motel_room_1_8017D754`,
/// indexed by `Task::state`: the set-up tick, which advances the state, the
/// idle tick, and `taskKill`.
static const TaskFuncTable3 D_dryfield_motel_room_1_8017D5C4 = {
    { func_dryfield_motel_room_1_8017D69C, func_dryfield_motel_room_1_8017D74C, taskKill },
};

/// Room entry task: runs the state handler `D_dryfield_motel_room_1_8017D5C4`
/// names for `Task::state`, through a copy of the table taken onto the stack.
void func_dryfield_motel_room_1_8017D754(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_room_1_8017D5C4;
    sp.funcs[task->state](task);
}
