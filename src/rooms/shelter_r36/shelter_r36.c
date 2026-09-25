#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 D_shelter_r36_8017DF2C;
extern s32 D_shelter_r36_8017E5A4;
extern s32 D_shelter_r36_8017E664;
extern s32 D_shelter_r36_8017E8BC;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// The room's message table, installed on the room entry task.
extern GpMsgEntry D_shelter_r36_8017E97C[];

/// The room's two event tasks, one per arrival warp.
extern TaskDesc D_shelter_r36_8017DF14[];

/// Two-entry spawn table: entry 0 is `func_shelter_r36_8017DBC0`, which starts
/// entry 1, the stream task `func_shelter_r36_8017DA34`, on the display list.
extern TaskDesc D_shelter_r36_8017E9A4[];

/// Entry 0 of `D_shelter_r36_8017DF14`, spawned on arrival by warp 1. If
/// event nibble 0x113 is clear it starts CAP slot 1; otherwise it loads CAP
/// file 3 and starts slot 2, then passes the first pair of event blocks to
/// `func_800E8634`. Once `eventState` is back to 0 it either sets restart mode
/// 0xFF and session `field_12E` to 1 and ends (nibble still clear), or resets
/// the CAP state and passes the second pair before ending.
void func_shelter_r36_8017D5E8(Task* task)
{
    s32 state;
    s16 slot;

    state = task->state;
    switch (state) {
        case 0:
            if (GameFlag_GetNibble(0x113) == 0) {
                slot = 1;
            } else {
                Gp_CapFile = 0;
                Gp_LoadCapFile(3);
                func_800E6D4C(0x140, 0x100);
                slot = 2;
            }
            Gp_StartCapSlot(slot, 0, 0);
            func_800E8634((s32)&D_shelter_r36_8017DF2C, 0, (s32)&D_shelter_r36_8017E5A4);
            task->state++;
            break;
        case 1:
            if (gGameSession->eventState == 0) {
                if (GameFlag_GetNibble(0x113) == 0) {
                    gGameSession->restartMode = 0xFF;
                    gGameSession->field_12E   = state;
                    taskKill(task);
                } else {
                    Gp_ResetCap();
                    task->state++;
                }
            }
            break;
        case 2:
            func_800E8634((s32)&D_shelter_r36_8017E664, 0, (s32)&D_shelter_r36_8017E8BC);
            taskKill(task);
            break;
    }
}

/// Leaves for stage 4, area 0x24, warp 2, room 1 by spawning task 0x11 and
/// starting the boot load, unless `demoScene` is 9. Reached from the room's
/// event data.
void func_shelter_r36_8017D738(void)
{
    if (Mc_SaveData.demoScene != 9) {
        Mc_SaveData.at4.loc.stage = 4;
        Mc_SaveData.at4.loc.area  = 0x24;
        Mc_SaveData.at4.loc.warp  = 2;
        Mc_SaveData.at4.loc.room  = 1;
        gDisplayState.roomVariant = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Fs_BeginBootLoad(&Mc_SaveData.at4.loc.view, 1);
    }
}

/// Entry 1 of `D_shelter_r36_8017DF14`, spawned on arrival by warp 2: spawns
/// entry 0 of `D_shelter_r36_8017E9A4`, which starts the stream, and two frames
/// later sets restart mode 0xFF and session `field_12E` to 1 and ends.
void func_shelter_r36_8017D7B4(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_shelter_r36_8017E9A4, 0, 0, 0);
            task->state++;
            break;
        case 1:
            Gp_MsgPlayerWeapon(0);
            task->state++;
            break;
        case 2:
            gGameSession->restartMode = 0xFF;
            gGameSession->field_12E   = 1;
            taskKill(task);
            break;
    }
}

/// Loads CAP file `arg0` (non-zero), with `func_800E6D4C` given 0x280 for file
/// 1 and 0x2C0 otherwise; 0 resets the CAP state instead. Reached from the
/// room's event data.
void func_shelter_r36_8017D870(s32 arg0)
{
    s16 var_a0;

    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(arg0);
        var_a0 = 0x2C0;
        if (arg0 == 1) {
            var_a0 = 0x280;
        }
        func_800E6D4C(var_a0, 0x100);
        return;
    }
    Gp_ResetCap();
}

/// Message-table handler for message 0x13F1. Does nothing.
s32 func_shelter_r36_8017D8C8(void)
{
    return 0;
}

/// Message-table handler for message 0x13EE: copies the incoming record onto
/// the outgoing one and passes both to `func_80179A04`. Always returns 1.
s32 func_shelter_r36_8017D8D0(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}

/// Message-table handler for message 0x13F0. Does nothing.
s32 func_shelter_r36_8017D914(void)
{
    return 0;
}

/// Message-table handler for message 0x13EF. Does nothing.
s32 func_shelter_r36_8017D91C(void)
{
    return 0;
}

/// The room entry task's first state: installs the room's message table, takes
/// pointer slot 7, and spawns the entry of `D_shelter_r36_8017DF14` that
/// matches the arrival warp (1 or 2).
void func_shelter_r36_8017D924(Task* task)
{
    task->msgTable = D_shelter_r36_8017E97C;
    Game_SetPtrSlot(task, 7);
    if (gGameSession->at4.loc.warp == 1) {
        Task_SpawnFromTable(D_shelter_r36_8017DF14, 0, 0, 0);
    }
    if (gGameSession->at4.loc.warp == 2) {
        Task_SpawnFromTable(D_shelter_r36_8017DF14, 1, 0, 0);
    }
    task->state++;
}

/// The room entry task's idle state.
void func_shelter_r36_8017D9CC(Task* task)
{
    char pad[0x10];
}

/// The room entry task's three states: set the room up, idle, end.
const TaskFuncTable3 D_shelter_r36_8017D5C4 = {
    { func_shelter_r36_8017D924, func_shelter_r36_8017D9CC, taskKill },
};

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_shelter_r36_8017D9DC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_r36_8017D5C4;
    sp.funcs[task->state](task);
}
