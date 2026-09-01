#include "common.h"

#include "actors/actor_215100.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"

/// Imports from the 0x80180000 overlay; no header names them yet.
void            func_80180390(s32 arg0);
void            func_801811C0(s16 arg0);
void            func_801848B4(void);
void            func_80184954(void);
extern TaskDesc D_80185384;

extern s8 D_8007216C;

extern TaskDesc D_actor_215100_8014CF6C;
extern TaskDesc D_actor_215100_8014E13C;
extern TaskDesc D_actor_215100_801544FC;
extern TaskDesc D_actor_215100_80154508;
extern Task*    D_actor_215100_8015E64C;

extern s32 D_actor_215100_8014D038;
extern s32 D_actor_215100_8014D03C;
extern s32 D_actor_215100_80153ED4;
extern s32 D_actor_215100_80153FDC;
extern s32 D_actor_215100_801543E4;

/// Caption script currently being played back, and the entry it is up to.
extern Actor215100Caption* D_actor_215100_8015E658;
extern s16                 D_actor_215100_8015E662;
extern s32                 D_actor_215100_8015E670;

void func_actor_215100_8014B0D4(void);
void func_actor_215100_8014B2B8(s16 arg0, s16 arg1, s32 arg2);
void func_actor_215100_8014B3C8(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void func_actor_215100_8014BEE8(void);

INCLUDE_RODATA("actors/nonmatchings/actor_215100/actor_215100", D_actor_215100_80149E20);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_80149F2C);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014A398);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014A5C0);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014A7C4);

void func_actor_215100_8014A908(void)
{
    D_actor_215100_8014D038 = 0;
    if (D_actor_215100_8015E670 < 3) {
        D_8007216C = 8;
        func_801811C0(0);
    } else {
        func_80180390(1);
        D_actor_215100_8014D03C = 1;
    }
    if (D_actor_215100_8015E670 < 4) {
        Gp_StateC08.field_6 &= 0xFD;
    }
    SndEvt_EnqueueType2(0, 0x1E);
}

void func_actor_215100_8014A9A0(void)
{
    if (D_actor_215100_8015E670 == 5) {
        D_actor_215100_8014D038 = 0;
        func_80180390(1);
        D_actor_215100_8014D03C = 1;
        Game_Session->field_126 = 1;
        SndEvt_EnqueueType2(0, 0x1E);
        Game_Session->field_69 |= 0x80;
    }
    if (D_actor_215100_8015E670 < 3) {
        Gp_RunCapCmd(0x1D, 3);
        Task_SpawnFromTable(&D_actor_215100_8014CF6C, 1, 0, 0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014AA54);

void func_actor_215100_8014AB6C(void)
{
    if (D_actor_215100_8014D038 != 0) {
        func_80184954();
        return;
    }
    Gp_SpawnIfCapIdle(0x11, 1);
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014ABAC);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014AD50);

void func_actor_215100_8014ADD8(void)
{
    Task_SpawnFromTable(&D_80185384, 0, 0, 0);
}

void func_actor_215100_8014AE08(s32 arg0)
{
    if (arg0 != 0) {
        func_801848B4();
    }
}

void func_actor_215100_8014AE2C(s32 arg0)
{
    if (arg0 != 0) {
        D_actor_215100_8015E64C = Task_SpawnFromTable(&D_actor_215100_8014E13C, 2, 0, 0);
        return;
    }
    if (D_actor_215100_8015E64C != NULL) {
        Task_Kill(D_actor_215100_8015E64C);
        D_actor_215100_8015E64C = NULL;
    }
}

void func_actor_215100_8014AE90(s16 arg0)
{
    func_801811C0(arg0);
}

void func_actor_215100_8014AEB4(s16 arg0)
{
    Game_Session->field_52 = arg0;
}

void func_actor_215100_8014AEC4(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x300, 0);
        return;
    }
    Gp_ResetCap();
}

void func_actor_215100_8014AF0C(void)
{
    switch (GameFlag_GetNibble(0xF5)) {
        case 0:
            GameFlag_SetNibble(0xF5, 1);
            func_800E8614((s32)&D_actor_215100_80153ED4, 0);
            break;
        case 1:
            func_800E8614((s32)&D_actor_215100_80153FDC, 0);
            GameFlag_SetNibble(0xF5, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_215100_801543E4, 0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014AFAC);

void func_actor_215100_8014B0D4(void)
{
    if ((D_actor_215100_8015E658 != NULL) &&
        (D_actor_215100_8015E658[D_actor_215100_8015E662].field_8 != -1) &&
        (Gp_CapBusy() == 0)) {
        func_actor_215100_8014B3C8(D_actor_215100_8015E658[D_actor_215100_8015E662].field_8, 0x80, 1,
                                   D_actor_215100_8015E658[D_actor_215100_8015E662].field_0 |
                                       ((D_actor_215100_8015E658[D_actor_215100_8015E662].field_1 & 0x10) * 0x10));
        if (!(D_actor_215100_8015E658[D_actor_215100_8015E662].field_4 & 1)) {
            func_actor_215100_8014BEE8();
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014B1B0);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014B2B8);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014B3C8);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014BDFC);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014BEE8);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C06C);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C17C);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C298);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C360);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C418);

void func_actor_215100_8014C46C(Task* task)
{
    s32 remaining;

    remaining       = task->spawnArg1 - 1;
    task->spawnArg1 = remaining;
    if (remaining <= 0) {
        Task_Kill(task);
    }
    func_actor_215100_8014B0D4();
}

void func_actor_215100_8014C4A8(Task* task)
{
    s32 remaining;
    s32 state;

    state = task->state;
    switch (state) {
        case 0:
            task->state = 1;
            break;
        case 1:
            remaining       = task->spawnArg1 - 1;
            task->spawnArg1 = remaining;
            if ((remaining <= 0) || (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0)) {
                Task_Kill(task);
                Stage_SetEndingFlag();
            }
            break;
    }
    func_actor_215100_8014B0D4();
}

void func_actor_215100_8014C538(s16 arg0, s16 arg1, s16 arg2)
{
    func_actor_215100_8014B2B8(arg0, arg1, 0xD0);
    Task_SpawnFromTable(&D_actor_215100_801544FC, 0, arg2, 0);
}

void func_actor_215100_8014C58C(s16 arg0, s16 arg1, s16 arg2)
{
    func_actor_215100_8014B2B8(arg0, arg1, 0xD0);
    Display_InitModeObj(&D_actor_215100_80154508, arg2, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C5E0);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C660);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C874);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014CA2C);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014CA80);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014CB04);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014CB2C);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014CBB8);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014CC04);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014CC7C);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014CCE0);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014CD4C);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014CDB0);

s32 func_actor_215100_8014CE28(void)
{
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014CE30);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014CEF8);
