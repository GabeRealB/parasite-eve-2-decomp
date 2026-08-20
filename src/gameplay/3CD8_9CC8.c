#include "common.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern TaskFuncTable3 D_80097678;
extern s32            D_80115720;
extern s32            D_80115724;
extern s32            D_80115728;
extern s32            D_8011572C;
extern s32            D_80115730;
extern s32            D_80115734;
extern s32            D_80115738;
extern s32            D_8011573C;
extern s32            D_80115744;
extern s32            D_8011574C;
extern s32            D_80115750;
extern s32            D_80115754;
extern s32            D_80115758;
extern s32            D_80111B70[];
extern s32            D_80111BC0[];
extern s32            D_80111C58[];
extern s32            D_80111CF0[];
extern s32            D_80111DB4[];

void func_800E9CC8(Task* arg0)
{
    GpState1C* p;
    s32        val;

    val = 0;
    p   = Mem_Calloc(0x1C, val);
    if (p == NULL) {
        Task_Kill(arg0);
        return;
    }

    D_80115748  = arg0;
    D_80115740  = p;
    arg0->idMap = (TaskIdMap*)p;
    p->field_0  = 0;
    p->field_2  = 0;
    p->field_4  = 0;
    p->field_6  = 1;
    p->field_8  = 0;
    func_800EA478(0x60053, 0, 0, 0);

    D_80115758  = 0;
    D_8011572C  = 0;
    D_80115750  = 0;
    D_80115730  = 0;
    D_80115734  = 0;
    D_80115754  = 0;
    D_80115728  = 0;
    D_80115744  = 0;
    D_8011573C  = 0;
    D_80115720  = 0;
    D_8011574C  = 0;
    p->field_A  = 0;
    p->field_C  = 0;
    p->field_E  = 0;
    p->field_10 = 0;
    p->field_12 = 0;
    p->field_14 = 0;
    p->field_16 = 0;
    p->field_18 = 0;
    p->field_1A = 0;
    D_80115738  = 0;
    D_80115724  = 0;
    arg0->state++;
    func_800EA3EC();

    switch (Game_Session->field_7) {
        case 1:
            val = D_80111B70[Game_Session->field_6 - 1];
            break;
        case 2:
            val = D_80111BC0[Game_Session->field_6 - 1];
            break;
        case 3:
            val = D_80111C58[Game_Session->field_6 - 1];
            break;
        case 4:
            val = D_80111CF0[Game_Session->field_6 - 1];
            break;
        case 5:
            val = D_80111DB4[Game_Session->field_6 - 1];
            break;
    }

    if (val != 0) {
        func_800EA478(val | 0x60000, 0, 0, 0);
    }
    Task_Spawn(6, 0x80000007, 0, 0);
}

void func_800E9EFC(void)
{
    GpState1C*  p;
    GpStateF0*  q;
    GpStateC08* r;
    s16         temp;

    if (D_80115740->field_0 <= 0) {
        D_80115740->field_0 = 0;
    }
    if (D_80115740->field_2 <= 0) {
        D_80115740->field_2 = 0;
    }
    temp = D_80115740->field_16;
    if ((temp == 1) && (D_801153F0.field_0 != temp)) {
        SndEvt_EnqueueType7(0xFF0D, 1);
        D_80115740->field_2 = 0;
    }
    p           = D_80115740;
    q           = &D_801153F0;
    p->field_16 = q->field_0;
    p->field_4  = q->field_4 | (p->field_1A & 0x100);
    p->field_E  = q->field_4 | (p->field_1A & 0x180);
    p->field_1A = 0;
    if (!(p->field_4 & 1)) {
        func_800EA3B4();
    }
    if (D_80115740->field_E >= 4) {
        r           = &D_80114C08;
        r->field_10 = 0;
        r->field_C  = 0;
        r->field_12 = 0;
        r->field_D  = 0;
        r->field_14 = 0;
        r->field_F  = 0;
        r->field_16 = 0;
        r->field_17 = 0;
        func_8010A1B0(1, 0x80);
    }
}

s32 func_800EA02C(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1)
{
    void**        scratch;
    u8*           head;
    GpRayScratch* block;
    SVECTOR*      dir;
    MATRIX*       world;
    s32           ret;
    u16           vz;

    scratch                                = (void**)G_SCRATCH_HEAD;
    head                                   = *scratch;
    block                                  = (GpRayScratch*)(head - 0x10);
    ((GpRayScratch*)(head - 0x10))->pos.vx = *(u16*)&arg0->workm.t[0];
    block->pos.vy                          = *(u16*)&arg0->workm.t[1];
    vz                                     = *(u16*)&arg0->workm.t[2];
    *scratch                               = block;
    block->dir.vx                          = 0;
    block->dir.vy                          = 0x1000;
    block->dir.vz                          = 0;
    block->pos.vz                          = vz;
    gte_SetRotMatrix(&D_80070F34);
    dir = (SVECTOR*)(head - 8);
    gte_ldv0(dir);
    gte_rtv0_real();
    gte_stsv(dir);
    block->dir.vx += ((GpRayScratch*)(head - 0x10))->pos.vx;
    block->dir.vy += block->pos.vy;
    block->dir.vz += block->pos.vz;
    ret            = func_800DE7CC(dir, &block->pos, dir, NULL);
    if (ret == 1) {
        world            = &D_80070F34;
        arg1->workm.t[0] = block->dir.vx;
        arg1->workm.t[1] = block->dir.vy;
        arg1->workm.t[2] = block->dir.vz;
        func_800A8864(world, &arg1->workm, &arg1->coord);
        arg1->sub = (GsCOORDINATE2*)((u8*)world - OFFSET_OF(GsCOORDINATE2, workm));
        arg1->flg = 0;
        func_80098F58(arg1);
    }
    *scratch = (u8*)*scratch + 0x10;
    return ret;
}

s32 func_800EA1A8(VECTOR3* arg0, VECTOR3* arg1)
{
    void**        scratch;
    u8*           head;
    GpRayScratch* block;
    SVECTOR*      dir;
    s32           ret;
    u16           vz;

    scratch                                = (void**)G_SCRATCH_HEAD;
    head                                   = *scratch;
    block                                  = (GpRayScratch*)(head - 0x10);
    ((GpRayScratch*)(head - 0x10))->pos.vx = *(u16*)&arg0->vx;
    block->pos.vy                          = *(u16*)&arg0->vy;
    vz                                     = *(u16*)&arg0->vz;
    *scratch                               = block;
    block->dir.vx                          = 0;
    block->dir.vy                          = 0x1000;
    block->dir.vz                          = 0;
    block->pos.vz                          = vz;
    gte_SetRotMatrix(&D_80070F34);
    dir = (SVECTOR*)(head - 8);
    gte_ldv0(dir);
    gte_rtv0_real();
    gte_stsv(dir);
    block->dir.vx += ((GpRayScratch*)(head - 0x10))->pos.vx;
    block->dir.vy += block->pos.vy;
    block->dir.vz += block->pos.vz;
    ret            = func_800DE7CC(dir, &block->pos, dir, NULL);
    if (ret == 1) {
        arg1->vx = block->dir.vx;
        arg1->vy = block->dir.vy;
        arg1->vz = block->dir.vz;
        ret      = block->dir.vy - block->pos.vy;
        if (ret == 0) {
            ret = 1;
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
    return ret;
}

s32 func_800EA318(s16 arg0, s16 arg1, s16 arg2)
{
    s32 result;

    result = 0;
    if (arg2 != 0) {
        result = (arg1 * (arg0 << 1)) / arg2;
        if (result >= 0x100) {
            result = 0xFF;
        } else if (result == 0) {
            result = -1;
        }
    }
    return result;
}

void func_800EA3A0(s32 arg0)
{
    D_80115740->field_C = arg0 + 1;
}

void func_800EA3B4(void)
{
    s32        i;
    GpCoord64* p;

    p = D_80114F30;
    for (i = 0; i < 8; i++) {
        if (p->field_0 != 0) {
            p->field_0--;
        }
        p++;
    }
}

void func_800EA3EC(void)
{
    s32        i;
    GpCoord64* p;

    p = D_80114F30;
    for (i = 0; i < 8; i++) {
        p->coord.sub = &D_80070F10;
        p->field_0   = 0;
        p++;
    }
}

void func_800EA420(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80097678;
    sp.funcs[arg0->state](arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8_9CC8", func_800EA478);

void func_800EA858(u8* arg0, s32 arg1)
{
    POLY_F4*  p;
    DR_TPAGE* dr;
    s32       x0;
    s32       x1;
    s32       yTop;
    s32       yBot;

    arg1 &= 3;
    x0    = -0xA0;
    x1    = 0xA0;
    yTop  = -0x78;
    yBot  = 0x78;

    p          = (POLY_F4*)D_80071190;
    D_80071190 = (DR_TPAGE*)(p + 1);
    setPolyF4(p);
    setRGB0(p, arg0[0], arg0[1], arg0[2]);
    p->x0 = x0;
    p->y0 = yTop - Display_State.vramYOffset;
    p->x1 = x1;
    p->y1 = yTop - Display_State.vramYOffset;
    p->x2 = x0;
    p->y2 = yBot - Display_State.vramYOffset;
    p->x3 = x1;
    p->y3 = yBot - Display_State.vramYOffset;
    addPrim((u_long*)(((((u32)0x10 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), p);

    setSemiTrans(p, 1);
    dr         = D_80071190;
    D_80071190 = dr + 1;
    setDrawTPage(dr, 0, 1, 0xA | (arg1 << 5));
    addPrim((u_long*)(((((u32)0x10 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), dr);
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8_9CC8", func_800EAA0C);

INCLUDE_ASM("gameplay/nonmatchings/3CD8_9CC8", func_800EAEB8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8_9CC8", func_800EB2C8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8_9CC8", func_800EB6E8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8_9CC8", func_800EB9B0);

INCLUDE_ASM("gameplay/nonmatchings/3CD8_9CC8", func_800EBF18);

INCLUDE_ASM("gameplay/nonmatchings/3CD8_9CC8", func_800EC47C);

void func_800EC674(Task* arg0)
{
    GpState1C* p;
    GpEffWork* mem;
    u16        color;
    u8         rgb[3];

    p   = D_80115740;
    mem = arg0->spawnArg2;
    if (p->field_18 != arg0->spawnArg1) {
        p->field_0--;
        Mem_Free(mem);
        Task_Kill(arg0);
        return;
    }

    mem->field_24 += 0x180;
    mem->field_26  = rsin(mem->field_24) >> 5;
    if (arg0->spawnArg1 != 0) {
        color  = D_80111EC0[(cln(arg0->spawnArg1 << 12) / 2839) & 7];
        rgb[0] = (mem->field_26 * ((color >> 8) & 0xF)) >> 3;
        rgb[1] = (mem->field_26 * ((color >> 4) & 0xF)) >> 3;
        rgb[2] = (mem->field_26 * (color & 0xF)) >> 3;
        func_800EA858(rgb, color >> 12);
    }
    if (mem->field_24 >= 0x700) {
        D_80115740->field_0--;
        Mem_Free(mem);
        Task_Kill(arg0);
    }
}

void func_800EC7E4(void* arg0, Task* arg1)
{
    D_80115740->field_0--;
    Mem_Free(arg0);
    Task_Kill(arg1);
}

void func_800EC824(Task* arg0)
{
    void* mem;

    mem = arg0->spawnArg2;
    D_80115740->field_0--;
    Mem_Free(mem);
    Task_Kill(arg0);
}

void func_800EC868(void)
{
    D_80115740->field_1A |= 0x100;
}

void func_800EC888(P_TAG* arg0, s32 arg1, s32 arg2)
{
    DR_TPAGE* p;

    setSemiTrans(arg0, 1);
    p          = D_80071190;
    D_80071190 = p + 1;
    setlen(p, 1);
    p->code[0] = 0xE100020A | ((arg1 & 3) << 5);
    addPrim(Gpu_CurrentOt + (arg2 >> 4), p);
}

void func_800EC914(P_TAG* arg0, s32 arg1, s32 arg2)
{
    DR_TPAGE* p;

    setSemiTrans(arg0, 1);
    p          = D_80071190;
    D_80071190 = p + 1;
    p->code[0] = 0xE100020A | ((arg1 & 3) << 5);
    setlen(p, 1);
    addPrim((u_long*)(((((u32)arg2 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), p);
}

void func_800EC9C8(void)
{
    if (!(D_80115740->field_10 & 1)) {
        func_800EA478(0x800600E8, 0, 0, 0);
    }
}

void func_800ECA10(s32 arg0)
{
    D_80115740->field_18 = (u8)arg0;
    func_800EA478(0x8006000F, 0, (u8)arg0, 0);
}

void func_800ECA54(void)
{
    GpState1C* p;

    p = D_80115740;
    if (!(p->field_10 & 0x80)) {
        p->field_12 &= 0xF7FF;
        func_800EA478(0x8006000E, 0, 0, 0);
    }
}
