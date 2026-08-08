#include "common.h"

#include "main/game.h"
#include "main/task.h"

#include <psyq/rand.h>

extern u8           D_80071068;
extern s8           D_800710A9;
extern WipUiHolder* Wip_UiHolder;

/// 5-way task dispatch table at package header + 4 (header.s).
extern TaskFuncTable5 D_80093804;
/// Stores the result of rand() after each title dispatcher tick.
extern s32 D_80094CA4;

/// Title-screen work block stored at Task::field_1C (Mem_Calloc 0x18).
typedef struct {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte pad_C[0x8];
    /* 0x14 */ s32  field_14;
} TitleWork; /* size 0x18 */

void func_80093ABC(Task* arg0);
void func_807246B4(void);

void func_8009389C(Task* arg0)
{
    register s32  flag asm("s2");
    DisplayState* ds;
    TitleWork*    work;

    flag          = 1;
    ds            = &Display_State;
    ds->field_100 = 0;
    Wip_UiHolder  = NULL;
    if (arg0->field_34 < 0) {
        flag            = 0;
        arg0->field_34 &= 0x7FFFFFFF;
    }
    if (arg0->field_34 > 0) {
        arg0->field_34 -= 1;
        return;
    }
    work = Mem_Calloc(0x18, 0);
    if (work != NULL) {
        arg0->field_1C                  = (TaskIdMap*)work;
        *(volatile s32*)&work->field_14 = 5;
        *(volatile s32*)&work->field_4  = 2;
        *(volatile s32*)&work->field_8  = flag;
        *(volatile s32*)&work->field_0  = 0;
        if (D_800710A9 != 0) {
            work->field_4 = 3;
        }
        Text_LoadClutImages();
        Display_SetMode(0x9010);
        ds->field_1d  = -1;
        work->field_0 = -0x10;
        ds->field_100 = 1;
        if (ds->field_112 != 0) {
            func_807246B4();
        }
        CdCmd_EnqueueLoadFile(1, 0, 0);
        arg0->field_30 += 2;
        func_80093ABC(arg0);
    }
}

INCLUDE_ASM("title/nonmatchings/title", func_800939C4);

INCLUDE_ASM("title/nonmatchings/title", func_80093ABC);

INCLUDE_ASM("title/nonmatchings/title", func_8009407C);

void func_8009470C(Task* arg0)
{
    s32* p = &arg0->field_30;

    D_80071068 = 1;
    (*p)++;
}

void func_8009472C(Task* arg0)
{
    TaskFuncTable5 sp;

    sp         = D_80093804;
    D_80094CA4 = rand();
    sp.funcs[arg0->field_30](arg0);
}

void func_800947A8(Task* arg0)
{
    Task_CallExit(arg0);
}

INCLUDE_ASM("title/nonmatchings/title", func_800947C8);

INCLUDE_ASM("title/nonmatchings/title", func_80094A08);

void func_80094B90(s32 arg0)
{
    s8              param2[4];
    u8*             param1;
    register void** scratch asm("s0");
    register void*  head asm("v1");
    GameSession*    gs;
    u8*             p2;

    scratch = (void**)G_SCRATCH_HEAD;
    gs      = Game_Session;
    arg0    = arg0 + 0xA;
    p2      = (u8*)param2;

    head     = *scratch;
    param1   = (u8*)head - 8;
    *scratch = param1;

    gs->field_80    = 0;
    param1[3]       = 0;
    param1[2]       = 0x50;
    ((u8*)head)[-8] = 0;

    param2[0] = arg0;
    param2[3] = 0;
    param2[2] = 0;
    param2[1] = 0;
    CdCmd_Enqueue(0x21, param1, p2);

    *scratch = (u8*)*scratch + 8;
}
