#include "common.h"

#include "actors/actors_shared_80131fc8.h"

#include "psyq/inline_c.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/sound.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_405800.h"
#include "actors/actor_405800_anim.h"
#include "actors/actors_shared_80139dcc.h"
#include "actors/actors_shared_8013a0b0.h"
#include "actors/actors_shared_8016a538.h"

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s32                   Gp_LcgState;
extern u8                    D_801153F4;
extern u8                    D_actor_405800_801513F8[];
extern TaskDesc              D_actor_405800_801514B4;
extern GpPairSrcE            D_actor_405800_801418FC;
extern u8                    D_actor_405800_80151410[];
extern u8                    D_actor_405800_8015149C[];
extern const TaskFuncTable18 D_actor_405800_80131E64;

void func_actor_405800_801329C8(Task* arg0, s16 arg1, s16 arg2, s16 arg3, s16 arg4, u8 arg5);
/* Unprototyped so the first jal keeps a nop delay slot; a0 still holds the task. */
s32  func_actor_405800_80136A1C();
void func_actor_405800_80135A3C(Task* arg0, s16 arg1);
s32  func_actor_405800_8013728C(Task* arg0);
s32  func_actor_405800_801373E0(Task* arg0);
s32  func_actor_405800_80137908(Task* arg0);
void func_actor_405800_801379F8(Task* task);
void func_actor_405800_80137948(Task* task);
void func_actor_405800_80137994(Task* arg0, s16 arg1);
void func_actor_405800_80135780(Task* arg0);
void func_actor_405800_8013340C(Task* arg0);
void func_actor_405800_80138514(Task* arg0, s16 arg1, Actor405800ViewPos* arg2);
void func_actor_405800_801361F8(Task* arg0);
void func_actor_405800_80136388(Task* arg0);
void func_actor_405800_801375C4(Task* arg0);
void func_actor_405800_8013795C(Task* arg0);

void func_actor_405800_80132670(Task* arg0)
{
    Actor405800Work* work;

    work                   = (Actor405800Work*)arg0->work;
    work->obj_4B4.coord    = &((TmdObject*)arg0->extra)->coords[3];
    work->obj_4B4.ctx.recs = work->rec_4D4;
    work->obj_4B4.pos.vx   = 0;
    work->obj_4B4.pos.vy   = 0;
    work->obj_4B4.pos.vz   = 0x110;
    work->obj_4B4.key      = 0x3003A;
    work->obj_4B4.radius   = 0x2F0;
    work->obj_4B4.flags    = 1;
    Gp_LinkObj(2, &work->obj_4B4);
    Gp_InitRec18Table(work->rec_4D4, 8, 0);
    work->obj_4B4.flags   |= 0x8000;
    work->obj_594.coord    = ((TmdObject*)arg0->extra)->coords;
    work->obj_594.ctx.recs = work->rec_5B4;
    work->obj_594.pos.vx   = 0;
    work->obj_594.pos.vy   = -0x220;
    work->obj_594.pos.vz   = 0;
    work->obj_594.key      = 0x3003A;
    work->obj_594.radius   = 0x460;
    work->obj_594.flags    = 1;
    Gp_LinkObj(2, &work->obj_594);
    Gp_InitRec18Table(work->rec_5B4, 8, 0);
    work->rec_744.field_4   = 0xBB8;
    work->rec_744.field_10  = 0xA;
    work->rec_744.field_12  = 0xA;
    work->rec_744.field_0   = 0;
    work->rec_744.field_C   = 0;
    work->rec_744.field_8   = 0;
    work->rec_744.field_14  = work->rec_75C;
    work->obj_594.flags    |= 0x4000;
    work->obj_724.coord     = ((TmdObject*)arg0->extra)->coords;
    work->obj_724.ctx.d4rec = &work->rec_744;
    work->obj_724.pos.vx    = 0;
    work->obj_724.pos.vy    = -0x190;
    work->obj_724.pos.vz    = 0;
    work->obj_724.key       = 0x30005;
    work->obj_724.radius    = 0;
    work->obj_724.flags     = 3;
    Gp_LinkObj(2, &work->obj_724);
    Gp_InitRec18Table(work->rec_75C, 8, 0);
    work->obj_724.flags   &= 0x3FFF;
    work->obj_6B4.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj_6B4.coord    = &((TmdObject*)arg0->extra)->coords[7];
    work->obj_6B4.ctx.recs = work->rec_6F4;
    work->obj_6B4.pos.vx   = -0x460;
    work->obj_6B4.pos.vy   = 0;
    work->obj_6B4.pos.vz   = 0;
    work->obj_6B4.radius   = 0x290;
    work->obj_6B4.flags    = 1;
    Gp_LinkObj(3, &work->obj_6B4);
    Gp_InitRec18Table(work->rec_6F4, 1, 0);
    work->obj_6B4.flags   &= 0x7FFF;
    work->obj_674.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj_674.coord    = &((TmdObject*)arg0->extra)->coords[7];
    work->obj_674.ctx.recs = work->rec_6F4;
    work->obj_674.pos.vx   = -0x200;
    work->obj_674.pos.vy   = 0;
    work->obj_674.pos.vz   = 0;
    work->obj_674.radius   = 0x250;
    work->obj_674.flags    = 1;
    Gp_LinkObj(3, &work->obj_674);
    Gp_InitRec18Table(work->rec_6F4, 1, 0);
    work->obj_674.flags   &= 0x7FFF;
    work->obj_6D4.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj_6D4.coord    = &((TmdObject*)arg0->extra)->coords[10];
    work->obj_6D4.ctx.recs = work->rec_70C;
    work->obj_6D4.pos.vx   = 0x460;
    work->obj_6D4.pos.vy   = 0;
    work->obj_6D4.pos.vz   = 0;
    work->obj_6D4.radius   = 0x290;
    work->obj_6D4.flags    = 1;
    Gp_LinkObj(3, &work->obj_6D4);
    Gp_InitRec18Table(work->rec_70C, 1, 0);
    work->obj_6D4.flags   &= 0x7FFF;
    work->obj_694.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj_694.coord    = &((TmdObject*)arg0->extra)->coords[10];
    work->obj_694.ctx.recs = work->rec_70C;
    work->obj_694.pos.vx   = 0x200;
    work->obj_694.pos.vy   = 0;
    work->obj_694.pos.vz   = 0;
    work->obj_694.radius   = 0x250;
    work->obj_694.flags    = 1;
    Gp_LinkObj(3, &work->obj_694);
    Gp_InitRec18Table(work->rec_70C, 1, 0);
    work->obj_694.flags &= 0x7FFF;
}

void func_actor_405800_801329C8(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade)
{
    Actor405800BeamScratch* s;
    s16                     angle;
    GsCOORDINATE2*          secondCoord;
    GsCOORDINATE2*          firstCoord;
    s32                     offset0;
    s32                     offset1;
    s32                     offset2;
    s32                     offset3;
    s32                     halfX;
    s32                     halfZ;
    GsCOORDINATE2*          coords;
    POLY_FT4*               poly;

    coords      = ((TmdObject*)task->extra)->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = (Actor405800BeamScratch*)(*(u8**)G_SCRATCH_HEAD -= sizeof(Actor405800BeamScratch));
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &firstCoord->workm, &s->firstMatrix);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &secondCoord->workm, &s->secondMatrix);
        s->first.vy       = height;
        s->second.vy      = height;
        s->first.vx       = s->firstMatrix.t[0];
        s->first.vz       = s->firstMatrix.t[2];
        s->second.vx      = s->secondMatrix.t[0];
        s->second.vz      = s->secondMatrix.t[2];
        angle             = ratan2(s->second.vx - s->first.vx, s->second.vz - s->first.vz);
        halfX             = (s->first.vx - s->second.vx) / 2;
        halfZ             = (s->first.vz - s->second.vz) / 2;
        offset0           = rcos(angle) * width;
        s->corner0.vy     = height;
        s->corner0.vx     = halfX + (s->first.vx - (offset0 >> 0xC));
        s->corner0.vz     = halfZ + (s->first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1           = rcos(angle) * width;
        s->corner1.vy     = height;
        s->corner1.vx     = halfX + (s->first.vx + (offset1 >> 0xC));
        s->corner1.vz     = halfZ + (s->first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2           = rcos(angle) * width;
        s->corner2.vy     = height;
        s->corner2.vx     = (s->second.vx - (offset2 >> 0xC)) - halfX;
        s->corner2.vz     = (s->second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        offset3           = rcos(angle) * width;
        s->corner3.vy     = height;
        s->corner3.vx     = (s->second.vx + (offset3 >> 0xC)) - halfX;
        s->corner3.vz     = (s->second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        gGfxViewCoord.flg = 0;
        Gp_UpdateCoord(&gGfxViewCoord);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                                 &s->screen2, &s->screen3, &s->perspective, &s->flags);
        if (s->flags >= 0) {
            poly           = Gpu_PrimCursor;
            Gpu_PrimCursor = (u8*)poly + 0x28;
            setlen(poly, 9);
            poly->code       = 0x2E;
            *(s32*)&poly->x0 = s->screen0;
            *(s32*)&poly->x1 = s->screen1;
            *(s32*)&poly->x2 = s->screen2;
            *(s32*)&poly->x3 = s->screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt), poly);
        }
        *(u8**)G_SCRATCH_HEAD += sizeof(Actor405800BeamScratch);
    }
}

void func_actor_405800_80132E3C(Task* arg0, s16 arg1, u8 arg2)
{
    func_actor_405800_801329C8(arg0, 3, 9, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 9, 0xA, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 0xA, 0xB, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 3, 6, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 6, 7, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 7, 8, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 1, 5, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 1, 0xC, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 0xC, 0xD, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 0xD, 0xE, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 1, 0xF, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 0xF, 0x10, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 0x10, 0x11, 0x100, arg1, arg2);
}

void func_actor_405800_80132FE0(Task* arg0)
{
    Actor405800Work* work;
    TmdObject*       model;
    GsCOORDINATE2*   coord;
    VECTOR           scale;
    SVECTOR          rot;

    work              = (Actor405800Work*)arg0->work;
    model             = (TmdObject*)arg0->extra;
    coord             = model->coords;
    work->field_832   = (u16)work->field_832 + ((s16)(0xFF - (u16)work->field_832) >> 4);
    work->field_834   = (u16)work->field_834 + ((s16)(-(u16)work->field_834) >> 4);
    work->field_866   = (u16)work->field_866 + (-work->field_866 >> 2);
    model->lightLevel = work->field_834;
    func_8009EA50(work->field_832);
    work->flags_83C.h.field_83E -= 0x30;
    scale.vx                     = 0x1000;
    scale.vy                     = work->flags_83C.h.field_83E;
    scale.vz                     = 0x1000;
    coord->coord                 = work->matrix_0;
    ScaleMatrix(&coord->coord, &scale);
    coord->flg = 0;
    work->field_842++;
    if ((s16)work->field_842 == 8) {
        rot.vx = 0;
        rot.vy = 0;
        rot.vz = 0;
        Gp_SpawnEff(0x600A5, coord, 3, &rot);
    }
    if ((s16)work->field_842 >= 0x41) {
        model->flags |= 0x80;
        work->field_846++;
    }
}

void func_actor_405800_8013315C(Task* arg0)
{
    GpEnemy*         enemy;
    Actor405800Work* work;
    TmdObject*       model;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor405800Work*)arg0->work;
    model = (TmdObject*)arg0->extra;
    if ((s8)work->field_895 < 0) {
        if (!(work->field_895 & 1)) {
            switch ((s8)work->field_896) {
                case 0:
                    work->field_832 = (u16)work->field_832 + ((s16)(0xFF - (u16)work->field_832) >> 2);
                    if (work->field_832 >= 0xF8) {
                        work->field_832 = 0xFF;
                        work->field_830 = 0;
                        work->field_896++;
                    }
                    goto block_32;
                case 1:
                    work->field_830++;
                    if (work->field_836 < (s16)work->field_830) {
                        goto block_28;
                    }
                    break;
                case 2:
                    work->field_834 = (u16)work->field_834 + ((s16)(-(u16)work->field_834) >> 2);
                    work->field_866 = (u16)work->field_866 + (-work->field_866 >> 2);
                    if (work->field_834 == 0) {
                        enemy->node.flags = 1;
                        if (work->field_897 == 0) {
                            enemy->node.flags = 5;
                        }
                        work->field_866 = 0;
                        work->field_895 = 0;
                    }
                    goto block_26;
            }
        } else {
            switch ((s8)work->field_896) {
                case 0:
                    enemy->node.flags = 0;
                    if (work->field_897 == 0) {
                        enemy->node.flags = 4;
                    }
                    work->field_834 = (u16)work->field_834 + ((s16)(0x1000 - (u16)work->field_834) >> 2);
                    work->field_866 = (u16)work->field_866 + ((0xFF - work->field_866) >> 2);
                    if (work->field_834 >= 0xFF0) {
                        work->field_866 = 0xFF;
                        work->field_834 = 0x1000;
                        work->field_830 = 0;
                        work->field_896++;
                    }
                block_26:
                    model->lightLevel = work->field_834;
                    break;
                case 1:
                    work->field_830++;
                    if ((s16)work->field_830 >= 0x11) {
                    block_28:
                        work->field_896++;
                    }
                    break;
                case 2:
                    work->field_832 = (u16)work->field_832 + ((s16)(-(u16)work->field_832) >> 2);
                    if (work->field_832 < 9) {
                        work->field_832 = 0;
                        work->field_895 = 0;
                        func_actor_405800_8013340C(arg0);
                        if (work->field_83A == 0) {
                            work->field_83A = (u16)work->field_838;
                        }
                    }
                block_32:
                    func_8009EA50(work->field_832);
                    break;
            }
        }
    }
    if (work->field_83A > 0) {
        work->field_83A = (u16)work->field_83A - 1;
    }
}

void func_actor_405800_8013340C(Task* arg0)
{
    GpEnemy*         enemy;
    Actor405800Work* work;
    s16              hp;
    s32              maxHp;
    s32              quarter;

    enemy   = (GpEnemy*)arg0->spawnArg2;
    hp      = enemy->field_40;
    work    = (Actor405800Work*)arg0->work;
    maxHp   = enemy->field_42 << 0x10;
    quarter = maxHp >> 0x12;
    if ((quarter + (maxHp >> 0x11)) < hp) {
        work->field_836 = 0x10;
        work->field_838 = 0;
        return;
    }
    if (quarter < hp) {
        work->field_836 = 0x20;
        work->field_838 = 0x40;
        return;
    }
    if ((maxHp >> 0x13) < hp) {
        work->field_836 = 0x30;
        work->field_838 = 0x80;
        return;
    }
    if ((maxHp >> 0x14) < hp) {
        work->field_836 = 0x40;
        work->field_838 = 0xC0;
        return;
    }
    work->field_836 = 0x50;
    work->field_838 = 0x100;
}

void func_actor_405800_801334B8(Task* arg0)
{
    TmdObject*       model;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    Actor405800Work* work;
    Actor405800Work* w2;
    Actor405800Work* w3;
    Actor405800Work* w4;
    TmdObject*       extra;
    u32              rnd;

    model = (TmdObject*)arg0->extra;
    enemy = (GpEnemy*)arg0->spawnArg2;
    coord = model->coords;
    if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) != 0x04080000) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    arg0->work = memCalloc(0x89CU, false);
    work       = (Actor405800Work*)arg0->work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    D_80062735         = 2;
    model->lightMtx    = &work->matrix_40;
    model->colorMtx    = &work->matrix_20;
    model->flags       = 0;
    arg0->msgTable     = D_actor_405800_8015149C;
    enemy->field_4     = &coord->coord;
    enemy->field_48    = 0;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_18    = &((TmdObject*)arg0->extra)->coords[3];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags     = 5;
    enemy->field_50       = &D_actor_405800_801418FC;
    enemy->field_54       = (s32)work->rec_4D4;
    work->eff_81C.field_0 = &((TmdObject*)arg0->extra)->coords[3];
    work->eff_81C.field_4 = 0x100;
    work->eff_81C.field_6 = 2;
    enemy->field_40 = enemy->field_42 = D_actor_405800_801418FC.field_4;
    func_800B3F84(&work->anim, D_actor_405800_80151410, model, work->pad_394, work->slots);

    w2            = (Actor405800Work*)arg0->work;
    w2->field_850 = 0x10;
    w2->field_872 = 1;
    w2->field_86E = 2;

    Actor405800_TickAnim(arg0);

    coord->sub = &gGfxViewCoord;
    func_actor_405800_80132670(arg0);
    func_actor_405800_80135780(arg0);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    w3            = (Actor405800Work*)arg0->work;
    w3->field_846 = 0;
    w3->field_848 = 0;
    if (gGameSession->at4.loc.warp == 1) {
        coord->coord.t[0] = 0x14B4;
        coord->coord.t[2] = 0xD7A;
        coord->coord.t[1] = 0;
        work->field_82    = 0x400;
    } else {
        coord->coord.t[0] = 0x514;
        coord->coord.t[1] = 0;
        coord->coord.t[2] = 0x251C;
        work->field_82    = 0;
    }
    work->field_90  = coord->coord.t[0];
    work->field_92  = coord->coord.t[1];
    work->field_94  = coord->coord.t[2];
    rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
    Gp_LcgState     = rnd;
    work->field_840 = rnd >> 0x10;
    work->field_86A = work->field_92;
    w4              = (Actor405800Work*)arg0->work;
    extra           = (TmdObject*)arg0->extra;
    w4->field_832   = 0xFF;
    w4->field_834   = 0;
    w4->field_866   = 0;
    w4->field_836   = 0x10;
    func_8009EA50(w4->field_832);
    extra->lightLevel = w4->field_834;
    w3                = (Actor405800Work*)arg0->work;
    arg0->state       = 1;
    w3->field_846     = 0;
    w3->field_848     = 0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800", D_actor_405800_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800", D_actor_405800_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800", ActorsShared801328ccTable);

INCLUDE_RODATA("actors/nonmatchings/actor_405800/actor_405800", D_actor_405800_80131E64);

static __inline__ void Actor405800_ProjectPart(GsCOORDINATE2* part)
{
    void**                   scratch;
    u8*                      head;
    Actor405800PerspScratch* block;
    SVECTOR*                 vec;
    MATRIX*                  wm;

    scratch       = (void**)G_SCRATCH_HEAD;
    head          = *scratch;
    block         = (Actor405800PerspScratch*)(head - 0x18);
    *scratch      = block;
    block->vec.vx = 0;
    block->vec.vy = 0;
    block->vec.vz = 0;
    Gp_UpdateCoord(part);
    vec = &block->vec;
    wm  = &part->workm;
    gte_SetRotMatrix(wm);
    gte_SetTransMatrix(wm);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((Actor405800PerspScratch*)(head - 0x18))->sxy);
    gte_stdp(&((Actor405800PerspScratch*)(head - 0x18))->p);
    gte_stflg(&((Actor405800PerspScratch*)(head - 0x18))->flag);
    gte_stszotz(&((Actor405800PerspScratch*)(head - 0x18))->otz);
    if (block->flag < 0) {
        block->otz = 0;
    }
    block->otz = (block->otz >> 4) + 0x1E;
    ActorsShared80131fc8(block->otz);
}

void func_actor_405800_80133800(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor405800Work* work  = (Actor405800Work*)arg0->work;
    GsCOORDINATE2*   coord = model->coords;
    GpEnemy*         enemy = (GpEnemy*)arg0->spawnArg2;
    GsCOORDINATE2*   part  = &coord[2];
    GsCOORDINATE2*   root  = coord;
    TaskFuncTable18  fns   = D_actor_405800_80131E64;
    Actor405800Work* w;
    u8*              head;

    switch (D_801153F4) {
        case 2:
            model->flags |= 0x80;
            break;
        case 0:
            work->field_840++;
            func_actor_405800_801361F8(arg0);
            fns.funcs[(s16)work->field_846](arg0);
            func_actor_405800_8013795C(arg0);
            func_actor_405800_801375C4(arg0);
            func_actor_405800_8013315C(arg0);
            Actor405800_TickAnim(arg0);
            work->flags_83C.half = work->slots[1].flags;
            root->flg            = 0;
            Actor405800_RebuildRotation(arg0);
            func_actor_405800_80136388(arg0);
            if (enemy->field_40 <= 0 && (u8)work->field_88F == 0) {
                w            = (Actor405800Work*)arg0->work;
                arg0->state  = 2;
                w->field_846 = 0;
                w->field_848 = 0;
            }
        case 1:
            Actor405800_UpdateColor(arg0);
            func_actor_405800_80132E3C(arg0, work->field_86A, work->field_866);
            Actor405800_ProjectPart(part);
            __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
            head                  = *(u8**)(head + 0x3FC);
            head                 += 0x18;
            *(u8**)G_SCRATCH_HEAD = head;
            model->flags         &= 0xFF7F;
            break;
    }
}

void func_actor_405800_80133CD0(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    u32              sound;
    s32              pan;

    if (((Actor405800Work*)arg0->work)->field_852 < 0x1450) {
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x40050004;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
        work = (Actor405800Work*)arg0->work;
        if (((s8)work->field_895 >= 0) || ((work->field_895 & 0x7F) != 1)) {
            work->field_895 = 0x81;
            work->field_896 = 0;
        }
        Gp_ArmStateF0(1);
        work2            = (Actor405800Work*)arg0->work;
        work2->field_846 = 0xC;
        work2->field_848 = 0;
    }
}

void func_actor_405800_80133DB0(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    s32              id;
    u32              sound;
    s32              pan;

    work = (Actor405800Work*)arg0->work;
    work->field_842++;
    if ((s16)work->field_842 == 0x16) {
        id = 0x40050005;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan   = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
        work->obj_6D4.flags |= 0x8000;
        work->obj_694.flags |= 0x8000;
    }
    if ((s16)work->field_842 == 0x1C) {
        work->obj_6D4.flags &= 0x7FFF;
        work->obj_694.flags &= 0x7FFF;
    }
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        func_actor_405800_80137994(arg0, 0);
        func_actor_405800_80137948(arg0);
        if (work->field_890 == 0 && work->field_852 < 0x578 && (u16)(work->field_856 - 0x200) > 0xC00 && (u16)(work->field_854 - 0x200) > 0xC00) {
            work2            = (Actor405800Work*)arg0->work;
            work2->field_846 = 9;
            work2->field_848 = 0;
        } else {
            work3            = (Actor405800Work*)arg0->work;
            work3->field_846 = 2;
            work3->field_848 = 0;
        }
    }
}

void func_actor_405800_80133F48(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    s32              id;
    u32              sound;
    s32              pan;

    work = (Actor405800Work*)arg0->work;
    work->field_842++;
    if ((s16)work->field_842 == 0x16) {
        id = 0x40050005;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan   = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
        work->obj_6B4.flags |= 0x8000;
        work->obj_674.flags |= 0x8000;
    }
    if ((s16)work->field_842 == 0x1C) {
        work->obj_6B4.flags &= 0x7FFF;
        work->obj_674.flags &= 0x7FFF;
    }
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        func_actor_405800_80137994(arg0, 0);
        func_actor_405800_80137948(arg0);
        if (work->field_890 == 0 && work->field_852 < 0x578 && (u16)(work->field_856 - 0x200) > 0xC00 && (u16)(work->field_854 - 0x200) > 0xC00) {
            work2            = (Actor405800Work*)arg0->work;
            work2->field_846 = 9;
            work2->field_848 = 0;
        } else {
            work3            = (Actor405800Work*)arg0->work;
            work3->field_846 = 2;
            work3->field_848 = 0;
        }
    }
}

void func_actor_405800_801340E0(Task* arg0)
{
    Actor405800Msg3FF msg;
    Actor405800Msg3F8 query;
    Actor405800Work*  work;
    Actor405800Work*  work2;
    Actor405800Work*  work3;
    s32               base;
    s32               sound;
    s32               pan;

    work = (Actor405800Work*)arg0->work;
    if (Gp_ActorSlots[0]->actor->field_954 == 2 || (func_actor_405800_8013728C(arg0) << 0x10) != 0) {
        func_actor_405800_801379F8(arg0);
        work3            = (Actor405800Work*)arg0->work;
        work3->field_846 = 2;
        work3->field_848 = 0;
        func_actor_405800_80135A3C(arg0, work->field_87E);
        return;
    }
    query.field_14 = 0x18;
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)&query, 0) != 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
        if (work->field_890 == 0) {
            work3            = (Actor405800Work*)arg0->work;
            work3->field_846 = 2;
            work3->field_848 = 0;
            return;
        }
        work2            = (Actor405800Work*)arg0->work;
        work2->field_846 = 0xD;
        work2->field_848 = 0;
        return;
    }
    work->field_86A = work->field_92;
    func_actor_405800_801379F8(arg0);
    work->field_890      = 0;
    Gp_StateC08.field_6 |= 1;
    work->field_88F      = 1;
    work->field_9A       = work->field_92;
    msg.field_0          = D_actor_405800_801513F8;
    msg.field_8          = 0;
    msg.field_C          = 0;
    msg.field_10         = 0;
    msg.field_4          = 4;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&msg, 0);
    work->obj_4B4.flags &= 0x7FFF;
    work->obj_594.flags &= 0xBFFF;
    work2                = (Actor405800Work*)arg0->work;
    work2->field_850     = 0x10;
    work2->field_872     = 0x21;
    work2->field_84A     = 4;
    work2->field_86E     = 1;
    work->field_842      = 0;
    work->field_844      = 0;
    base                 = 0x40050004;
    if ((arg0->spawnArg1 & 0xF0) == 0x10) {
        base = 0x404A0004;
    }
    sound = base | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
    pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    work->field_884 = 0;
    work->field_848++;
}

void func_actor_405800_80134314(Task* arg0)
{
    Actor405800Msg3FF msg;
    SVECTOR           vec;
    Actor405800Work*  work;
    Actor405800Work*  work2;
    GpEnemy*          enemy;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    player;
    GsCOORDINATE2*    root;
    PlayerStatus*     cfg;
    s32               id;
    s32               sound;
    s32               pan;
    s32               sound2;
    s32               pan2;

    work               = (Actor405800Work*)arg0->work;
    coord              = ((TmdObject*)arg0->extra)->coords;
    enemy              = (GpEnemy*)arg0->spawnArg2;
    player             = Gp_ActorSlots[0]->extra->coords;
    work->field_84    += -(s16)work->field_84 >> 2;
    coord->coord.t[0] += (player->coord.t[0] - coord->coord.t[0]) >> 2;
    coord->coord.t[2] += (player->coord.t[2] - coord->coord.t[2]) >> 2;
    coord->coord.t[1] += (player->coord.t[1] - coord->coord.t[1]) >> 2;
    work->field_842++;
    cfg = &Player_Status;
    if (++work->field_844 == 8) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 6;
        pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if ((u8)work->field_88B == 1 || work->field_88C == 1 || enemy->field_40 <= 0 || work->field_884 >= 4) {
        work->field_88B = 0;
        if (work->field_88C == 0) {
            msg.field_0  = D_actor_405800_801513F8;
            msg.field_8  = 1;
            msg.field_C  = 8;
            msg.field_10 = 0;
            msg.field_4  = 5;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F4, (s32)&msg, 0);
        }
        work2            = (Actor405800Work*)arg0->work;
        work2->field_84A = 8;
        work2->field_850 = 0x10;
        work2->field_872 = 0x22;
        work2->field_86E = 1;
        work->field_84C  = -0x2A;
        work->field_84E  = 0;
        work->field_842  = 0;
        work->field_848++;
        return;
    }
    if ((s16)work->field_842 == 1 || (s16)work->field_842 == 0x10 || (s16)work->field_842 == 0x25) {
        root = &Gp_ActorSlots[0]->extra->coords[4];
        Gp_SpawnPadLerp(0xA, 0xC0, 8);
        id = 0x40050009;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0009;
        }
        sound2 = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan2   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound2, pan2, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 2), 0);
        vec.vx         = 0;
        vec.vy         = -200;
        vec.vz         = 0;
        work->field_98 = ((rsin((s16)work->field_82 + 0x800) * 3000) >> 12) / 20;
        work->field_9C = ((rcos((s16)work->field_82 + 0x800) * 3000) >> 12) / 20;
        Gp_SpawnEff(0x6009B, root, 0x10100, &vec);
        if (cfg->hp <= 0) {
            work->field_88C = 1;
        }
    }
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        work->field_842 = 0;
        work->field_884++;
    }
}

void func_actor_405800_8013471C(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   player;
    s32              y;
    s32              id;
    s32              sound;
    s32              pan;

    work            = (Actor405800Work*)arg0->work;
    coord           = ((TmdObject*)arg0->extra)->coords;
    player          = Gp_ActorSlots[0]->extra->coords;
    work->field_84 += -(s16)work->field_84 >> 2;
    work->field_842++;
    if ((s16)work->field_842 >= 8) {
        work->obj_594.flags |= 0x4000;
        coord->coord.t[0]   += (s16)work->field_98;
        coord->coord.t[2]   += (s16)work->field_9C;
        work->obj_4B4.flags |= 0x8000;
        work->field_84C     += 6;
        work->field_84E     += work->field_84C;
        y                    = coord->coord.t[1] + work->field_84E;
        coord->coord.t[1]    = y;
        if (y >= work->field_9A) {
            coord->coord.t[1] = work->field_9A;
            player->flg       = 0;
            Gp_UpdateCoord(player);
            id = 0x40050003;
            if ((arg0->spawnArg1 & 0xF0) == 0x10) {
                id = 0x404A0003;
            }
            sound = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
            pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
            work->obj_594.flags |= 0x4000;
            work2                = (Actor405800Work*)arg0->work;
            work2->field_84A     = 2;
            work2->field_872     = 0x19;
            work2->field_850     = 0x10;
            work2->field_86E     = 1;
            work->field_848++;
        }
    }
}

void func_actor_405800_801348E4(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    GsCOORDINATE2*   coord;
    s16              v;

    work  = (Actor405800Work*)arg0->work;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    v     = func_actor_405800_8013728C(arg0);
    if (v != 0) {
        if (v < 0x4E9) {
            func_actor_405800_801379F8(arg0);
            work3            = (Actor405800Work*)arg0->work;
            work3->field_846 = 2;
            work3->field_848 = 0;
            return;
        }
        work->field_98 = (u16)coord->coord.t[0] + ((rsin((s16)work->field_82 + 0x800) * (v - 0x100)) >> 12);
        work->field_9C = (u16)coord->coord.t[2] + ((rcos((s16)work->field_82 + 0x800) * (v - 0x100)) >> 12);
    } else {
        work->field_98 = (u16)coord->coord.t[0] + ((rsin((s16)work->field_82 + 0x800) * 0x1770) >> 12);
        work->field_9C = (u16)coord->coord.t[2] + ((rcos((s16)work->field_82 + 0x800) * 0x1770) >> 12);
    }
    func_actor_405800_801379F8(arg0);
    work2            = (Actor405800Work*)arg0->work;
    work2->field_84A = 4;
    work2->field_850 = 0x10;
    work2->field_872 = 0x15;
    work2->field_86E = 1;
    work->field_84C  = -0x2A;
    work->field_84E  = 0;
    work->field_842  = 0;
    work->field_848++;
}

void func_actor_405800_80134A64(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GsCOORDINATE2*   coord;
    s32              y;
    s32              id;
    s32              sound;
    s32              pan;

    work  = (Actor405800Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    work->field_842++;
    if ((s16)work->field_842 < 0x11) {
        func_actor_405800_80136A1C(arg0);
        return;
    }
    if ((s16)work->field_842 == 0x11) {
        work->field_85A = 0;
        work->field_88F = 1;
    }
    coord->coord.t[0] += ((s16)work->field_98 - coord->coord.t[0]) >> 4;
    coord->coord.t[2] += ((s16)work->field_9C - coord->coord.t[2]) >> 4;
    work->field_84C   += 6;
    work->field_84E   += work->field_84C;
    y                  = coord->coord.t[1] + work->field_84E;
    coord->coord.t[1]  = y;
    if (y >= (s16)work->field_92) {
        coord->coord.t[1] = (s16)work->field_92;
        id                = 0x40050003;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
        work2            = (Actor405800Work*)arg0->work;
        work2->field_84A = 2;
        work2->field_872 = 0x19;
        work2->field_850 = 0x10;
        work2->field_86E = 1;
        work->field_88F  = 0;
        work->field_848++;
    }
}

void func_actor_405800_80134C00(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    GsCOORDINATE2*   coord;

    work  = (Actor405800Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        work->field_82   = (work->field_82 + 0x800) & 0xFFF;
        work2            = (Actor405800Work*)arg0->work;
        work2->field_850 = 0x10;
        work2->field_872 = 2;
        work2->field_86E = 2;
        Actor405800_RebuildRotation(arg0);
        Actor405800_TickAnim(arg0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        ActorsShared80139dcc(arg0, 0xB, (ActorsShared80139dccPos*)&work->field_88);
        work->field_891  = 0;
        work3            = (Actor405800Work*)arg0->work;
        work3->field_846 = 2;
        work3->field_848 = 0;
    }
}

void func_actor_405800_80134E80(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GsCOORDINATE2*   coord;
    s32              id;
    s32              sound;
    s32              pan;
    s32              y;

    work  = (Actor405800Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    work->field_842++;
    if ((s16)work->field_842 < 0x11) {
        func_actor_405800_80136A1C();
        return;
    }
    if ((s16)work->field_842 == 0x11) {
        work->field_85A      = 0;
        work->field_88F      = 1;
        work->obj_594.flags &= ~0x4000;
    }
    if ((u32)(work->field_842 - 0x11) < 4U) {
        work->field_866 = (u16)work->field_866 + (-work->field_866 >> 1);
    }
    if ((s16)work->field_842 == 0x15) {
        work->field_86A = -0x9C4;
    }
    if ((s16)work->field_842 >= 0x15) {
        work->field_866 = (u16)work->field_866 + ((0xFF - work->field_866) >> 1);
    }
    y                  = coord->coord.t[1] + 0x190;
    coord->coord.t[1] += ((s16)work->field_9A - y) >> 3;
    work->field_80    += (0x800 - (s16)work->field_80) >> 3;
    if ((s16)work->field_9A >= coord->coord.t[1]) {
        work->field_866 = 0xFF;
        id              = 0x40050003;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
        work->obj_594.flags |= 0x4000;
        coord->coord.t[1]    = (s16)work->field_9A;
        work->field_80       = 0;
        work->field_84       = 0x800;
        work->field_82      += 0x800;
        Actor405800_RebuildRotation(arg0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work2            = (Actor405800Work*)arg0->work;
        work2->field_84A = 2;
        work2->field_850 = 0x10;
        work2->field_872 = 0x19;
        work2->field_86E = 1;
        work->field_88F  = 0;
        work->field_890  = 1;
        work->field_848++;
    }
}

void func_actor_405800_801351BC(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GsCOORDINATE2*   coord;

    work  = (Actor405800Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    work->field_842++;
    if ((s16)work->field_842 < 8) {
        work->field_866 = (u16)work->field_866 + (-work->field_866 >> 1);
        ActorsShared80139dcc(arg0, 3, (ActorsShared80139dccPos*)&work->field_88);
        return;
    }
    work->field_866   = (u16)work->field_866 + ((0xFF - work->field_866) >> 1);
    work->field_86A   = work->field_92;
    work->field_88.x += ((s16)work->field_98 - work->field_88.x) >> 2;
    work->field_88.z += ((s16)work->field_9C - work->field_88.z) >> 2;
    func_actor_405800_80138514(arg0, 3, &work->field_88);
    work->field_84C   += 2;
    work->field_84E   += work->field_84C;
    coord->coord.t[1] += work->field_84E;
    if ((work->field_80 & 0xFFF) != 0x800) {
        work->field_80 -= 0x80;
    }
    if ((s16)work->field_92 < coord->coord.t[1]) {
        work->field_890   = 0;
        work->field_866   = 0xFF;
        coord->coord.t[0] = (s16)work->field_98;
        coord->coord.t[1] = (s16)work->field_92;
        coord->coord.t[2] = (s16)work->field_9C;
        work->field_80    = 0;
        work->field_84    = 0;
        work->field_82   += 0x800;
        Actor405800_RebuildRotation(arg0);
        work2            = (Actor405800Work*)arg0->work;
        work2->field_84A = 2;
        work2->field_850 = 0x10;
        work2->field_872 = 0x19;
        work2->field_86E = 1;
        Actor405800_TickAnim(arg0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work->field_842 = 0;
        work->field_848++;
    }
}

void func_actor_405800_80135558(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    u32              sound;
    s32              pan;
    u32              rnd;

    work = (Actor405800Work*)arg0->work;
    if ((s16)work->field_842 == 0) {
        work->field_88F = 0;
        sound           = 0x40050006 | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan             = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords) << 24;
        pan           >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
        work->field_842++;
    }
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        if (work->field_85A != 3) {
            work2            = (Actor405800Work*)arg0->work;
            work2->field_84A = 2;
            work2->field_850 = 0x10;
            work2->field_872 = 0x14;
            work2->field_86E = 1;
            work->field_848++;
            return;
        }
        work->field_85A  = 0;
        rnd              = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = rnd;
        work->field_87C  = ((rnd >> 0x10) & 0x7F) + 0x1E;
        work3            = (Actor405800Work*)arg0->work;
        work3->field_846 = 5;
        work3->field_848 = 0;
    }
}

void func_actor_405800_801356A8(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    u16              count;
    u32              rnd;

    work = (Actor405800Work*)arg0->work;
    if (((func_actor_405800_80136A1C() << 0x10) == 0) && ((func_actor_405800_801373E0(arg0) << 0x10) == 0)) {
        count           = (u16)work->field_882 - 1;
        work->field_882 = count;
        if ((count << 0x10) == 0) {
            rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
            work->field_838 = ((rnd >> 0x10) & 0x3F) + 0x1E;
            work2           = (Actor405800Work*)arg0->work;
            Gp_LcgState     = rnd;
            if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
                work2->field_895 = 0x81;
                work2->field_896 = 0;
            }
            work3            = (Actor405800Work*)arg0->work;
            work3->field_846 = 2;
            work3->field_848 = 0;
        }
    }
}

/// Spawn the two child models from `D_actor_405800_801514B4`, parent them to
/// root parts 10 and 7 at +/-0x400 along X, turn each by -/+0x180 from an
/// identity rotation, copy the parent's texture page and CLUT row, and point
/// their light / color matrices at this actor's own.
void func_actor_405800_80135780(Task* arg0)
{
    Actor405800Work*         work;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           root;
    GsCOORDINATE2*           parent;
    GsCOORDINATE2*           parent2;
    Task*                    task;
    TmdObject*               obj;
    TmdObject*               dst;
    TmdObject*               src;
    MATRIX*                  mdst;
    ActorsShared8016a538Mat* pm;
    ActorsShared8016a538Mat* pm2;
    ActorsShared8016a538Mat  m;

    root              = ((TmdObject*)arg0->extra)->coords;
    work              = (Actor405800Work*)arg0->work;
    parent            = &root[7];
    parent2           = &root[10];
    task              = Task_SpawnFromTable(&D_actor_405800_801514B4, 0, 0, 0);
    work->field_824   = task;
    obj               = (TmdObject*)task->extra;
    coord             = obj->coords;
    obj->flags        = 0x80;
    coord->sub        = parent2;
    coord->coord.t[0] = 0x400;
    coord->coord.t[1] = 0;
    coord->coord.t[2] = 0;
    pm                = &m;
    pm->ident.m00_m01 = 0x1000;
    pm->ident.m02_m10 = 0;
    pm->ident.m11_m12 = 0x1000;
    pm->ident.m20_m21 = 0;
    pm->ident.m22     = 0x1000;
    func_8004BFF8(-0x180, &pm->mat);
    mdst          = &coord->coord;
    mdst->m[0][0] = pm->mat.m[0][0];
    mdst->m[0][1] = pm->mat.m[0][1];
    mdst->m[0][2] = pm->mat.m[0][2];
    mdst->m[1][0] = pm->mat.m[1][0];
    mdst->m[1][1] = pm->mat.m[1][1];
    mdst->m[1][2] = pm->mat.m[1][2];
    mdst->m[2][0] = pm->mat.m[2][0];
    mdst->m[2][1] = pm->mat.m[2][1];
    mdst->m[2][2] = pm->mat.m[2][2];
    src           = (TmdObject*)arg0->extra;
    dst           = (TmdObject*)task->extra;
    dst->tpage    = src->tpage;
    dst->clut     = src->clut;
    if (dst->buffer != NULL) {
        Tmd_ProcessStream(dst);
        Tmd_ProcessStream(dst);
    }
    obj->lightMtx = &work->matrix_40;
    obj->colorMtx = &work->matrix_20;
    task = work->field_828 = Task_SpawnFromTable(&D_actor_405800_801514B4, 1, 0, 0);
    obj                    = (TmdObject*)task->extra;
    coord                  = obj->coords;
    obj->flags             = 0x80;
    coord->sub             = parent;
    coord->coord.t[0]      = -0x400;
    coord->coord.t[1]      = 0;
    coord->coord.t[2]      = 0;
    src                    = (TmdObject*)arg0->extra;
    dst                    = (TmdObject*)task->extra;
    dst->tpage             = src->tpage;
    dst->clut              = src->clut;
    if (dst->buffer != NULL) {
        Tmd_ProcessStream(dst);
        Tmd_ProcessStream(dst);
    }
    pm2                = &m;
    pm2->ident.m00_m01 = 0x1000;
    pm2->ident.m02_m10 = 0;
    pm2->ident.m11_m12 = 0x1000;
    pm2->ident.m20_m21 = 0;
    pm2->ident.m22     = 0x1000;
    func_8004BFF8(0x180, &pm2->mat);
    mdst          = &coord->coord;
    mdst->m[0][0] = pm2->mat.m[0][0];
    mdst->m[0][1] = pm2->mat.m[0][1];
    mdst->m[0][2] = pm2->mat.m[0][2];
    mdst->m[1][0] = pm2->mat.m[1][0];
    mdst->m[1][1] = pm2->mat.m[1][1];
    mdst->m[1][2] = pm2->mat.m[1][2];
    mdst->m[2][0] = pm2->mat.m[2][0];
    mdst->m[2][1] = pm2->mat.m[2][1];
    mdst->m[2][2] = pm2->mat.m[2][2];
    obj->lightMtx = &work->matrix_40;
    obj->colorMtx = &work->matrix_20;
}

/// Animation state 2: the landing slam. Same two sound/tracking windows as
/// `func_actor_400600_80135998`, one frame-count pair per sound event.
void func_actor_405800_80135A3C(Task* arg0, s16 arg1)
{
    Actor405800Work* work;
    GsCOORDINATE2*   coord;
    /* The first window starts at frame 0. `start0` is still its own `u8`: the
     * width is what folds both of its tests against a literal zero, and the
     * wider first temp below is what keeps the zero arm a fresh constant
     * instead of a copy of it. */
    u8  start0;
    u32 tmp0;
    u8  tmp1;
    u8  tmp2;
    u8  end0;
    u8  start1;
    u8  end1;
    s32 id;
    u32 sound;
    u32 voice;
    s32 pan;

    work  = (Actor405800Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->field_872 != 2) {
        work->field_850 = 0x10;
        work->field_872 = 2;
        work->field_86E = 2;
        Actor405800_TickAnim(arg0);
    }
    start0 = 0;
    if (((Actor405800Work*)arg0->work)->field_850 == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((Actor405800Work*)arg0->work)->field_850) >> 4;
    }
    end0 = tmp0;
    if (((Actor405800Work*)arg0->work)->field_850 == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((Actor405800Work*)arg0->work)->field_850) >> 4;
    }
    start1 = tmp1;
    if (((Actor405800Work*)arg0->work)->field_850 == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((Actor405800Work*)arg0->work)->field_850) >> 4;
    }
    end1 = tmp2;
    if ((func_actor_405800_80137908(arg0) << 0x10) != 0) {
        work->field_874 = 0;
        work->field_850 = arg1;
    }
    if (work->field_874 == start0) {
        ActorsShared80139dcc(arg0, 0xB, (ActorsShared80139dccPos*)&work->field_88);
        id = 0x40050001;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0001;
        }
        /* `voice` is a plain copy that the compiler propagates away; writing
         * `sound = id | sound` instead swaps the operands of the `or`. */
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if (work->field_874 == start1) {
        ActorsShared80139dcc(arg0, 8, (ActorsShared80139dccPos*)&work->field_88);
        id = 0x40050002;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0002;
        }
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if (work->field_874 >= start0 && work->field_874 <= end0) {
        func_actor_405800_80138514(arg0, 0xB, &work->field_88);
        work->field_860 = 8;
    }
    if (work->field_874 >= start1 && work->field_874 <= end1) {
        func_actor_405800_80138514(arg0, 8, &work->field_88);
        work->field_860 = 0xB;
    }
    coord->flg = 0;
}

/// Animation state 4: drives the two sound/tracking windows the same way
/// `func_actor_400600_80135DDC` does, one frame-count pair per sound event.
void func_actor_405800_80135E28(Task* arg0)
{
    Actor405800Work* work;
    GsCOORDINATE2*   coord;
    u8               start0;
    u32              tmp0;
    u8               tmp1;
    u8               tmp2;
    u8               end0;
    u8               start1;
    u8               end1;
    s32              id;
    u32              sound;
    u32              voice;
    s32              pan;

    work  = (Actor405800Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->field_872 != 4) {
        work->field_850 = 0x10;
        work->field_84A = 4;
        work->field_872 = 4;
        work->field_86E = 1;
        Actor405800_TickAnim(arg0);
    }
    start0 = 0;
    if (((Actor405800Work*)arg0->work)->field_850 == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xD00 / ((Actor405800Work*)arg0->work)->field_850) >> 4;
    }
    end0 = tmp0;
    if (((Actor405800Work*)arg0->work)->field_850 == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xE00 / ((Actor405800Work*)arg0->work)->field_850) >> 4;
    }
    start1 = tmp1;
    if (((Actor405800Work*)arg0->work)->field_850 == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1B00 / ((Actor405800Work*)arg0->work)->field_850) >> 4;
    }
    end1 = tmp2;
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        work->field_874 = 0;
    }
    if (work->field_874 == start0) {
        ActorsShared80139dcc(arg0, 8, (ActorsShared80139dccPos*)&work->field_88);
        id = 0x40050001;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0001;
        }
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if (work->field_874 == start1) {
        ActorsShared80139dcc(arg0, 0xB, (ActorsShared80139dccPos*)&work->field_88);
        id = 0x40050002;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0002;
        }
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    if (work->field_874 >= start0 && work->field_874 <= end0) {
        func_actor_405800_80138514(arg0, 8, &work->field_88);
    }
    if (work->field_874 >= start1 && work->field_874 <= end1) {
        func_actor_405800_80138514(arg0, 0xB, &work->field_88);
    }
    coord->flg = 0;
}

void func_actor_405800_801361F8(Task* arg0)
{
    Actor405800Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   player;
    GameActor*       actor;
    SVECTOR          v;
    s16              py;

    work              = (Actor405800Work*)arg0->work;
    coord             = ((TmdObject*)arg0->extra)->coords;
    arg0              = (Task*)Gp_ActorSlots[0];
    work->field_70.vx = coord->coord.t[0];
    work->field_70.vy = coord->coord.t[1];
    work->field_70.vz = coord->coord.t[2];
    if (arg0 == NULL) {
        return;
    }
    player = ((GpActorWork*)arg0)->extra->coords;
    actor  = ((GpActorWork*)arg0)->actor;
    if (player->coord.t[0] < 0x3A98 || Gp_StateF0.field_0 == 0) {
        work->field_A8.vx = (u16)player->coord.t[0];
        work->field_A8.vy = (u16)player->coord.t[1];
        work->field_A8.vz = (u16)player->coord.t[2];
    } else {
        work->field_A8.vx = 0x834;
        py                = (u16)player->coord.t[1];
        work->field_A8.vz = 0xD48;
        work->field_85C   = 2;
        work->field_A8.vy = py;
    }
    v.vx            = (u16)work->field_A8.vx - (u16)coord->coord.t[0];
    v.vy            = (u16)work->field_A8.vy - (u16)coord->coord.t[1];
    v.vz            = (u16)work->field_A8.vz - (u16)coord->coord.t[2];
    work->field_852 = SquareRoot0(v.vx * v.vx + v.vz * v.vz);
    VectorNormalSS(&v, &v);
    work->field_856 = (ratan2(v.vx, v.vz) - work->field_82) & 0xFFF;
    work->field_854 = (ratan2(-v.vx, -v.vz) - actor->field_52) & 0xFFF;
}
