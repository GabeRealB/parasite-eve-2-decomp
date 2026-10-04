#include "rooms/shelter_r49.h"

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"

#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

extern TaskMessageEntry D_shelter_r49_8017D9D8[];

extern EvsCommand D_actor_143900_80133560[];
extern EvsCommand D_80133860[];

static void func_shelter_r49_8017D648(Task* arg0);
static void func_shelter_r49_8017D6B4(Task* task);

/// The room task's states: set up, idle, then `taskKill`.
static const TaskFuncTable3 D_shelter_r49_8017D5C4 = {
    { func_shelter_r49_8017D648, func_shelter_r49_8017D6B4, taskKill },
};

s32 func_shelter_r49_8017D5EC(Task*, s32, s32, s32);
s32 func_shelter_r49_8017D5F4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_r49_8017D638(Task*, s32, s32, s32);
s32 func_shelter_r49_8017D640(Task*, s32, s32, s32);

TaskMessageEntry D_shelter_r49_8017D9D8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_r49_8017D5F4 },
    { 5105, func_shelter_r49_8017D5EC },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_r49_8017D640 },
    { ROOM_MESSAGE_COMMAND, func_shelter_r49_8017D638 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// The room's handler for message 0x13F1: does nothing and returns 0.
s32 func_shelter_r49_8017D5EC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming record onto the
/// outgoing one, passes both to `func_map_shelter_80179A04` and returns 1.
s32 func_shelter_r49_8017D5F4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

/// The room's handler for message 0x13F0: does nothing and returns 0.
s32 func_shelter_r49_8017D638(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// The room's handler for message 0x13EF: does nothing and returns 0.
s32 func_shelter_r49_8017D640(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// The room task's setup state: installs the room's message table, stores the
/// task in pointer slot 7 and, unless `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene` is 9, calls
/// `func_800E8634`.
static void func_shelter_r49_8017D648(Task* arg0)
{
    arg0->msgTable = D_shelter_r49_8017D9D8;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 9) {
        func_800E8634(D_actor_143900_80133560, 0, D_80133860);
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// The room task's idle state: does nothing. The unused local reproduces the
/// original's stack frame.
static void func_shelter_r49_8017D6B4(Task* task)
{
    char pad[0x10];
}

/// The room task: copies its three-state table to the stack and runs the
/// entry the task's state selects.
void func_shelter_r49_8017D6C4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_r49_8017D5C4;
    sp.funcs[task->state](task);
}
