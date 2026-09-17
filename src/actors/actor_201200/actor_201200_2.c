#include "common.h"
#include "actors/actor_201200.h"
#include "actors/actors_shared_8014a7b0.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "psyq/inline_c.h"

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern MATRIX* D_80073B8C;
extern u32     Gp_LcgState;

void Gp_ArmStateF0(s32 active);
void func_actor_201200_8014A640(Actor201200* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void func_actor_201200_8014C738(Actor201200Ctx* arg0, Actor201200* arg1);
void ActorsShared8014db78(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014DBE0(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014AE60(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014DC98(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014B054(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014B5FC(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014BDFC(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014CA08(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014D0B4(Actor201200Ctx* arg0, Actor201200* arg1);
void func_actor_201200_8014DD50(Actor201200Ctx* arg0, Actor201200* arg1);

extern u8  D_801153F4;
extern s32 D_actor_201200_8014DE64;
extern s32 D_actor_201200_8014DE70;

INCLUDE_ASM("actors/nonmatchings/actor_201200/actor_201200_2", func_actor_201200_8014A88C);

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor201200_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor201200RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)0x1F8003FC;
    ((Actor201200RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor201200RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor201200RangeScratch*)(head - 0xC))->dx *= ((Actor201200RangeScratch*)(head - 0xC))->dx;
    *(Actor201200RangeScratch**)0x1F8003FC        = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)0x1F8003FC                             = head;
    ret                                           = ((Actor201200RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

void func_actor_201200_8014AE60(Actor201200Ctx* arg0, Actor201200* arg1)
{
    Actor201200Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    TmdObject*       obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                 = arg1->field_2C;
        arg0->field_14      = 0;
        obj->field_C        = 0;
        work->field_174     = 5;
        work->field_170     = 1;
        work->field_178     = 0;
        work->obj2C8.flags |= 0x8000;
        work->obj300.flags &= 0x7FFF;
        work->obj338.flags &= 0x7FFF;
        work->obj230.flags |= 0x4000;
        func_actor_201200_8014A640(arg1);
        return;
    }
    func_actor_201200_8014A640(arg1);
    if ((work->field_58 & 2) && work->field_17C >= 0x19) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 0x10) & 7)) {
            work->field_0 = 3;
        }
    }
    coord    = arg1->field_2C->field_8;
    d        = &delta;
    delta.vx = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy    = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz    = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor201200_OutOfRange(d, 2000)) {
        Gp_ArmStateF0(1);
        work->field_0 = 3;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_201200/actor_201200_2", func_actor_201200_8014B054);

INCLUDE_ASM("actors/nonmatchings/actor_201200/actor_201200_2", func_actor_201200_8014B5FC);

static __inline__ void Actor201200_FaceScale(GsCOORDINATE2* coord, s16 s)
{
    Actor201200FaceScratch* head;
    Actor201200FaceScratch* sc;

    head                                      = *(Actor201200FaceScratch**)G_SCRATCH_HEAD;
    sc                                        = head - 1;
    *(Actor201200FaceScratch**)G_SCRATCH_HEAD = sc;
    sc->angle                                 = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&sc->m, sc->angle, 1);
    sc->scale.vx = sc->scale.vy = sc->scale.vz = s;
    ScaleMatrix(&sc->m, &head[-1].scale);
    coord->coord.m[0][0]                       = head[-1].m.m[0][0];
    coord->coord.m[0][1]                       = sc->m.m[0][1];
    coord->coord.m[0][2]                       = sc->m.m[0][2];
    coord->coord.m[1][0]                       = sc->m.m[1][0];
    coord->coord.m[1][1]                       = sc->m.m[1][1];
    coord->coord.m[1][2]                       = sc->m.m[1][2];
    coord->coord.m[2][0]                       = sc->m.m[2][0];
    coord->coord.m[2][1]                       = sc->m.m[2][1];
    coord->coord.m[2][2]                       = sc->m.m[2][2];
    coord->flg                                 = 0;
    *(Actor201200FaceScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_201200_8014BDFC(Actor201200Ctx* arg0, Actor201200* arg1)
{
    SVECTOR          ofs;
    VECTOR           scale;
    Actor201200Work* work;
    TmdObject*       obj;
    s16              s;
    s32              pan;
    s32              id;

    work = arg1->field_1C;
    obj  = arg1->field_2C;
    if (work->field_4 != 0) {
        arg0->field_14        = 1;
        obj->field_C          = 0;
        work->obj2C8.flags   &= 0x7FFF;
        work->obj300.flags   &= 0x7FFF;
        work->obj338.flags   &= 0x7FFF;
        work->obj300.field_18 = Gp_PackObjPair((GpObj50*)arg0, 0);
        work->obj338.field_18 = 0x22121;
        work->field_6         = 0;
        work->obj230.flags   &= 0xBFFF;
        work->savedColorMtx   = work->colorMtx;
        work->field_174       = 0xA;
        work->field_170       = 1;
        work->field_178       = 0;
        work->field_176       = 0x2C;
        func_actor_201200_8014A640(arg1);
        work->obj338.field_10 = arg1->field_2C->field_8->coord.t[0];
        work->obj338.field_12 = arg1->field_2C->field_8->coord.t[1] - 0x190;
        work->obj338.field_14 = arg1->field_2C->field_8->coord.t[2];
        work->obj300.field_10 = arg1->field_2C->field_8->coord.t[0];
        work->obj300.field_12 = arg1->field_2C->field_8->coord.t[1];
        work->obj300.field_14 = arg1->field_2C->field_8->coord.t[2];
        return;
    }
    func_actor_201200_8014A640(arg1);
    switch ((s16)(work->field_6 - 0xD)) {
        case 0:
            ofs.vx = 0x1E;
            ofs.vz = 0x1E;
            ofs.vy = -0x3C;
            Gp_SpawnEff(0x60030, arg1->field_2C->field_8, 0x10080, &ofs);
            ofs.vy = -0xA;
            ofs.vz = -0x50;
            Gp_SpawnEff(0x60030, arg1->field_2C->field_8, 0x10030, &ofs);
            id  = ((arg0->field_8 >> 12) << 8) | 0x400C0004;
            pan = (s8)Gp_GetObjPan((GpObj38*)arg1->field_2C->field_8);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)arg1->field_2C->field_8));
            arg1->field_2C->field_C = 2;
            break;
        case 1:
            work->obj300.field_1C = 0x320;
            work->obj300.flags   |= 0x8000;
            Gp_SpawnEff(0x6009C, &arg1->field_2C->field_8[2], 1, NULL);
            Gp_SpawnScript18((s32)&D_actor_201200_8014DE64, (s32)&D_actor_201200_8014DE70);
            work->eff1A8.field_0 = &arg1->field_2C->field_8[4];
            work->eff1A8.field_4 = 0x120;
            work->eff1A8.field_6 = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->field_2C->field_8[4], NULL, &work->eff1A8);
            break;
        case 2:
            work->obj338.field_1C = 0xC8;
            work->obj300.flags   &= 0x7FFF;
            work->obj338.flags   |= 0x8000;
            break;
        case 3:
            work->obj338.field_1C = 0x190;
            work->eff1A8.field_0  = &arg1->field_2C->field_8[2];
            work->eff1A8.field_4  = 0x100;
            work->eff1A8.field_6  = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->field_2C->field_8[2], NULL, &work->eff1A8);
            break;
        case 4:
            work->obj338.field_1C = 0x320;
            break;
        case 6:
            work->obj338.flags &= 0x7FFF;
            ofs.vx              = arg1->field_2C->field_8->coord.t[0];
            ofs.vy              = arg1->field_2C->field_8->coord.t[1];
            ofs.vz              = arg1->field_2C->field_8->coord.t[2];
            Gp_SpawnEff(0x6009E, &Gfx_ViewCoord, 0, &ofs);
            work->eff1A8.field_0 = &arg1->field_2C->field_8[1];
            work->eff1A8.field_4 = 0x200;
            work->eff1A8.field_6 = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->field_2C->field_8[1], NULL, &work->eff1A8);
            break;
        case 8:
            obj->field_C = 0x80;
            break;
        case 25:
            Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0xC);
            work->field_0 = 0;
            break;
    }
    if ((u16)(work->field_6 - 0xD) < 9) {
        s = 0xBB8 - ((s16)work->field_6 - 0xB) * 0x320;
        if (s < 0) {
            s = 0;
        }
        scale.vx = scale.vy = scale.vz = s;
        work->colorMtx                 = work->savedColorMtx;
        ScaleMatrix(&work->colorMtx, &scale);
        gte_lddp(s);
        gte_ldlvl(work->colorMtx.t);
        gte_gpf12_real();
        gte_stlvl(work->colorMtx.t);
        s = (s16)work->field_6 * 0xB4 + 0x1000;
        if (s > 0x2000) {
            s = 0x2000;
        }
        Actor201200_FaceScale(arg1->field_2C->field_8, s);
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}
