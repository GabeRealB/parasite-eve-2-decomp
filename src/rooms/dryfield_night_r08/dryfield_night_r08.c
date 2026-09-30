#include "rooms/dryfield_night_r08.h"

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

#include "main/fs.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

extern s32 D_80133898;
extern s32 D_801341E0;
/// The room's message table, published in `Task::msgTable` for
/// `Gp_DispatchMsg` to walk: 0x13EE, 0x13F1, 0x13EF and 0x13F0.
extern GpMsgEntry D_dryfield_night_r08_80180544[];

s32 func_dryfield_night_r08_8017D5F0(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_dryfield_night_r08_8017D5F8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_night_r08_8017D620(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_dryfield_night_r08_8017D628(Task*, s32, TaskMessageArg, TaskMessageArg);

GpMsgEntry D_dryfield_night_r08_80180544[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_r08_8017D5F8 },
    { 5105, func_dryfield_night_r08_8017D5F0 },
    { 5103, func_dryfield_night_r08_8017D628 },
    { 5104, func_dryfield_night_r08_8017D620 },
    { 0x7FFFFFFF, NULL },
};

static void func_dryfield_night_r08_8017D630(Task* arg0);
static void func_dryfield_night_r08_8017D6B0(Task* task);

/// Handler for message 0x13F1 in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_night_r08_8017D5F0(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table: copies the location
/// record the sender passes onto the reply record and answers 1.
s32 func_dryfield_night_r08_8017D5F8(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    return 1;
}

/// Handler for message 0x13F0 in the room's message table: does nothing and
/// answers 0.
s32 func_dryfield_night_r08_8017D620(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13EF in the room's message table: does nothing and
/// answers 0.
s32 func_dryfield_night_r08_8017D628(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// The room task's set-up state: publishes the room's message table, claims
/// pointer slot 7, places the stream buffer 0x20000 bytes into `Fs_ActorLoadBase1`
/// and, unless `Mc_SaveData[0].state.demoScene` is 9, passes `D_80133898` and `D_801341E0` to
/// `func_800E8634`. Then advances to the idle state.
static void func_dryfield_night_r08_8017D630(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_r08_80180544;
    Game_SetPtrSlot(arg0, 7);
    Gp_SetStreamBuf((u8*)Fs_ActorLoadBase1 + 0x20000);
    if (Mc_SaveData[0].state.demoScene != 9) {
        func_800E8634(&D_80133898, 0, &D_801341E0);
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// The room task's idle state, entry 1 of its three-state table: does nothing.
/// The 0x10-byte local is never used, but the original reserved the frame.
static void func_dryfield_night_r08_8017D6B0(Task* task)
{
    char pad[0x10];
}

/// The room task's three states: set up, idle, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_r08_8017D5C4 = {
    { func_dryfield_night_r08_8017D630, func_dryfield_night_r08_8017D6B0, taskKill },
};

/// The room task: copies its three-state table onto the stack and runs the
/// entry for the task's current state.
void func_dryfield_night_r08_8017D6C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_r08_8017D5C4;
    sp.funcs[task->state](task);
}
