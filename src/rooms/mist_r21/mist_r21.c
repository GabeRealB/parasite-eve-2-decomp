#include "rooms/mist_r21.h"

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/fs.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

/// The room's message table, published at `Task::msgTable` by the room task.
s32  func_mist_r21_8017D5DC(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3);
s32  func_mist_r21_8017D5E4(Task* task, s32 msgId, TaskMessageArg requestArg, TaskMessageArg replyArg);
s32  func_mist_r21_8017D60C(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3);
s32  func_mist_r21_8017D614(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3);
void func_mist_r21_8017D760(Task* task);

GpMsgEntry D_mist_r21_8017D770[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_mist_r21_8017D5E4 },
    { 0x13F1, func_mist_r21_8017D5DC },
    { 0x13EF, func_mist_r21_8017D614 },
    { 0x13F0, func_mist_r21_8017D60C },
    { 0x7FFFFFFF, NULL },
};

/// The one task the room task spawns on entry; its callback is the empty
/// `func_mist_r21_8017D760`.
TaskDesc D_mist_r21_8017D798[] = {
    { 0, 0xC0, func_mist_r21_8017D760, { .value = 0 } },
};

static void func_mist_r21_8017D61C(Task* task);
static void func_mist_r21_8017D678(Task* task);

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_mist_r21_8017D5DC(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EE: copies the location record it is given
/// onto the reply record unchanged and answers 1.
s32 func_mist_r21_8017D5E4(Task* task, s32 msgId, TaskMessageArg requestArg, TaskMessageArg replyArg)
{
    RoomEventMsg* src = requestArg.roomEvent;
    RoomEventMsg* dst = replyArg.roomEvent;

    *dst = *src;
    return 1;
}

/// Message-table handler for id 0x13F0: accepts the message and does nothing.
s32 func_mist_r21_8017D60C(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_mist_r21_8017D614(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7, spawns the task `D_mist_r21_8017D798` describes and
/// advances to the next state.
static void func_mist_r21_8017D61C(Task* task)
{
    task->msgTable = D_mist_r21_8017D770;
    Game_SetPtrSlot(task, 7);
    Task_SpawnFromTable(D_mist_r21_8017D798, 0, 0, 0);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: waits for pad 0 to report button 0x200 in
/// mode 0 and button 0x40 in mode 1, then sets the saved location to area 5,
/// warp 1, view 2, starts loading from it, spawns task 0x11 and ends itself.
static void func_mist_r21_8017D678(Task* task)
{
    if ((Pad_CheckButtons(0, 0, 0x200) != 0) && (Pad_CheckButtons(0, 1, 0x40) != 0)) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = 5;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 1;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 2;
        Fs_BeginBootLoad((u8*)&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, 0);
        Task_Spawn(0, 0x11, 1, 0);
        taskKill(task);
    }
}

/// The room task's three states.
static const TaskFuncTable3 D_mist_r21_8017D5C4 = {
    { func_mist_r21_8017D61C, func_mist_r21_8017D678, taskKill },
};

/// `"target set\n"`: no code in the room reads it.
static const char D_mist_r21_8017D5D0[] = "target set\n";

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_mist_r21_8017D5C4`.
void func_mist_r21_8017D708(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_r21_8017D5C4;
    sp.funcs[task->state](task);
}

/// Callback of the task `D_mist_r21_8017D798` spawns: does nothing each frame
/// but reserve a 16-byte stack frame.
void func_mist_r21_8017D760(Task* task)
{
    char pad[0x10];
}
