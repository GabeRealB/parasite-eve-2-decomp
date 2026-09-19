#include "common.h"
#include "actors/actor_201200.h"
#include "actors/actor_201200_motion.h"
#include "actors/actors_shared_8014a7b0.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/wipsys.h"
#include "psyq/inline_c.h"

extern MATRIX* D_80073B8C;
extern u32     Gp_LcgState;

void Gp_ArmStateF0(s32 active);
s16  func_actor_201200_80149F50(GsCOORDINATE2* coord, GpRec18* rec, s32 n, SVECTOR* d);
s32  func_actor_201200_8014A49C(GsCOORDINATE2* coord, GpRec18* rec, s32 n);
void func_actor_201200_8014A640(Actor201200* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void ActorsShared8014c738(Actor201200Ctx* arg0, Actor201200* arg1);
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

extern GpPairSrcE D_actor_201200_8014DE54;
extern u8         D_actor_201200_80150DB8[]; // animation bank handed to `func_800B3F84`
extern u8         D_actor_201200_80150E78[];

void func_actor_201200_8014A88C(GpEnemy* arg0, Actor201200* arg1)
{
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   part;
    Actor201200Work* work;
    GpRec18*         hits;
    SVECTOR          sv;
    VECTOR           pos;
    SVECTOR*         p;
    SVECTOR*         q;
    GpObj*           o1;
    GpObj*           o2;
    GpObj*           o3;
    GpObj*           o4;

    obj            = arg1->field_2C;
    coord          = obj->coords;
    work           = memCalloc(sizeof(Actor201200Work), 0);
    arg1->field_1C = work;
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, (Task*)arg1);
        return;
    }
    arg1->field_24 = D_actor_201200_80150E78;
    coord->sub     = &gGfxViewCoord;
    obj->flags     = 0;
    func_800B3F84(&work->anim, D_actor_201200_80150DB8, obj, work->poses, work->slots);

    o1           = &work->obj230;
    o1->ctx.recs = &work->rec1B8;
    o1->pos.vy   = -0x34;
    o1->coord    = coord;
    o1->pos.vx   = 0;
    o1->pos.vz   = 0;
    o1->key      = 0x3000C;
    o1->radius   = 0xB4;
    o1->flags    = 1;
    Gp_LinkObj(2, o1);
    o1->flags |= 0x4000;
    Gp_InitRec18Table(o1->ctx.recs, 5, 0);

    o2           = &work->obj2C8;
    sv.vx        = 0;
    sv.vy        = -0x168;
    sv.vz        = 0;
    p            = &sv;
    hits         = &work->rec250;
    o2->coord    = arg1->field_2C->coords + 2;
    o2->ctx.recs = hits;
    o2->pos.vx   = p->vx;
    o2->pos.vy   = p->vy;
    o2->pos.vz   = p->vz;
    o2->key      = 0x3000C;
    o2->radius   = 0x168;
    o2->flags    = 1;
    Gp_LinkObj(2, o2);
    o2->flags |= 0x8000;
    Gp_InitRec18Table(o2->ctx.recs, 5, 0);

    o3           = &work->obj300;
    sv.vx        = 0;
    sv.vy        = 0;
    sv.vz        = 0;
    o3->coord    = &gGfxViewCoord;
    o3->ctx.recs = &work->rec2E8;
    o3->pos.vx   = p->vx;
    o3->pos.vy   = p->vy;
    o3->pos.vz   = p->vz;
    o3->radius   = 0x500;
    o3->flags    = 1;
    Gp_LinkObj(3, o3);
    Gp_InitRec18Table(o3->ctx.recs, 1, 0);

    o4           = &work->obj338;
    o4->coord    = &gGfxViewCoord;
    o4->ctx.recs = (GpRec18*)work->pad_320;
    o4->pos.vx   = p->vx;
    o4->pos.vy   = p->vy;
    o4->pos.vz   = p->vz;
    o4->radius   = 0x80;
    o4->flags    = 1;
    Gp_LinkObj(8, o4);
    Gp_InitRec18Table(o4->ctx.recs, 1, 0);

    arg0->field_4    = &coord->coord;
    arg0->field_48   = 0;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->coord      = arg1->field_2C->coords + 2;
    Gp_LinkNode(&arg0->node);
    arg0->node.flags = 1;
    arg0->hp = arg0->hpMax = 1;
    arg0->reactionFlags    = 0;
    arg0->hp = arg0->hpMax = D_actor_201200_8014DE54.hpMax;
    arg0->param            = &D_actor_201200_8014DE54;
    arg0->recs             = hits;
    work->field_170        = 2;
    work->field_174        = 1;
    work->field_176        = 0x10;
    work->field_178        = 0;
    func_actor_201200_8014A640(arg1);
    work->field_17E = 0;
    work->field_8   = 0;
    obj->lightMtx   = &work->lightMtx;
    obj->colorMtx   = &work->colorMtx;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    work->field_198 = 5;
    work->field_19A = 0x14;
    if ((u16)(arg0->placeKey >> 12) % 2 == 1) {
        work->field_176 += arg0->placeKey >> 12;
        work->field_19A += arg0->placeKey >> 12;
        work->field_198 += arg0->placeKey >> 12;
    } else {
        work->field_176 -= (u16)(arg0->placeKey >> 12) / 2;
        work->field_19A -= arg0->placeKey >> 13;
        work->field_198 -= arg0->placeKey >> 13;
    }
    work->origin.vx = arg1->field_2C->coords->coord.t[0];
    work->origin.vy = arg1->field_2C->coords->coord.t[1];
    work->origin.vz = arg1->field_2C->coords->coord.t[2];
    Gfx_MatrixCol2(&arg1->field_2C->coords->coord, &sv);
    sv.vy = 0;
    q     = &sv;
    VectorNormalSS(q, q);
    gte_lddp(1000);
    gte_ldsv(q);
    gte_gpf12_real();
    gte_stsv(q);
    work->patrol[0].vx = arg1->field_2C->coords->coord.t[0] + sv.vx;
    work->patrol[0].vy = arg1->field_2C->coords->coord.t[1];
    work->patrol[0].vz = arg1->field_2C->coords->coord.t[2] + sv.vz;
    work->patrol[1].vx = arg1->field_2C->coords->coord.t[0] - sv.vx;
    work->patrol[1].vy = arg1->field_2C->coords->coord.t[1];
    work->patrol[1].vz = arg1->field_2C->coords->coord.t[2] - sv.vz;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    if (arg1->field_36 == 0) {
        work->field_0 = 7;
    } else if (arg1->field_36 == 1) {
        work->field_0 = 2;
    } else {
        work->field_0 = 7;
    }
    work->field_2           = -1;
    part                    = arg1->field_2C->coords;
    work->eff1A8.spawnArgLo = 0x80;
    work->eff1A8.spawnArgHi = 2;
    work->eff1A8.coord      = part + 1;
    arg1->state++;
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
        obj->flags          = 0;
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
    coord    = arg1->field_2C->coords;
    d        = &delta;
    delta.vx = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy    = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz    = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor201200_OutOfRange(d, 2000)) {
        Gp_ArmStateF0(1);
        work->field_0 = 3;
    }
}

void func_actor_201200_8014B054(Actor201200Ctx* arg0, Actor201200* arg1)
{
    Actor201200Work*        work;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          facing;
    GsCOORDINATE2*          part;
    TmdObject*              obj;
    Actor201200TurnScratch* head;
    Actor201200TurnScratch* s;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                 = arg1->field_2C;
        arg0->field_14      = 0;
        obj->flags          = 0;
        work->field_174     = 3;
        work->field_170     = 1;
        work->field_178     = 0x10;
        work->obj2C8.flags |= 0x8000;
        work->obj300.flags &= 0x7FFF;
        work->obj338.flags &= 0x7FFF;
        work->obj230.flags |= 0x4000;
        func_actor_201200_8014A640(arg1);
        work->field_3DC = 0;
        Gp_ArmStateF0(1);
        return;
    }
    head                                      = *(Actor201200TurnScratch**)G_SCRATCH_HEAD;
    *(Actor201200TurnScratch**)G_SCRATCH_HEAD = head - 1;
    s                                         = head - 1;
    func_actor_201200_8014A640(arg1);
    coord         = arg1->field_2C->coords;
    head[-1].d.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    s->d.vy       = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    s->d.vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    facing        = arg1->field_2C->coords;
    s->angle      = Actor201200_NormalizeYaw(ratan2(head[-1].d.vx, s->d.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]));
    if (s->angle > 0x10) {
        s->angle = 0x10;
    }
    if (s->angle < -0x10) {
        s->angle = -0x10;
    }
    part      = arg1->field_2C->coords;
    s->angle += ratan2(-part->coord.m[2][0], part->coord.m[2][2]);
    Gfx_RotMatrixY(&arg1->field_2C->coords->coord, s->angle, 1);
    Actor201200_StepForward(arg1->field_2C->coords, 0x14);
    func_actor_201200_8014A49C(arg1->field_2C->coords, &work->rec1B8, 5);
    if (Actor201200_OutOfRange(&s->d, 1000)) {
        work->field_3DC++;
    } else {
        work->field_3DC = 0;
    }
    if (!Actor201200_OutOfRange(&s->d, 1000)) {
        work->field_8++;
    } else {
        work->field_8 = 0;
    }
    if (work->field_8 >= 0x15) {
        work->field_0 = 5;
    }
    if (func_actor_201200_80149F50(arg1->field_2C->coords, &work->rec250, 5, &s->d) == 1) {
        work->field_0 = 6;
    }
    arg1->field_2C->coords->flg = 0;
    s->d.vx                     = work->origin.vx - arg1->field_2C->coords->coord.t[0];
    s->d.vy                     = 0;
    s->d.vz                     = work->origin.vz - arg1->field_2C->coords->coord.t[2];
    Actor201200_OutOfRange(&s->d, 3000);
    if (work->field_3DC >= 0xF1) {
        work->field_0 = 8;
    }
    *(Actor201200TurnScratch**)G_SCRATCH_HEAD += 1;
}

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

void func_actor_201200_8014B5FC(Actor201200Ctx* arg0, Actor201200* arg1)
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
    memset(&ofs, 0, 8);
    if (work->field_4 != 0) {
        arg0->field_14      = 1;
        obj->flags          = 0;
        work->obj2C8.flags &= 0x7FFF;
        work->obj300.flags &= 0x7FFF;
        work->obj338.flags &= 0x7FFF;
        work->obj300.key    = Gp_PackObjPair((GpObj50*)arg0, 0);
        work->obj338.key    = 0x22121;
        work->field_6       = 0;
        work->obj230.flags |= 0x4000;
        work->savedColorMtx = work->colorMtx;
        work->field_174     = 0xA;
        work->field_170     = 1;
        work->field_178     = 8;
        func_actor_201200_8014A640(arg1);
        work->obj338.pos.vx = arg1->field_2C->coords->coord.t[0];
        work->obj338.pos.vy = arg1->field_2C->coords->coord.t[1] - 0x190;
        work->obj338.pos.vz = arg1->field_2C->coords->coord.t[2];
        work->obj300.pos.vx = arg1->field_2C->coords->coord.t[0];
        work->obj300.pos.vy = arg1->field_2C->coords->coord.t[1];
        work->obj300.pos.vz = arg1->field_2C->coords->coord.t[2];
        return;
    }
    func_actor_201200_8014A640(arg1);
    switch ((s16)(work->field_6 - 0x29)) {
        case 0:
            arg1->field_2C->flags |= 2;
            ofs.vx                 = 0x1E;
            ofs.vz                 = 0x1E;
            ofs.vy                 = -0xA;
            Gp_SpawnEff(0x60030, arg1->field_2C->coords, 0x10100, &ofs);
            ofs.vy = -0x14;
            ofs.vz = -0x50;
            Gp_SpawnEff(0x60030, arg1->field_2C->coords, 0x10100, &ofs);
            break;
        case 1:
            work->eff1A8.coord      = &arg1->field_2C->coords[4];
            work->eff1A8.spawnArgLo = 0x120;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->field_2C->coords[4], NULL, &work->eff1A8);
            Gp_SpawnScript18Ex((s32)&D_actor_201200_8014DE64, (s32)&D_actor_201200_8014DE70, (s16)gpGetObjDepth(arg1->field_2C->coords));
            work->obj300.radius = 0x320;
            work->obj338.radius = 0xC8;
            work->obj300.flags |= 0x8000;
            work->obj338.flags |= 0x8000;
            Gp_SpawnEff(0x6009C, &arg1->field_2C->coords[2], 1, NULL);
            break;
        case 2:
            work->obj338.radius = 0x190;
            work->obj300.flags &= 0x7FFF;
            break;
        case 3:
            work->eff1A8.coord      = &arg1->field_2C->coords[1];
            work->eff1A8.spawnArgLo = 0x80;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->field_2C->coords[1], NULL, &work->eff1A8);
            work->obj338.radius = 0x320;
            break;
        case 5:
            work->obj338.flags &= 0x7FFF;
            break;
        case 7:
            work->eff1A8.coord      = &arg1->field_2C->coords[1];
            work->eff1A8.spawnArgLo = 0x200;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->field_2C->coords[1], NULL, &work->eff1A8);
            Gp_SpawnEff(0x6009E, arg1->field_2C->coords, 0, &ofs);
            id  = ((arg0->field_8 >> 12) << 8) | 0x400C0004;
            pan = (s8)Gp_GetObjPan(arg1->field_2C->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(arg1->field_2C->coords));
            break;
        case 9:
            obj->flags = 0x80;
            break;
        case 28:
            Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0xC);
            work->field_0 = 0;
            break;
        default:
            work->colorMtx = work->savedColorMtx;
            break;
    }
    work->colorMtx = work->savedColorMtx;
    if ((u16)(work->field_6 - 0x17) < 0x12) {
        work->colorMtx.t[0] += ((s16)work->field_6 - 0x16) * 0x60;
    }
    if ((u16)(work->field_6 - 0x2A) < 9) {
        s = 0xBB8 - ((s16)work->field_6 - 0x2A) * 0x258;
        if (s < 0x4B0) {
            scale.vx = scale.vy = scale.vz = 0;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(0);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12_real();
            gte_stlvl(work->colorMtx.t);
            Actor201200_FaceScale(arg1->field_2C->coords, 0x1000);
        } else {
            scale.vx = scale.vy = scale.vz = s;
            work->colorMtx                 = work->savedColorMtx;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(s);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12_real();
            gte_stlvl(work->colorMtx.t);
            s = ((s16)work->field_6 - 0x28) * 0x400 + 0x1000;
            if (s > 0x2000) {
                s = 0x2000;
            }
            Actor201200_FaceScale(arg1->field_2C->coords, s);
        }
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
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
        arg0->field_14      = 1;
        obj->flags          = 0;
        work->obj2C8.flags &= 0x7FFF;
        work->obj300.flags &= 0x7FFF;
        work->obj338.flags &= 0x7FFF;
        work->obj300.key    = Gp_PackObjPair((GpObj50*)arg0, 0);
        work->obj338.key    = 0x22121;
        work->field_6       = 0;
        work->obj230.flags &= 0xBFFF;
        work->savedColorMtx = work->colorMtx;
        work->field_174     = 0xA;
        work->field_170     = 1;
        work->field_178     = 0;
        work->field_176     = 0x2C;
        func_actor_201200_8014A640(arg1);
        work->obj338.pos.vx = arg1->field_2C->coords->coord.t[0];
        work->obj338.pos.vy = arg1->field_2C->coords->coord.t[1] - 0x190;
        work->obj338.pos.vz = arg1->field_2C->coords->coord.t[2];
        work->obj300.pos.vx = arg1->field_2C->coords->coord.t[0];
        work->obj300.pos.vy = arg1->field_2C->coords->coord.t[1];
        work->obj300.pos.vz = arg1->field_2C->coords->coord.t[2];
        return;
    }
    func_actor_201200_8014A640(arg1);
    switch ((s16)(work->field_6 - 0xD)) {
        case 0:
            ofs.vx = 0x1E;
            ofs.vz = 0x1E;
            ofs.vy = -0x3C;
            Gp_SpawnEff(0x60030, arg1->field_2C->coords, 0x10080, &ofs);
            ofs.vy = -0xA;
            ofs.vz = -0x50;
            Gp_SpawnEff(0x60030, arg1->field_2C->coords, 0x10030, &ofs);
            id  = ((arg0->field_8 >> 12) << 8) | 0x400C0004;
            pan = (s8)Gp_GetObjPan(arg1->field_2C->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(arg1->field_2C->coords));
            arg1->field_2C->flags = 2;
            break;
        case 1:
            work->obj300.radius = 0x320;
            work->obj300.flags |= 0x8000;
            Gp_SpawnEff(0x6009C, &arg1->field_2C->coords[2], 1, NULL);
            Gp_SpawnScript18((s32)&D_actor_201200_8014DE64, (s32)&D_actor_201200_8014DE70);
            work->eff1A8.coord      = &arg1->field_2C->coords[4];
            work->eff1A8.spawnArgLo = 0x120;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->field_2C->coords[4], NULL, &work->eff1A8);
            break;
        case 2:
            work->obj338.radius = 0xC8;
            work->obj300.flags &= 0x7FFF;
            work->obj338.flags |= 0x8000;
            break;
        case 3:
            work->obj338.radius     = 0x190;
            work->eff1A8.coord      = &arg1->field_2C->coords[2];
            work->eff1A8.spawnArgLo = 0x100;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->field_2C->coords[2], NULL, &work->eff1A8);
            break;
        case 4:
            work->obj338.radius = 0x320;
            break;
        case 6:
            work->obj338.flags &= 0x7FFF;
            ofs.vx              = arg1->field_2C->coords->coord.t[0];
            ofs.vy              = arg1->field_2C->coords->coord.t[1];
            ofs.vz              = arg1->field_2C->coords->coord.t[2];
            Gp_SpawnEff(0x6009E, &gGfxViewCoord, 0, &ofs);
            work->eff1A8.coord      = &arg1->field_2C->coords[1];
            work->eff1A8.spawnArgLo = 0x200;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->field_2C->coords[1], NULL, &work->eff1A8);
            break;
        case 8:
            obj->flags = 0x80;
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
        Actor201200_FaceScale(arg1->field_2C->coords, s);
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}
