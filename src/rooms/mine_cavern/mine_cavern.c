#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/mine_cavern.h"
#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

extern TaskDesc D_mine_cavern_80183CA4[];

extern s32            D_mine_cavern_80183C6C;
extern s32            D_mine_cavern_80187C74;
extern s32            D_mine_cavern_8018804C;
extern s32            D_mine_cavern_80188214;
extern s32            D_mine_cavern_801887B4;
extern s32            D_mine_cavern_80188A3C;
extern s32            D_mine_cavern_80188D24;
extern TaskDesc       D_mine_cavern_8018E3F4;
extern s32            D_mine_cavern_8018EB50;
extern GpAreaApplyRec D_mine_cavern_8018E32C;

extern s8 D_80114C12;

/// One byte of gameplay state. Read back with `lb` elsewhere, so it is signed.
extern s8 D_8007272D;

s32 func_mine_cavern_8017D908(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);

    if (in->msgId == 8) {
        if (GameFlag_GetNibble(0xBB) != 1) {
            if (in->field_5 != 0) {
                return 0;
            }
            Gp_SetNibbleIf(in->field_6, 2);
            if (Gp_StateF0.field_0 == 1 && gGameSession->at4.loc.place == Gp_StateF0.field_0) {
                Gp_RunCapCmd1(9);
                return 0;
            }
            Gp_RunCapCmd1(0xD);
            if (GameFlag_GetNibble(0x11A) != 0) {
                return 0;
            }
            GameFlag_SetNibble(0x11A, 1);
            return 0;
        }
        if (in->field_5 == 0 && GameFlag_GetNibble(0x11A) != 2) {
            GameFlag_SetNibble(0x11A, 2);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 0);
        }
    }

    if (in->msgId == 5) {
        if (gGameSession->at4.loc.place == 1 || gGameSession->at4.loc.place == 4) {
            if (Gp_StateF0.field_0 == 1) {
                if (in->field_5 == 0) {
                    Gp_RunCapCmd1(0xB);
                }
                return 0;
            }
        }
    }
    return 1;
}

s32 func_mine_cavern_8017DAA0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    u8  temp;
    s32 flag;
    s32 cmd;

    if (arg2 == 1) {
        if (GameFlag_GetNibble(0xC7) != 0) {
            return 0;
        }
        if (Gp_StateF0.field_0 == arg2) {
            temp = gGameSession->at4.loc.place;
            if (temp == arg2 || temp == 4) {
                cmd = 0xA;
                goto cap_only;
            }
        }
        flag = GameFlag_GetNibble(0xBB);
        if (flag == 1) {
            cmd = 0x11;
            goto cap_only;
        }
        flag = GameFlag_GetNibble(0xBB);
        if (flag == 3) {
            cmd = 0x12;
            goto spawn;
        }
        flag = GameFlag_GetNibble(0xBE);
        cmd  = 5;
        if (flag == 2) {
            goto cap_only;
        }
    spawn:
        Gp_RunCapCmd1(cmd);
        Task_SpawnFromTable(D_mine_cavern_80183CA4, 0, 0, 0);
        goto rest;
    cap_only:
        Gp_RunCapCmd1(cmd);
    }
rest:
    temp = gGameSession->at4.loc.place;
    if (temp == 1 || temp == 4) {
        switch (arg2) {
            case 8:
                Gp_StartCapSlot(8, 1, GameFlag_GetNibble(0xE2) & 1);
                break;
            case 14:
                Gp_StartCapSlot(0xE, 1, ((u32)GameFlag_GetNibble(0xE2) >> 1) & 1);
                break;
            case 15:
                Gp_StartCapSlot(0xF, 1, ((u32)GameFlag_GetNibble(0xE2) >> 2) & 1);
                break;
            case 16:
                Gp_StartCapSlot(0x10, 1, ((u32)GameFlag_GetNibble(0xE2) >> 3) & 1);
                break;
        }
    }
    return 0;
}

/// Room script callback that does nothing and reports 0.
s32 func_mine_cavern_8017DC50(void)
{
    return 0;
}

s32 func_mine_cavern_8017DC58(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if ((arg2->field_2 == 6) && (GameFlag_GetNibble(0xC4) == 1)) {
        Gp_RunCapCmd1(6);
    }
    return 0;
}

/// Advances the cavern's collapse sequence one step: flag 0xE6 goes 0 -> 1
/// (bit 0 of `Gp_StateC08.field_6` set) and 1 -> 2 (quake shake, then camera
/// pan), each step writing `D_mine_cavern_8018EB50` to the step number.
s32 func_mine_cavern_8017DC9C(void)
{
    if (GameFlag_GetNibble(0xE6) == 0) {
        Gp_StateC08.field_6 |= 1;
        Gp_PulseState1C();
        GameFlag_SetNibble(0xE6, 1);
        D_mine_cavern_8018EB50 = 1;
    } else if (GameFlag_GetNibble(0xE6) == 1) {
        func_800E3FAC(0xA2, 0x3D);
        func_800E8634((s32)&D_mine_cavern_80188A3C, 0, (s32)&D_mine_cavern_80188D24);
        GameFlag_SetNibble(0xE6, 2);
    }
    return 0;
}

s32 func_mine_cavern_8017DD38(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0xD) {
        SndEvt_EnqueueType6(0x54020000 | 0xD, 0, 0);
    }
    return 0;
}

void func_mine_cavern_8017DD6C(Task* task)
{
    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() == 0xB) {
            GameFlag_SetNibble(0xC4, 1);
            GameFlag_SetNibble(0xBE, 2);
            GameFlag_SetNibble(0xC3, 0);
        }
        if (Gp_GetCapEventKey() == 0x15) {
            GameFlag_SetNibble(0xBB, 1);
            GameFlag_SetNibble(0x1B9, 0);
        }
        taskKill(task);
    }
}

void func_mine_cavern_8017DDFC(Task* arg0)
{
    arg0->msgTable = &D_mine_cavern_80183C6C;
    Game_SetPtrSlot(arg0, 7);
    if ((gGameSession->at4.loc.place == 1) && (GameFlag_GetNibble(0x10F) == 0)) {
        func_800E8634((s32)&D_mine_cavern_80187C74, 0, (s32)&D_mine_cavern_8018804C);
        func_mine_cavern_8017E394();
        GameFlag_SetNibble(0x10F, 1);
    } else {
        D_80062735 = 1;
    }
    Task_SpawnFromTable(&D_mine_cavern_8018E3F4, 0, 0, 0);
    if (GameFlag_GetNibble(0xC7) != 0) {
        func_mine_cavern_8017E3A0(1);
    } else {
        func_mine_cavern_8017E3A0(0);
    }
    arg0->state            = arg0->state + 1;
    D_mine_cavern_8018EB50 = 0;
}

void func_mine_cavern_8017DEE4(Task* task)
{
    s32 flag;

    flag = GameFlag_GetNibble(0xE6);
    if ((flag == 1) && (D_mine_cavern_8018EB50 == flag) && (D_80114C12 != D_mine_cavern_8018EB50)) {
        func_800E8634((s32)&D_mine_cavern_80188214, 0, (s32)&D_mine_cavern_801887B4);
        D_mine_cavern_8018EB50 = 2;
    }
}

/// The room task's state handlers, run by `func_mine_cavern_8017DF54`.
const TaskFuncTable3 D_mine_cavern_8017D5C4 = {
    { func_mine_cavern_8017DDFC, func_mine_cavern_8017DEE4, taskKill },
};

/// Runs the room task's current state handler from the room's three-entry
/// table, copying the table onto the stack before the call.
void func_mine_cavern_8017DF54(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_cavern_8017D5C4;
    sp.funcs[task->state](task);
}

void func_mine_cavern_8017DFAC(s32 arg0)
{
    if ((GameFlag_GetNibble(0xE6) == 1 && D_mine_cavern_8018EB54 == 0) ||
        (GameFlag_GetNibble(0xE6) == 2 && D_mine_cavern_8018EB54 == 1)) {
        Gp_ReleaseStateF0Add(Gp_LookupSlot4(0), 0x1E);
        Gp_StateF0.field_1       = arg0;
        gGameSession->flowFlags |= 0x80;
        D_mine_cavern_8018EB54  += 1;
        return;
    }
    if (arg0 < Gp_StateF0.field_1) {
        Gp_StateF0.field_1 = arg0;
    }
}

void func_mine_cavern_8017E088(s16 arg0)
{
    Gp_StartCapSlot(arg0, 1, 1);
}

void func_mine_cavern_8017E0B4(void)
{
    Gp_StateF0.field_0 = 0;
    if (Gp_StateF0.field_6 == 0) {
        ((void (*)(s32))Gp_IncStateF0Ref)(0);
    }
    Gp_ArmStateF0(1);
}

void func_mine_cavern_8017E0F4(s32 arg0)
{
    if (arg0 != 0) {
        gGameSession->flowFlags &= 0xFD;
        return;
    }
    gGameSession->flowFlags |= 2;
    gGameSession->flowFlags |= 8;
}

/// Room script callback: stores its argument into `D_8007272D`.
void func_mine_cavern_8017E150(s8 arg0)
{
    D_8007272D = arg0;
}

void func_mine_cavern_8017E15C(void)
{
    Gp_ApplyAreaRecs(&D_mine_cavern_8018E32C);
}

/// Room script callback: stores its argument into `D_80062735`.
void func_mine_cavern_8017E180(u8 arg0)
{
    D_80062735 = arg0;
}

void func_mine_cavern_8017E18C(Task* task)
{
    task->killCountdown++;
    switch (task->killCountdown) {
        case 0x21:
        case 0x6:
        case 0x40:
        case 0x7C:
        case 0x60:
        case 0x8C:
            SndEvt_EnqueueType6(0x1000003A, 0, 0x30);
            break;
        case 0x50:
        case 0x12:
        case 0x30:
        case 0x70:
        case 0x87:
        case 0x218:
            SndEvt_EnqueueType6(0x10000039, 0, 0x30);
            break;
    }
    if ((gGameSession->evtSkipped != 0) || (task->killCountdown >= 0x219)) {
        taskKill(task);
    }
}

void func_mine_cavern_8017E2D8(void)
{
    SndEvt_EnqueueType2(0, 0x64);
}

/// Sets bit 0 of `Gp_StateC08.field_6` and pulses `Gp_State1C`.
void func_mine_cavern_8017E2FC(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
}
