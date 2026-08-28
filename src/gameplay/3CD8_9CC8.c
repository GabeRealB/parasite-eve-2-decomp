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

#define gte_rtps_real()   __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtv0_real()   __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")

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

void Gp_InitState1C(Task* arg0)
{
    GpState1C* p;
    s32        val;

    val = 0;
    p   = Mem_Calloc(0x1C, val);
    if (p == NULL) {
        Task_Kill(arg0);
        return;
    }

    Gp_State1CTask = arg0;
    Gp_State1C     = p;
    arg0->idMap    = (TaskIdMap*)p;
    p->field_0     = 0;
    p->field_2     = 0;
    p->field_4     = 0;
    p->field_6     = 1;
    p->field_8     = 0;
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
    Gp_InitRoomCoords();

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

void Gp_TickState1C(void)
{
    GpState1C*  p;
    GpStateF0*  q;
    GpStateC08* r;
    s16         temp;

    if (Gp_State1C->field_0 <= 0) {
        Gp_State1C->field_0 = 0;
    }
    if (Gp_State1C->field_2 <= 0) {
        Gp_State1C->field_2 = 0;
    }
    temp = Gp_State1C->field_16;
    if ((temp == 1) && (Gp_StateF0.field_0 != temp)) {
        SndEvt_EnqueueType7(0xFF0D, 1);
        Gp_State1C->field_2 = 0;
    }
    p           = Gp_State1C;
    q           = &Gp_StateF0;
    p->field_16 = q->field_0;
    p->field_4  = q->field_4 | (p->field_1A & 0x100);
    p->field_E  = q->field_4 | (p->field_1A & 0x180);
    p->field_1A = 0;
    if (!(p->field_4 & 1)) {
        Gp_DecRoomCoordRefs();
    }
    if (Gp_State1C->field_E >= 4) {
        r           = &Gp_StateC08;
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
        Gp_WorldToLocal(world, &arg1->workm, &arg1->coord);
        arg1->sub = (GsCOORDINATE2*)((u8*)world - OFFSET_OF(GsCOORDINATE2, workm));
        arg1->flg = 0;
        Gp_UpdateCoord(arg1);
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
    Gp_State1C->field_C = arg0 + 1;
}

void Gp_DecRoomCoordRefs(void)
{
    s32        i;
    GpCoord64* p;

    p = Gp_RoomCoords;
    for (i = 0; i < 8; i++) {
        if (p->field_0 != 0) {
            p->field_0--;
        }
        p++;
    }
}

void Gp_InitRoomCoords(void)
{
    s32        i;
    GpCoord64* p;

    p = Gp_RoomCoords;
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

GpEffWork* func_800EA478(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, SVECTOR* arg3)
{
    Task*      task;
    GpEffWork* mem;
    s32        bank;

    bank = (arg0 >> 16) & 0x7FFF;
    if ((arg0 >= 0) && (Gp_State1C->field_0 >= 0x81)) {
        return NULL;
    }
    arg0 &= 0xFFFF;
    if (arg0 == 0) {
        return NULL;
    }
    task = Task_Spawn(bank, arg0, arg2, 0);
    if (task == NULL) {
        return NULL;
    }
    mem = Mem_Calloc(sizeof(GpEffWork), false);
    if (mem == NULL) {
        Task_Kill(task);
        return NULL;
    }
    Gp_State1C->field_0++;

    if (arg1 != NULL) {
        GsCOORDINATE2* coord;
        SVECTOR        vec;

        coord = (GsCOORDINATE2*)((GameActorExt*)task->extra)->field_8;
        memset(&vec, 0, sizeof(vec));
        mem->field_C = arg3;
        if (arg3 == NULL) {
            arg3 = &vec;
        }
        mem->field_18 = arg3->vx;
        mem->field_1A = arg3->vy;
        mem->field_1C = arg3->vz;
        if (arg1->sub == &D_80070F10) {
            coord->coord = arg1->coord;
            gte_SetRotMatrix(&arg1->coord);
            gte_SetTransMatrix(&arg1->coord);
            gte_ldv0(arg3);
            gte_rtv0tr_real();
            gte_stlvnl(coord->coord.t);
        } else {
            Gp_UpdateCoord(arg1);
            coord->workm = arg1->workm;
            gte_SetRotMatrix(&arg1->workm);
            gte_SetTransMatrix(&arg1->workm);
            gte_ldv0(arg3);
            gte_rtv0tr_real();
            gte_stlvnl(coord->workm.t);
            Gp_WorldToLocal(&D_80070F10.workm, &coord->workm, &coord->coord);
        }
        coord->sub = &D_80070F10;
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        mem->field_8 = arg1;
    } else {
        GsCOORDINATE2* coord;
        SVECTOR        vec;

        coord = (GsCOORDINATE2*)((GameActorExt*)task->extra)->field_8;
        memset(&vec, 0, sizeof(vec));
        mem->field_C = arg3;
        if (arg3 == NULL) {
            arg3 = &vec;
        }
        mem->field_18 = arg3->vx;
        mem->field_1A = arg3->vy;
        mem->field_1C = arg3->vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(arg3);
        gte_rtv0tr_real();
        gte_stlvnl(coord->coord.t);
        coord->sub = &D_80070F10;
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        mem->field_8 = &D_80070F10;
    }

    task->spawnArg2    = mem;
    task->exitCallback = Gp_KillState1CTask;
    mem->field_0       = task;
    mem->field_20      = 0;
    mem->field_22      = 0;
    mem->field_24      = 0;
    mem->field_26      = 0;
    mem->field_28      = 0;
    mem->field_2A      = 0;
    mem->field_4       = 0;
    mem->field_10      = 0;
    mem->field_12      = 0;
    mem->field_14      = 0;
    return mem;
}

void Gp_DrawFadeQuad(u8* arg0, s32 arg1)
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

void func_800EAEB8(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
{
    register void**         scratch asm("a1");
    register u8*            head asm("a2");
    register GpRingScratch* block asm("s2");
    POLY_G4*                prim;
    DR_TPAGE*               dr;
    register s32            ang asm("s3");
    register s32            ang2 asm("s1");
    s32                     otz;
    u16                     vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    __asm__ volatile("" ::"r"(head));
    {
        register u16 vx asm("v0");
        vx                                      = *(u16*)&arg0->workm.t[0];
        ((GpRingScratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (GpRingScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        __asm__ volatile("" ::"r"(head));
        block->otz++;
        block->step = ((s16)arg1 * 64) / block->otz;
        ang         = 0;
        do {
            prim       = (POLY_G4*)D_80071190;
            D_80071190 = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->step * rcos(ang)) >> 12);
            ang2     = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            ang2     = ang + 0x200;
            prim->x3 = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
            otz = block->otz;
            setSemiTrans(prim, 1);
            dr         = D_80071190;
            D_80071190 = dr + 1;
            setDrawTPage(dr, 0, 1, 0x2A);
            addPrim((u_long*)(((((u32)otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    dr);
            ang = ang2;
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

void func_800EB2C8(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, u16 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s32              clutIdx;
    s32              ang2;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = *(u16*)&arg0->workm.t[1];
    vz                                        = *(u16*)&arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    clutIdx = arg3 >> 12;
    __asm__ volatile("" ::"r"(clutIdx));
    arg3 &= 0xFFF;
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim       = (POLY_FT4*)D_80071190;
        D_80071190 = (DR_TPAGE*)(prim + 1);
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        prim->tpage = 0x2A;
        setClut(prim, Gp_QuadClutX[clutIdx], 0x10B);
        u0 = arg1 << 5;
        u1 = u0 + 0x1F;
        setUV4(prim, u0, 0x18, u1, 0x18, u0, 0x37, u1, 0x37);
        block->dx = (((arg2 * 31) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8_9CC8", func_800EB6E8);

INCLUDE_ASM("gameplay/nonmatchings/3CD8_9CC8", func_800EB9B0);

INCLUDE_ASM("gameplay/nonmatchings/3CD8_9CC8", func_800EBF18);

INCLUDE_ASM("gameplay/nonmatchings/3CD8_9CC8", func_800EC47C);

void Gp_FadeWaveTask(Task* arg0)
{
    GpState1C* p;
    GpEffWork* mem;
    u16        color;
    u8         rgb[3];

    p   = Gp_State1C;
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
        color  = Gp_FadeQuadColors[(cln(arg0->spawnArg1 << 12) / 2839) & 7];
        rgb[0] = (mem->field_26 * ((color >> 8) & 0xF)) >> 3;
        rgb[1] = (mem->field_26 * ((color >> 4) & 0xF)) >> 3;
        rgb[2] = (mem->field_26 * (color & 0xF)) >> 3;
        Gp_DrawFadeQuad(rgb, color >> 12);
    }
    if (mem->field_24 >= 0x700) {
        Gp_State1C->field_0--;
        Mem_Free(mem);
        Task_Kill(arg0);
    }
}

void Gp_ReleaseState1CMem(void* arg0, Task* arg1)
{
    Gp_State1C->field_0--;
    Mem_Free(arg0);
    Task_Kill(arg1);
}

void Gp_KillState1CTask(Task* arg0)
{
    void* mem;

    mem = arg0->spawnArg2;
    Gp_State1C->field_0--;
    Mem_Free(mem);
    Task_Kill(arg0);
}

void Gp_PulseState1C(void)
{
    Gp_State1C->field_1A |= 0x100;
}

void Gp_AddTpage(P_TAG* arg0, s32 arg1, s32 arg2)
{
    DR_TPAGE* p;

    setSemiTrans(arg0, 1);
    p          = D_80071190;
    D_80071190 = p + 1;
    setlen(p, 1);
    p->code[0] = 0xE100020A | ((arg1 & 3) << 5);
    addPrim(Gpu_CurrentOt + (arg2 >> 4), p);
}

void Gp_AddTpageShift(P_TAG* arg0, s32 arg1, s32 arg2)
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
    if (!(Gp_State1C->field_10 & 1)) {
        func_800EA478(0x800600E8, 0, 0, 0);
    }
}

void Gp_SetState1CPe(s32 arg0)
{
    Gp_State1C->field_18 = (u8)arg0;
    func_800EA478(0x8006000F, 0, (u8)arg0, 0);
}

void func_800ECA54(void)
{
    GpState1C* p;

    p = Gp_State1C;
    if (!(p->field_10 & 0x80)) {
        p->field_12 &= 0xF7FF;
        func_800EA478(0x8006000E, 0, 0, 0);
    }
}
