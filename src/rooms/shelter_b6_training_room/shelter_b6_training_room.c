#include "common.h"
#include "main/stage.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern s16 D_800691CA;
extern u8  D_801156F9;

extern GpMsgEntry D_shelter_b6_training_room_80182AF4[];
extern s32        D_shelter_b6_training_room_80182B24;
extern TaskDesc   D_shelter_b6_training_room_801839A8;
extern s32        D_shelter_b6_training_room_80183BB4;
extern s32        D_shelter_b6_training_room_80184124;
extern s32        D_shelter_b6_training_room_80184274;
/// The stream playback descriptors: the one-shot launcher, then the player.
extern TaskDesc D_shelter_b6_training_room_8018431C[];
extern s32      D_shelter_b6_training_room_80185C58;

/// The room's tracked task, driven by `func_shelter_b6_training_room_8017D974`,
/// or NULL when none is running.
extern Task* D_shelter_b6_training_room_80185C5C;

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

void func_shelter_b6_training_room_8017DBB0(s32 arg0);

/// The room's handler for message 0x13F1, which does nothing and returns 0.
s32 func_shelter_b6_training_room_8017D638(void)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming `GpSaveLoc` onto
/// the outgoing one, passes both to `func_80179B14`, and returns 1.
s32 func_shelter_b6_training_room_8017D640(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}

s32 func_shelter_b6_training_room_8017D684(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 5:
            if (GameFlag_GetNibble(0x153) != 0) {
                Gp_RunCapCmd1(7);
            } else if (Gp_StateF0.field_0 == 1) {
                Gp_RunCapCmd1(5);
            } else {
                Gp_RunCapCmd1(7);
            }
            break;
        case 6:
            if (GameFlag_GetNibble(0x154) != 0) {
                Gp_RunCapCmd1(8);
            } else if (Gp_StateF0.field_0 == 1) {
                Gp_RunCapCmd1(6);
            } else {
                Gp_RunCapCmd1(8);
            }
            break;
        case 4:
            if (GameFlag_GetNibble(0x146) != 0) {
                Gp_RunCapCmd1(7);
            } else if (Gp_StateF0.field_0 != 1) {
                Gp_RunCapCmd1(0xA);
            } else {
                Gp_RunCapCmd1(4);
            }
            break;
    }
    return 0;
}

/// The room's handler for message 0x13EF, which does nothing and returns 0.
s32 func_shelter_b6_training_room_8017D75C(void)
{
    return 0;
}

s32 func_shelter_b6_training_room_8017D764(void)
{
    gGameSession->flowFlags |= 0x80;
    func_800E8634((s32)&D_shelter_b6_training_room_80183BB4, 0, (s32)&D_shelter_b6_training_room_80184124);
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(3), 0x7DB, (s32)&D_shelter_b6_training_room_80182B24, 0);
    D_shelter_b6_training_room_80185C58 = 1;
    return 0;
}

void func_shelter_b6_training_room_8017D7D4(Task* arg0)
{
    u16* ptr;
    s32  i;

    arg0->msgTable = D_shelter_b6_training_room_80182AF4;
    Game_SetPtrSlot(arg0, 7);
    ptr = (u16*)Fs_ImgBuffers;
    i   = 0;
    do {
        *ptr = (u16)(*ptr | 0x8000);
        i   += 1;
        ptr += 1;
    } while (i <= 0x12BFF);
    GameFlag_SetNibble(0x4D, 1);
    gStageSceneMusicEntry = 0xA;
    func_shelter_b6_training_room_8017DBB0(0);
    arg0->state                         = (s32)(arg0->state + 1);
    D_shelter_b6_training_room_80185C58 = 0;
}

void func_shelter_b6_training_room_8017D874(void)
{
    u8 place;

    D_800691CA = 2;
    place      = gGameSession->at4.loc.place;
    if (place == 1 && gGameSession->eventState == 0 && D_shelter_b6_training_room_80185C58 == place) {
        func_800E8614((s32)&D_shelter_b6_training_room_80184274, 0);
        D_shelter_b6_training_room_80185C58 = 2;
    }
}

/// State handlers of the room task: set-up, the per-frame tick and `taskKill`.
const TaskFuncTable3 D_shelter_b6_training_room_8017D5C4 = {
    { func_shelter_b6_training_room_8017D7D4, func_shelter_b6_training_room_8017D874, taskKill },
};

/// Runs a task through the room's three-state table, copied onto the stack
/// first and indexed by the task's state.
void func_shelter_b6_training_room_8017D8E8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b6_training_room_8017D5C4;
    sp.funcs[task->state](task);
}

void func_shelter_b6_training_room_8017D940(void)
{
    D_shelter_b6_training_room_80185C5C = Task_SpawnFromTable(&D_shelter_b6_training_room_801839A8, 0, 0, 0);
}

/// Drives the room's tracked task: an argument in 0..1 becomes its
/// `spawnArg1`; anything else kills the task and clears the pointer. Does
/// nothing when no task is tracked.
void func_shelter_b6_training_room_8017D974(s32 arg0)
{
    Task* t = D_shelter_b6_training_room_80185C5C;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1 = arg0;
    return;
kill:
    taskKill(D_shelter_b6_training_room_80185C5C);
    D_shelter_b6_training_room_80185C5C = NULL;
}

void func_shelter_b6_training_room_8017D9C8(Task* task)
{
    GpWorkObj* work;
    u16        tick;

    if (D_801156F9 == 0) {
        if (task->state == 0) {
            if (task->spawnArg1 != 0) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            work = Gp_FindWorkById(gGameSession->at4.loc.area | ((gGameSession->at4.loc.stage << 8) | 0x1000));
            func_800B0928(gameGetPtrSlot(3), (Task*)work->field_0, 0x200, 0x100, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}

/// Spawns the task that starts the room's stream playback.
void func_shelter_b6_training_room_8017DAC8(void)
{
    Task_SpawnFromTable(D_shelter_b6_training_room_8018431C, 0, 0, 0);
}

void func_shelter_b6_training_room_8017DAF8(s32 arg0)
{
    gGameSession->flowFlags |= 0x80;
    if (arg0 != 0) {
        Gp_StateF0.field_1 = arg0;
    }
}

/// Sets the saved location to area 0x16, warp 1, room 1 and spawns task 0x11.
void func_shelter_b6_training_room_8017DB28(void)
{
    Mc_SaveData.at4.loc.area = 0x16;
    Mc_SaveData.at4.loc.warp = 1;
    Mc_SaveData.at4.loc.room = 1;
    Task_Spawn(0, 0x11, 0, 0);
}

void func_shelter_b6_training_room_8017DB70(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
    SndEvt_EnqueueType7(0x80000000, 1);
}

void func_shelter_b6_training_room_8017DBB0(s32 arg0)
{
    D_shelter_b6_training_room_80185C5C = NULL;
}
