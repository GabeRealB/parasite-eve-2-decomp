#include "common.h"
#include "main/gamemain.h"
#include "main/display.h"

#include <psyq/libetc.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

typedef struct _GsCOORDINATE2 {
    unsigned long          flg;
    MATRIX                 coord;
    MATRIX                 workm;
    void*                  param;
    struct _GsCOORDINATE2* super;
    struct _GsCOORDINATE2* sub;
} GsCOORDINATE2;

extern MATRIX GsWSMATRIX;

void func_800271D4(void)
{
    s32 flag; // The indirection is required.

    SetDispMask(0);
    F04CF8_800148A0();
    ResetCallback();
    F04CF8_80014A98(1);
    VSync(10);

    GameResetScratchHead();
    D_8005EC64++;
    func_800144F8(0, 0);
    Mem_Init();
    Task_ResetDefaultList();
    func_80028718();
    func_800281D4();

    Mem_Set(&Display_State, 0, sizeof(Display_State));
    Display_State.field_120 = 1;
    Display_State.field_124 = 0;
    Display_State.field_101 = 0;
    Display_State.field_1d  = -1;
    Display_State.field_1e  = 0;
    Display_State.field_10d = 0;
    Display_State.field_0   = 0;
    Display_State.field_4   = 0;
    Display_State.field_8   = 0;
    Display_State.field_c   = 0;
    Display_State.field_10  = 0;
    Display_State.field_14  = 0;
    func_800280F4(0);

    D_8005EC70 = 0;
    F179D4_ClearOTag(0);
    F179D4_ClearOTag(1);
    func_8004CFE8();
    func_80053FA0(0);
    F04CF8_800148EC();
    VSyncCallback(func_80027498);

    flag                   = 1;
    Display_State.field_1f = flag;
    func_8003DB48(0x1010);
    Mem_Set(D_800626A8, 0, 0x1C);
}

void func_80027F48(s32 arg0);
void func_80020058(void);

void func_8002731C(s32 arg0)
{
    s32 mode;
    u8  saved;

    mode = D_80070E38 & 0xF;
    if (mode != 2) {
        PutDrawEnv(&Display_State.field_48[arg0]);
        PutDispEnv(&Display_State.field_20[arg0]);
        if (mode == 0) {
            if (D_8006EC30 != 0) {
                func_80027F48(arg0);
                saved                  = Display_State.field_1f;
                Display_State.field_1f = arg0;
                func_80020058();
                Display_State.field_1f = saved;
            }
            DrawOTag(D_80070EE8[Display_State.field_114].field_10);
        } else if (D_8006EC30 == 2) {
            func_80027F48(arg0);
        } else if (D_8006EC30 == 3) {
            func_800149E8(D4F564_8005ED64->field_7, D4F564_8005ED64->field_6, arg0);
        }
        if ((s8)D_80070E38 < 0x10) {
            DrawOTag(D_8007A0E8[arg0].tag);
        }
    }
}

void func_80027498(void)
{
    s32           temp_s4;
    s32           temp_s0;
    s32           stride; // Forces s0*0x14 before (ds+0x20); required for match.
    DisplayState* ds;
    DRAWENV*      drawBase;
    DISPENV*      dispBase;

    temp_s4 = VSync(1);
    if ((s32)D_8005EC70 >= 0) {
        if (((temp_s4 & 0xFFFF) + D_8005EC78) > (D_8005EC6C >> 1)) {
            ds = &Display_State;
            if (ds->field_108 == 0) {
                temp_s0  = D_8005EC70;
                drawBase = ds->field_48;
                PutDrawEnv(&drawBase[temp_s0]);
                stride   = temp_s0 * 0x14;
                dispBase = ds->field_20;
                PutDispEnv(&dispBase[temp_s0]);
                if (ds->field_100 != 0) {
                    func_80027F48(temp_s0);
                }
                func_80020058();
                if (ds->field_104 == 0) {
                    DrawOTag(D_80070EE8[temp_s0].field_10);
                }
                D_8005EC70 = -1;
            } else if (ds->field_108 == 1) {
                func_8002731C(D_8005EC70);
                D_8005EC70 = -1;
            }
        }
    }
    D_80070F64 -= 1;
    if (Display_State.field_1e == 0) {
        Display_State.field_0 += 1;
    }
    Display_State.field_c += 1;
    func_80057564();
    func_8004D008();
    func_8002C1D8();
    D_8005EC74 = VSync(1) - (temp_s4 & 0xFFFF);
}

INCLUDE_ASM("main/nonmatchings/gamemain", func_8002764C);

INCLUDE_ASM("main/nonmatchings/gamemain", func_8002785C);

void func_80027E7C(void)
{
    MATRIX*        m;
    GsCOORDINATE2* c1;
    GsCOORDINATE2* c2;
    GsCOORDINATE2* c3;
    s32            one;

    *(s32*)&D_80070E94 = ONE;
    one                = ONE;
    m                  = &D_80070E94;
    c1                 = (GsCOORDINATE2*)((u8*)m - OFFSET_OF(GsCOORDINATE2, coord));
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[1][1] = one;
    *(s32*)&m->m[2][0] = 0;
    m->m[2][2]         = one;
    c1->sub            = NULL;
    c1->coord.t[0]     = 0;
    c1->coord.t[1]     = 0;
    c1->coord.t[2]     = 0x8000;
    c1->flg            = 0;

    *(s32*)&D_80070E44 = one;
    m                  = &D_80070E44;
    c2                 = (GsCOORDINATE2*)((u8*)m - OFFSET_OF(GsCOORDINATE2, coord));
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[1][1] = one;
    *(s32*)&m->m[2][0] = 0;
    m->m[2][2]         = one;
    c2->sub            = c1;
    c2->coord.t[0]     = 0;
    c2->coord.t[1]     = 0;
    c2->coord.t[2]     = 0;
    c2->flg            = 0;

    *(s32*)&D_80070F14 = one;
    m                  = &D_80070F14;
    c3                 = (GsCOORDINATE2*)((u8*)m - OFFSET_OF(GsCOORDINATE2, coord));
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[1][1] = one;
    *(s32*)&m->m[2][0] = 0;
    m->m[2][2]         = one;
    c3->sub            = c2;
    c3->coord.t[0]     = 0;
    c3->coord.t[1]     = 0;
    c3->coord.t[2]     = 0;
    c3->flg            = 0;

    gte_SetGeomScreen(0x400);

    *(s32*)&GsWSMATRIX = one;
    m                  = &GsWSMATRIX;
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[1][1] = one;
    *(s32*)&m->m[2][0] = 0;
    m->m[2][2]         = one;
}

INCLUDE_ASM("main/nonmatchings/gamemain", func_80027F48);

void func_800280F4(s32 arg0)
{
    if (arg0 == 0) {
        Display_State.field_10a = 1;
        D_8005EC68              = 0;
        D_8005EC6C              = 0x106;
    } else if (arg0 == 1) {
        Display_State.field_10a = 2;
        D_8005EC68              = 2;
        D_8005EC6C              = 0x20D;
    } else if (arg0 == 2) {
        Display_State.field_10a = 3;
        D_8005EC68              = 3;
        D_8005EC6C              = 0x313;
    }
}

void F179D4_ClearOTag(s16 tableIdx)
{
    u_long* tableStart = D5F414_OrderingTables + tableIdx * C5F414_OTAG_ENTRIES;
    ClearOTagR(tableStart, C5F414_OTAG_ENTRIES);
    *tableStart = C5F414_OTAG_END_PRIM;
}

void func_800281D4(void)
{
    RECT       rect;
    GStruct35* otCtx;
    u_long*    ot;
    s32        depth;

    if (D_8005EC64 == 1) {
        ResetGraph(0);
    }

    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0x200;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    InitGeom();

    otCtx             = D_80070EE8;
    depth             = 0xA;
    otCtx->field_0    = depth;
    ot                = D5F414_OrderingTables;
    otCtx->field_10   = ot + C5F414_OTAG_ENTRIES - 1;
    otCtx->field_4    = ot;
    otCtx[1].field_0  = depth;
    otCtx[1].field_4  = ot + C5F414_OTAG_ENTRIES;
    otCtx[1].field_10 = ot + 2 * C5F414_OTAG_ENTRIES - 1;
    func_80028290();
    func_80027E7C();
    func_8003AF04();
    Display_State.field_100 = 0;
}

void func_80028290(void)
{
    if (D_8005EC64 == 1) {
        Task_Spawn(0, 0x1F, 0, 0);
    } else {
        Task_Spawn(0, 0x20, 0, 0);
    }
}

void func_800282D8(s32 arg0)
{
    PutDrawEnv(&Display_State.field_48[arg0]);
    PutDispEnv(&Display_State.field_20[arg0]);
    if (Display_State.field_100 != 0) {
        func_80027F48(arg0);
    }
    func_80020058();
    if (Display_State.field_104 == 0) {
        DrawOTag(D_80070EE8[arg0].field_10);
    }
}

// TODO
void GameMain(void)
{
    GameResetScratchHead();
    ResetCallback();
    SetVideoMode(MODE_NTSC);
    func_8004CFC8();
    Mc_InitLib();
    Pad_Init();
    F04CF8_80014A50();
    Mem_Set(&D_800710A8, 0, sizeof(D_800710A8));
    D_8005EC64 = 0;
    func_800271D4();
    func_8002785C();
}

u32 func_80028404(void)
{
    return D_8005EC64;
}
