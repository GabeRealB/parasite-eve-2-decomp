#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

extern s16 D_80071076;

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// Cutscene script blob argument of `func_800E8634`.
extern s32 D_neo_ark_r26_8017DA74;

/// Cutscene script blob argument of `func_800E8634`.
extern s32 D_neo_ark_r26_8017DFCC;

/// Room message handler table installed into `Task::msgTable`.
extern GpMsgEntry D_neo_ark_r26_8017E0A4[];

/// Script callback: unless attract demo 9 is playing, points the save's
/// location at stage 5, area 0x1C, warp 1, room 1, sets `D_80071076`, spawns
/// task 0x11 and starts loading that location.
void func_neo_ark_r26_8017D5D0(void)
{
    if (Mc_SaveData.demoScene != 9) {
        Mc_SaveData.at4.loc.stage = 5;
        Mc_SaveData.at4.loc.area  = 0x1C;
        Mc_SaveData.at4.loc.warp  = 1;
        Mc_SaveData.at4.loc.room  = 1;
        D_80071076                = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Fs_BeginBootLoad(&Mc_SaveData.at4.loc.view, 1);
    }
}

s32 func_neo_ark_r26_8017D648(void)
{
    return 0;
}

/// Message handler for the save location: copies the incoming `GpSaveLoc`
/// onto the outgoing one and passes both to `func_80179B14`. Returns 1.
s32 func_neo_ark_r26_8017D650(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}

s32 func_neo_ark_r26_8017D694(void)
{
    return 0;
}

s32 func_neo_ark_r26_8017D69C(void)
{
    return 0;
}

/// Room task state 0: installs the message table, claims pointer slot 7, then
/// starts the room script unless the attract demo 9 is playing. Advances to
/// state 1.
void func_neo_ark_r26_8017D6A4(Task* arg0)
{
    arg0->msgTable = D_neo_ark_r26_8017E0A4;
    Game_SetPtrSlot(arg0, 7);
    if (Mc_SaveData.demoScene != 9) {
        func_800E8634((s32)&D_neo_ark_r26_8017DA74, 0, (s32)&D_neo_ark_r26_8017DFCC);
    }
    arg0->state = arg0->state + 1;
}

/// Room task state 1: does nothing, keeping the task alive.
void func_neo_ark_r26_8017D710(Task* task)
{
    char pad[0x10];
}

/// State handlers of the room task `func_neo_ark_r26_8017D720`, indexed by
/// `Task::state`: the set-up tick, the idle tick, and `taskKill`.
const TaskFuncTable3 D_neo_ark_r26_8017D5C4 = {
    {
        func_neo_ark_r26_8017D6A4,
        func_neo_ark_r26_8017D710,
        taskKill,
    },
};

/// Room task: dispatches through a stack copy of its state table.
void func_neo_ark_r26_8017D720(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_r26_8017D5C4;
    sp.funcs[task->state](task);
}

void func_neo_ark_r26_8017D778(void)
{
}
