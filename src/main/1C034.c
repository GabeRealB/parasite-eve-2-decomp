#include "common.h"
#include "main/task.h"
#include "main/pad.h"

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/1C034", func_8002B834);

void func_8002BA9C(s32 r, s32 g, s32 b, s32 mode)
{
    TILE*     p;
    DR_TPAGE* dr;
    s8        yoff;

    p          = (TILE*)D_80071190;
    D_80071190 = (DR_TPAGE*)(p + 1);
    setlen(p, 3);
    setcode(p, 0x62);
    p->x0 = -0xA0;
    p->r0 = r;
    p->g0 = g;
    p->b0 = b;
    yoff  = Display_State.unknown_109[0];
    p->w  = 0x140;
    p->h  = 0xF0;
    p->y0 = -0x78 - yoff;
    addPrim(D_800710A0 - 0x10, p);

    dr         = (DR_TPAGE*)D_80071190;
    D_80071190 = dr + 1;
    setDrawTPage(dr, 0, 1, (mode & 3) << 5);
    addPrim(D_800710A0 - 0x10, dr);
}

void func_8002BB9C(void)
{
    u8* ptr;
    u32 i;

    ptr = (u8*)D4F564_8005ED64;
    for (i = 0; i < sizeof(GStruct14); i++) {
        *ptr++ = 0;
    }
    Display_State.field_101 = 0;
}

void func_8002BBC8(void)
{
    Task_ResetDefaultList();
    func_80028718();
    Mem_Init();
    Task_Spawn(0, 9, 0, 0);
}

void func_8002BC0C(Task* arg0)
{
    u8*         ptr;
    u32         i;
    s32         saved;
    CdCmdQueue* p;

    p     = &CdCmd_Queue;
    saved = D_80072189;
    ptr   = (u8*)D4F564_8005ED64;
    for (i = 0; i < sizeof(GStruct14); i++) {
        *ptr++ = 0;
    }
    Display_State.field_101 = 0;
    Display_State.field_12e = 1;
    p->field_248            = 1;
    p->field_244            = 1;
    D_800710A8.field_4      = 1;
    func_800303AC();
    do {
        D_80072189 = saved;
    } while (0);
    arg0->field_30 = arg0->field_30 + 1;
}

void func_8002BCA8(Task* arg0)
{
    void* temp_v0;

    func_800280F4(0);
    temp_v0        = func_800486F0(D_800611C8, 0, 1, 0, 0);
    arg0->field_20 = temp_v0;
    if (temp_v0 != 0) {
        Display_State.field_11e  = 0xFF;
        D4F564_8005ED64->field_2 = 1;
        arg0->field_2a           = 0x10;
        arg0->field_30           = arg0->field_30 + 1;
    }
}

void func_8002BD24(Task* arg0)
{
    UiObject* obj;

    obj = arg0->field_20;
    if (obj->field_2E == -1) {
        func_80048838(obj, obj->field_28);
        Display_State.field_11e  = 0;
        D4F564_8005ED64->field_2 = 0;
        if (D_80072311 == 1) {
            func_800260B0(0);
        } else {
            func_800260B0(1);
        }
        func_800429C8(0);
        arg0->field_2a = 0xC;
        arg0->field_30 = arg0->field_30 + 1;
    }
}

void func_8002BDB8(Task* arg0)
{
    arg0->field_2a--;
    if (arg0->field_2a != 0) {
        return;
    }
    Pad_SetCooldown(0);
    arg0->field_30 = arg0->field_30 + 1;
}

void func_8002BE0C(Task* arg0)
{
    if (Display_State.field_101 == 0) {
        Task_Spawn(0, 2, 0, 0);
        func_8003DB48(0x5010);
        Task_Kill(arg0);
        return;
    }
    Display_State.field_10b = 1;
    Task_Kill(arg0);
    Task_ResetDefaultList();
    func_80028718();
    Mem_Init();
    Task_Spawn(0, 9, 0, 0);
}

void func_8002BEA8(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_800134BC;
    sp.funcs[arg0->field_30](arg0);
}

void func_8002BF10(Task* arg0)
{
    ((GStructOverlayAt4*)D4F564_8005ED64)->field_4 =
        ((GStructOverlayAt4*)&Mc_SaveData)->field_4;
    D_8007A394     = 0;
    arg0->field_30 = arg0->field_30 + 1;
}

void func_8002BF58(Task* arg0)
{
    u8 param1[8];
    u8 param2[8];

    if ((u8)func_80042500() == 0) {
        F12D18_8002252C(&D4F564_8005ED64->field_4, 0);
        param1[3] = 0;
        param1[2] = 0;
        param1[0] = 0;
        param2[0] = 0;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        arg0->field_30 = arg0->field_30 + 1;
    }
}

void func_8002BFD4(void)
{
    D_8005ED68 = 0;
}

void func_8002BFE0(Task* arg0)
{
    if (func_8001D344() != 0) {
        Task_Spawn(0, 0x11, 1, 0);
        Task_Kill(arg0);
    }
}

void func_8002C028(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_800134D0;
    Pad_SetCooldown(0);
    sp.funcs[arg0->field_30](arg0);
}

void func_8002C090(PadState* arg0)
{
    PadState*    pad;
    register u8* temp asm("t0");
    s32          i;
    register u8* p0 asm("a1");
    register u8* p1 asm("a2");
    s32          half;
    u8           val;
    u8           one;

    pad = arg0;
    {
        register void** scratch asm("a0");
        register void*  head asm("v1");
        register void*  alloc asm("v0");

        scratch         = (void**)G_SCRATCH_HEAD;
        p0              = &pad->field_10[0][0].field_0;
        i               = 0;
        one             = 1;
        head            = *scratch;
        p1              = &pad->field_10[0][0].field_1;
        alloc           = (u8*)head - 4;
        temp            = alloc;
        *scratch        = alloc;
        temp[1]         = 0;
        ((u8*)head)[-4] = 0;
    }

    do {
        if (*p0 != 0) {
            half                     = *(volatile u16*)(p1 + 1) - 1;
            *(volatile u16*)(p1 + 1) = half;
            if ((half << 16) == 0) {
                *p0 = 0;
            }
            if (*p1 != 0) {
                *temp = one;
            }
        }
        i  += 1;
        p1 += 4;
        p0 += 4;
    } while (i < 8);

    {
        register u8* p1b asm("a0");

        p0  = &pad->field_10[1][0].field_0;
        i   = 0;
        p1b = &pad->field_10[1][0].field_1;
        do {
            if (*p0 != 0) {
                half                      = *(volatile u16*)(p1b + 1) - 1;
                *(volatile u16*)(p1b + 1) = half;
                if ((half << 16) == 0) {
                    *p0 = 0;
                }
                val = *p1b;
                if (temp[1] < val) {
                    temp[1] = val;
                }
            }
            i   += 1;
            p1b += 4;
            p0  += 4;
        } while (i < 8);
    }

    if (D_80072189 == 0) {
        pad->field_5A = temp[0];
        pad->field_5B = temp[1];
    } else {
        pad->field_5A = 0;
        pad->field_5B = 0;
    }

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 4;
}

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C1D8);

INCLUDE_ASM("main/nonmatchings/1C034", func_8002C5A4);
