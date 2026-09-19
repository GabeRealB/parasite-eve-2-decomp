#include "common.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"

extern s32      D_mine_cavern_80183C6C;
extern s32      D_mine_cavern_80187C74;
extern s32      D_mine_cavern_8018804C;
extern s32      D_mine_cavern_80188214;
extern s32      D_mine_cavern_801887B4;
extern s32      D_mine_cavern_80188A3C;
extern s32      D_mine_cavern_80188D24;
extern TaskDesc D_mine_cavern_8018E3F4;
extern s32      D_mine_cavern_8018EB50;
extern u8       D_80062735;
extern s32      func_mine_cavern_8017E3A0(s32 arg0);
extern void     func_mine_cavern_8017E394(void);

/// Cutscene / "among us" mode flag in the main executable.
extern s8 D_80114C12;

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

void func_mine_cavern_8017DEE4(void)
{
    s32 flag;

    flag = GameFlag_GetNibble(0xE6);
    if ((flag == 1) && (D_mine_cavern_8018EB50 == flag) && (D_80114C12 != D_mine_cavern_8018EB50)) {
        func_800E8634((s32)&D_mine_cavern_80188214, 0, (s32)&D_mine_cavern_801887B4);
        D_mine_cavern_8018EB50 = 2;
    }
}
