#include "common.h"

#include "main/game.h"
#include "main/task.h"

#include <psyq/memory.h>
#include <psyq/rand.h>
#include <psyq/stdio.h>

extern u8           D_80071068;
extern s8           D_800710A9;
extern s16          D_800710AC;
extern u16          D_80071094;
extern u8           D_80071086;
extern u_long*      D_800710A0;
extern DR_TPAGE*    D_80071190;
extern WipUiHolder* Wip_UiHolder;
extern u16*         D_8005C374;
extern u8           D_800733F0[2][0x6C];
extern u8           D_800734C8[2][0xB0];
extern u8           D_80073628[2][0x24];
extern u8           D_80073670[2][0xE4];
extern u8           D_80073838[2][0xA4];
extern u8           D_80073980[0x208];

/// 5-way task dispatch table at package header + 4 (header.s).
extern TaskFuncTable5 D_80093804;
/// "####DEMO_CARD_RESTORE STAGE %d, SCENE %d\n" in header.s
extern char D_80093830[];
/// TaskDesc table for title/demo spawn (menu.data.s).
extern TaskDesc D_80094C8C;
/// Stores the result of rand() after each title dispatcher tick.
extern s32 D_80094CA4;
/// Title state flag (halfword after D_80094CA4).
extern u16 D_80094CA8;

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

void func_800939C4(s32 y, s32 v, s32 color)
{
    SPRT*     p;
    DR_TPAGE* dr;
    u8        c;

    c             = color;
    p             = (SPRT*)D_80071190;
    D_80071190    = (DR_TPAGE*)(p + 1);
    p->x0         = -0x80;
    p->w          = 0x100;
    p->h          = 0x10;
    p->clut       = 0x3FC0;
    *(s32*)&p->r0 = (c << 16) | (c << 8) | c;
    setlen(p, 4);
    p->u0 = 0;
    p->v0 = v;
    setcode(p, 0x66);
    p->y0 = y;
    addPrim(D_800710A0, p);

    dr         = D_80071190;
    D_80071190 = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE10002BC;
    addPrim(D_800710A0, dr);
}

INCLUDE_ASM("title/nonmatchings/title", func_80093ABC);

/// Restore demo card / save banks from D_8005C374 (or 0x80600100 when D_80071094 == 0x10).
/// Preserves Mc_SaveData.field_21 / field_23 across the bulk copy.
void func_8009407C(void)
{
    u8* src;
    s32 saveField23;
    s32 saveField21;
    s32 bank;
    s32 t;
    u8* base;

    src         = (u8*)D_8005C374;
    bank        = 0;
    saveField23 = Mc_SaveData.field_23;
    saveField21 = Mc_SaveData.field_21;
    if (D_80071094 == 0x10) {
        src = (u8*)0x80600100;
    }
    printf(D_80093830, Mc_SaveData.field_7, Mc_SaveData.field_6);

    memcpy(&Mc_SaveData, src, sizeof(McSaveData));
    src += sizeof(McSaveData);

    memcpy((u8*)&Wip_SysConfig + bank * 0x40, src, 0x40);
    src += 0x40;

    memcpy(D_800733F0[bank], src, 0x6C);
    src += 0x6C;

    memcpy(D_800734C8, src, 0xB0);
    src += 0xB0;

    memcpy(D_80073628, src, 0x24);
    src += 0x24;

    /* bank * 0xE4, split so GCC interleaves lui of D_80073670 after first sll */
    t    = bank * 8;
    base = (u8*)D_80073670;
    memcpy(base + ((t - bank) * 8 + bank) * 4, src, 0xE4);
    src += 0xE4;

    memcpy(D_80073838, src, 0xA4);
    src += 0xA4;

    memcpy(&D_80073980[bank * 0x100], src, 0x100);

    Mc_SaveData.field_23 = saveField23;
    Mc_SaveData.field_21 = saveField21;
    if (Fs_StageCdfIsAvailable(Mc_SaveData.field_7) != 1) {
        D_80071086 = 1;
    }
    printf(D_80093830, Mc_SaveData.field_7, Mc_SaveData.field_6);
}

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

void func_80094A08(Task* arg0)
{
    u8             param1[4];
    u8             param2[4];
    s32            next;
    register Task* task asm("s0");

    task = arg0;
    switch (task->field_30) {
        case 0:
            Display_State.field_100 = 0;
            D_80094CA8              = 1;
            if ((Display_State.field_112 < 0) || (D_800710AC != 0)) {
                next       = 6;
                D_80094CA8 = 0;
            } else {
                Display_SpawnWithOt(&D_80094C8C, 1, 0, 0);
                Display_State.field_103 = 1;
                next                    = task->field_30 + 1;
            }
            task->field_30 = next;
            return;
        case 1:
        case 2:
            task->field_30 = task->field_30 + 1;
            return;
        case 3:
            if (D_80094CA8 != 0) {
                Task_Spawn(0, 2, 0x80000000, 0);
            } else {
                Task_Spawn(0, 2, 0, 0);
            }
            /* fallthrough */
        case 4:
            task->field_30 = task->field_30 + 1;
            return;
        case 5:
            SetDispMask(1);
            D_800710AC = 1;
            Task_Kill(task);
            return;
        case 6:
            param1[3] = 0;
            param1[2] = 0;
            param1[0] = 2;
            param2[0] = 0;
            param2[1] = 0;
            param2[2] = 0;
            param2[3] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
            task->field_30 = task->field_30 + 1;
            /* fallthrough */
        case 7:
            if (CdCmd_IsIdle() & 0xFFFF) {
                task->field_30 = 3;
            }
            return;
    }
}

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
