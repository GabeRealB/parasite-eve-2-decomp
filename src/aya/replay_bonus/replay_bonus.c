#include "common.h"

#include "aya/replay_bonus.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "psyq/libpress.h"
extern UiObjectDesc D_replay_bonus_80119154;
extern u8           D_replay_bonus_801192AC;

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_801158C0);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_801159A0);

u16* func_replay_bonus_80115C68(void)
{
    u16* table = Mem_Malloc(0x11000, 1);

    DecDCTvlcBuild(table);
    return table;
}

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80115CA4);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80115D60);

INCLUDE_RODATA("aya/nonmatchings/replay_bonus/replay_bonus", D_replay_bonus_80115770);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80115ED0);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_801166AC);

INCLUDE_RODATA("aya/nonmatchings/replay_bonus/replay_bonus", D_replay_bonus_801157C8);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80116964);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80116AC0);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80116D68);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80116EC0);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80117194);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_801173A8);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80117484);

s32 func_replay_bonus_80117598(s32 arg0)
{
    u16* p;
    s32  i;

    p = D_replay_bonus_8011908C;
    i = 0;
    do {
        i++;
        if (*p != arg0) {
            p++;
        } else {
            return 1;
        }
    } while (i < 0x4E);
    return 0;
}

s16 func_replay_bonus_801175D0(UiList* list, ReplayBonusCtx* ctx, s32 index)
{
    s16* p = ctx->itemList->itemIds + index;

    return *p;
}

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_801175F0);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_801176A8);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_801177A0);

void func_replay_bonus_80117848(Task* arg0)
{
    u16 timer = arg0->killCountdown + 1;

    arg0->killCountdown = timer;
    if ((s16)timer >= 0x78) {
        Game_Session->field_2 = 1;
        GameMain_SetFrameTiming(0);
        CdCmd_EnqueueLoadFile(1, 0x3E, 3);
        arg0->state = arg0->state + 1;
    }
}

void func_replay_bonus_801178C0(Task* arg0)
{
    if (CdCmd_IsIdle() & 0xFFFF) {
        Text_LoadClutImages();
        arg0->spawnArg2 = Ui_SpawnFromDesc(&D_replay_bonus_80119154, 0, 1, 1, NULL);
        arg0->state     = (s32)(arg0->state + 1);
    }
}

void func_replay_bonus_80117924(Task* arg0)
{
    u16 remaining = arg0->killCountdown - 1;

    arg0->killCountdown = remaining;
    if ((s16)remaining < 0) {
        Display_State.field_11e = 0;
        Game_Session->field_2   = 0;
        Task_CallExit(arg0);
        Display_State.field_11e = 1;
    }
}

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_8011797C);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80117A08);

void func_replay_bonus_80117DE0(u8 arg0)
{
    char pad[0x10];

    D_replay_bonus_801192AC = 0x7F - (arg0 >> 1);
}

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80117E04);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_801183B8);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80118B6C);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80118C64);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80118D7C);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80118E3C);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80118F00);
