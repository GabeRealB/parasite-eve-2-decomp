#include "common.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/D4.h"

#include "main/fs.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"

/// The room's message table, published at `Task::msgTable` by the room task.
extern GpMsgEntry D_mist_r21_8017D770[];

/// The one task the room task spawns on entry; its callback is the empty
/// `func_mist_r21_8017D760`.
extern TaskDesc D_mist_r21_8017D798[];

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_mist_r21_8017D5DC(void)
{
    return 0;
}

/// Message-table handler for id 0x13EE: copies the location record it is given
/// onto the reply record unchanged and answers 1.
s32 func_mist_r21_8017D5E4(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}

/// Message-table handler for id 0x13F0: accepts the message and does nothing.
s32 func_mist_r21_8017D60C(void)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_mist_r21_8017D614(void)
{
    return 0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7, spawns the task `D_mist_r21_8017D798` describes and
/// advances to the next state.
void func_mist_r21_8017D61C(Task* task)
{
    task->msgTable = D_mist_r21_8017D770;
    Game_SetPtrSlot(task, 7);
    Task_SpawnFromTable(D_mist_r21_8017D798, 0, 0, 0);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: waits for pad 0 to report button 0x200 in
/// mode 0 and button 0x40 in mode 1, then sets the saved location to area 5,
/// warp 1, view 2, starts loading from it, spawns task 0x11 and ends itself.
void func_mist_r21_8017D678(Task* task)
{
    if ((Pad_CheckButtons(0, 0, 0x200) != 0) && (Pad_CheckButtons(0, 1, 0x40) != 0)) {
        Mc_SaveData.at4.loc.area = 5;
        Mc_SaveData.at4.loc.warp = 1;
        Mc_SaveData.at4.loc.view = 2;
        Fs_BeginBootLoad(&Mc_SaveData.at4.loc.view, 0);
        Task_Spawn(0, 0x11, 1, 0);
        taskKill(task);
    }
}

/// The room task's three states.
const TaskFuncTable3 D_mist_r21_8017D5C4 = {
    { func_mist_r21_8017D61C, func_mist_r21_8017D678, taskKill },
};

/// `"target set\n"`: no code in the room reads it.
INCLUDE_RODATA("rooms/nonmatchings/mist_r21/mist_r21", D_mist_r21_8017D5D0);

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
