#include "common.h"
#include "actors/actor_104000.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"
#include "actors/actors_shared_8014adfc.h"
#include <psyq/inline_c.h>

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern MATRIX* D_80073B8C;
extern u32     Gp_LcgState;
extern u8      D_801153F4;

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void func_actor_204000_8014AC8C(Actor104000* arg0);

extern Actor104000* D_actor_204000_80156530[2];
extern Actor104000* D_actor_204000_80156538[6];

extern GpPairSrcE D_actor_204000_80150EA4;
extern u8         D_actor_204000_801562E4[]; // animation bank handed to `func_800B3F84`
extern u8         D_actor_204000_801564D0[];

/// Spawn state: allocates the work block, links the four collision objects and
/// the enemy node, seeds the size and HP from the enemy's level nibble, records
/// the spawn position and the points 1000 units ahead and behind it, then
/// starts in state 2 when `field_36` is 1 and state 7 otherwise.
void func_actor_204000_8014AED8(GpEnemy* arg0, Actor104000* arg1)
{
    Actor104000Obj2C* obj;
    GsCOORDINATE2*    coord;
    Actor104000Work*  work;
    GpRec18*          hits;
    SVECTOR           sv;
    VECTOR            pos;
    SVECTOR*          p;
    SVECTOR*          q;
    GpObj*            o1;
    GpObj*            o2;
    GpObj*            o3;
    GpObj*            o4;

    obj            = arg1->field_2C;
    coord          = obj->field_8;
    work           = Mem_Calloc(sizeof(Actor104000Work), 0);
    arg1->field_1C = work;
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, (Task*)arg1);
        return;
    }
    coord->sub      = &Gfx_ViewCoord;
    arg1->field_24  = D_actor_204000_801564D0;
    work->field_180 = 0;
    work->field_184 = 1;
    work->field_18C = 3;
    work->field_188 = 0;
    work->field_190 = 1;
    obj->field_C    = 0;
    func_800B3F84(&work->anim, D_actor_204000_801562E4, (GpAnimObj*)obj, work->poses, work->slots);

    o1           = &work->obj270;
    o1->field_8  = arg1->field_2C->field_8 + 1;
    o1->field_C  = work->rec1B0;
    o1->field_12 = -0x110;
    o1->field_10 = 0;
    o1->field_14 = 0;
    o1->field_18 = 0x3000C;
    o1->field_1C = 0x190;
    o1->flags    = 1;
    Gp_LinkObj(2, o1);
    o1->flags |= 0x4000;
    Gp_InitRec18Table(o1->field_C, 8, 0);

    o2           = &work->obj350;
    sv.vx        = 0;
    sv.vy        = -0x168;
    sv.vz        = 0;
    p            = &sv;
    hits         = work->hits;
    o2->field_8  = arg1->field_2C->field_8 + 2;
    o2->field_C  = hits;
    o2->field_10 = p->vx;
    o2->field_12 = p->vy;
    o2->field_14 = p->vz;
    o2->field_18 = 0x3000C;
    o2->field_1C = 0x168;
    o2->flags    = 1;
    Gp_LinkObj(2, o2);
    o2->flags |= 0x8000;
    Gp_InitRec18Table(o2->field_C, 8, 0);

    sv.vx        = 0;
    sv.vy        = 0;
    sv.vz        = 0;
    o3           = &work->obj388;
    o3->field_8  = &Gfx_ViewCoord;
    o3->field_C  = &work->rec370;
    o3->field_10 = p->vx;
    o3->field_12 = p->vy;
    o3->field_14 = p->vz;
    o3->field_1C = 0x500;
    o3->flags    = 1;
    Gp_LinkObj(3, o3);
    Gp_InitRec18Table(o3->field_C, 1, 0);

    o4           = &work->obj3C0;
    o4->field_8  = &Gfx_ViewCoord;
    o4->field_C  = &work->rec3A8;
    o4->field_10 = p->vx;
    o4->field_12 = p->vy;
    o4->field_14 = p->vz;
    o4->field_1C = 0x80;
    o4->flags    = 1;
    Gp_LinkObj(8, o4);
    Gp_InitRec18Table(o4->field_C, 1, 0);

    arg0->field_4     = &coord->coord;
    arg0->field_48    = 0;
    arg0->field_1C.vx = 0;
    arg0->field_1C.vy = 0;
    arg0->field_1C.vz = 0;
    arg0->field_18    = arg1->field_2C->field_8 + 2;
    Gp_LinkNode(&arg0->node);
    arg0->node.field_4 = 1;
    arg0->field_4C     = 0;
    arg0->field_40 = arg0->field_42 = D_actor_204000_80150EA4.field_4;
    arg0->field_50                  = &D_actor_204000_80150EA4;
    arg0->field_54                  = (s32)hits;
    work->field_170                 = 2;
    work->field_174                 = 1;
    work->field_176                 = 0x10;
    work->field_178                 = 0;
    func_actor_204000_8014AC8C(arg1);
    work->field_17E = 0;
    work->field_A   = 0;
    obj->field_1C   = &work->lightMtx;
    obj->field_20   = &work->colorMtx;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    work->field_1A0 = 5;
    work->field_1A2 = 0x14;
    if ((u16)(arg0->field_8 >> 12) % 2 == 1) {
        work->field_176 += arg0->field_8 >> 12;
        work->field_1A2 += arg0->field_8 >> 12;
        work->field_1A0 += arg0->field_8 >> 12;
    } else {
        work->field_176 -= (u16)(arg0->field_8 >> 12) / 2;
        work->field_1A2 -= arg0->field_8 >> 13;
        work->field_1A0 -= arg0->field_8 >> 13;
    }
    work->origin.vx = arg1->field_2C->field_8->coord.t[0];
    work->origin.vy = arg1->field_2C->field_8->coord.t[1];
    work->origin.vz = arg1->field_2C->field_8->coord.t[2];
    Gfx_MatrixCol2(&arg1->field_2C->field_8->coord, &sv);
    sv.vy = 0;
    q     = &sv;
    VectorNormalSS(q, q);
    gte_lddp(1000);
    gte_ldsv(q);
    gte_gpf12_real();
    gte_stsv(q);
    work->patrol[0].vx = arg1->field_2C->field_8->coord.t[0] + sv.vx;
    work->patrol[0].vy = arg1->field_2C->field_8->coord.t[1];
    work->patrol[0].vz = arg1->field_2C->field_8->coord.t[2] + sv.vz;
    work->patrol[1].vx = arg1->field_2C->field_8->coord.t[0] - sv.vx;
    work->patrol[1].vy = arg1->field_2C->field_8->coord.t[1];
    work->patrol[1].vz = arg1->field_2C->field_8->coord.t[2] - sv.vz;
    /* the gameplay prototype takes no argument, but this call site passes 0 */
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    if (arg1->field_36 == 0) {
        work->field_0 = 7;
    } else if (arg1->field_36 == 1) {
        work->field_0 = 2;
    } else {
        work->field_0 = 7;
    }
    work->field_2   = -1;
    work->field_479 = 0;
    work->field_47A = 0;
    arg1->state++;
}

#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor204000_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor104000RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)0x1F8003FC;
    ((Actor104000RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor104000RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor104000RangeScratch*)(head - 0xC))->dx *= ((Actor104000RangeScratch*)(head - 0xC))->dx;
    *(Actor104000RangeScratch**)0x1F8003FC        = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)0x1F8003FC                             = head;
    ret                                           = ((Actor104000RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

/// Wraps a 12-bit angle difference into [-0x800, 0x800].
static __inline__ s16 Actor204000_WrapAngle(s16 angle)
{
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    return angle;
}

extern u8 D_80072729;

/// Step `coord` `amount` units along its local Z axis unless movement is
/// frozen. Same body as `Actor01900_StepForwardHead`.
static __inline__ void Actor204000_StepForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        __asm__ volatile("nop; nop; .word 0x4B98003D");
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

extern Actor104000MsgArg D_actor_204000_80156350;
extern s32               D_actor_204000_80150EB4;
extern s32               D_actor_204000_80150EC0;
extern byte              D_actor_204000_80156330[];
extern byte              D_actor_204000_80156340[];

/// Lunge state: steps forward on frames 8 and 9, then from frame 9 on grabs
/// the player when within 600 units and a quarter turn of the facing, dispatches
/// the side-dependent grab message and snaps the model beside and facing them.
void func_actor_204000_8014B4AC(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work*       work;
    GpActorWork*           player;
    GameActor*             actor;
    Actor104000Obj2C*      obj;
    Actor104000AimScratch* head;
    Actor104000AimScratch* sc;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         pos;
    s16                    angle;
    s32                    mag;

    work   = arg1->field_1C;
    player = (GpActorWork*)Game_GetPtrSlot(3);
    actor  = player->actor;
    if (work->field_4 != 0) {
        obj                      = arg1->field_2C;
        arg1->field_20->field_14 = 0;
        Gp_ArmStateF0(1);
        obj->field_C        = 0;
        work->field_170     = 1;
        work->field_176     = 0x10;
        work->field_178     = 0;
        work->field_174     = 0xD;
        work->obj270.flags |= 0x4000;
        func_actor_204000_8014AC8C(arg1);
        work->field_6 = 0;
        return;
    }
    func_actor_204000_8014AC8C(arg1);
    work->field_6++;
    if ((s16)work->field_6 < 8) {
        return;
    }
    if ((s16)work->field_6 == 8) {
        Actor204000_StepForward(arg1->field_2C->field_8, 0x32);
        return;
    }
    if ((s16)work->field_6 == 9) {
        Actor204000_StepForward(arg1->field_2C->field_8, 0x32);
    }
    work->field_0 = 0xC;
    head          = (Actor104000AimScratch*)SCRATCH_SP;
    sc            = (Actor104000AimScratch*)(SCRATCH_SP -= sizeof(Actor104000AimScratch));
    pos           = arg1->field_2C->field_8;
    head[-1].d.vx = Player_Status.coordMtx->t[0] - pos->coord.t[0];
    sc->d.vy      = Player_Status.coordMtx->t[1] - pos->coord.t[1];
    sc->d.vz      = Player_Status.coordMtx->t[2] - pos->coord.t[2];
    coord         = arg1->field_2C->field_8;
    angle         = ratan2(head[-1].d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle     = Actor204000_WrapAngle(angle);
    if (!Actor204000_OutOfRange(&sc->d, 600)) {
        mag = (sc->angle >= 0) ? sc->angle : -sc->angle;
        if (mag < 0x200) {
            if (actor->field_954 != 2) {
                work->field_490 = 0xC;
                if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)work->field_47C, 0) == 0) {
                    coord     = player->extra->field_8;
                    angle     = ratan2(sc->d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
                    sc->angle = Actor204000_WrapAngle(angle);
                    if (sc->angle < 0) {
                        D_actor_204000_80156350.field_0 = D_actor_204000_80156330;
                    } else {
                        D_actor_204000_80156350.field_0 = D_actor_204000_80156340;
                    }
                    D_actor_204000_80156350.field_4 = 1;
                    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&D_actor_204000_80156350, 0);
                    work->field_0   = 0xB;
                    work->field_496 = 1;
                    Gfx_MatrixCol0(&player->extra->field_8->coord, &sc->d);
                    sc->d.vy = 0;
                    VectorNormalSS(&sc->d, &sc->d);
                    if (sc->angle < 0) {
                        gte_lddp(-0x3C);
                        gte_ldsv(&sc->d);
                        gte_gpf12_real();
                        gte_stsv(&sc->d);
                    } else {
                        gte_lddp(0x3C);
                        gte_ldsv(&sc->d);
                        gte_gpf12_real();
                        gte_stsv(&sc->d);
                    }
                    arg1->field_2C->field_8->coord.t[0] = player->extra->field_8->coord.t[0] + sc->d.vx;
                    arg1->field_2C->field_8->coord.t[1] = player->extra->field_8->coord.t[1];
                    arg1->field_2C->field_8->coord.t[2] = player->extra->field_8->coord.t[2] + sc->d.vz;
                    sc->d.vy                            = 0;
                    VectorNormalSS(&sc->d, &sc->d);
                    gte_lddp(-0x258);
                    gte_ldsv(&sc->d);
                    gte_gpf12_real();
                    gte_stsv(&sc->d);
                    arg1->field_2C->field_8->coord.t[0] += sc->d.vx;
                    arg1->field_2C->field_8->coord.t[2] += sc->d.vz;
                    sc->d.vx                             = -sc->d.vx;
                    sc->d.vy                             = -sc->d.vy;
                    sc->d.vz                             = -sc->d.vz;
                    sc->yaw                              = ratan2(sc->d.vx, sc->d.vz);
                    Gfx_RotMatrixY(&arg1->field_2C->field_8->coord, sc->yaw, 1);
                    arg1->field_2C->field_8->flg = 0;
                }
            }
        }
    }
    SCRATCH_SP += sizeof(Actor104000AimScratch);
}

/// Turns `coord` to face along its own Z axis in the XZ plane and scales the
/// rotation uniformly by `s`, working on a scratch block.
static __inline__ void Actor204000_FaceScale(GsCOORDINATE2* coord, s16 s)
{
    Actor104000FaceScratch* head;
    Actor104000FaceScratch* sc;

    head                                      = *(Actor104000FaceScratch**)G_SCRATCH_HEAD;
    sc                                        = head - 1;
    *(Actor104000FaceScratch**)G_SCRATCH_HEAD = sc;
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
    *(Actor104000FaceScratch**)G_SCRATCH_HEAD += 1;
}

/// Frames 0x5B onward of the collapse: drifts the model along its facing for the
/// first 0x13 frames, steps the effects keyed on `field_6`, then fades the colour
/// matrix out and grows the model over frames 0x5C-0x64.
void func_actor_204000_8014BC3C(Actor104000Ctx* arg0, Actor104000* arg1)
{
    SVECTOR           dir;
    SVECTOR*          d;
    VECTOR            scale;
    Actor104000Work*  work;
    Actor104000Obj2C* obj;
    s16               s;
    s32               pan;
    s32               id;

    work = arg1->field_1C;
    obj  = arg1->field_2C;
    if (work->field_4 != 0) {
        arg0->field_14        = 1;
        obj->field_C          = 0;
        work->obj350.flags   &= 0x7FFF;
        work->obj388.flags   &= 0x7FFF;
        work->obj3C0.flags   &= 0x7FFF;
        work->obj388.field_18 = Gp_PackObjPair((GpObj50*)arg0, 1);
        work->obj3C0.field_18 = 0x22222;
        work->field_6         = 0;
        work->obj270.flags   |= 0x4000;
        work->savedColorMtx   = work->colorMtx;
        work->field_174       = 0xE;
        work->field_170       = 1;
        work->field_178       = 0;
        func_actor_204000_8014AC8C(arg1);
        work->obj3C0.field_10           = arg1->field_2C->field_8->coord.t[0];
        work->obj3C0.field_12           = arg1->field_2C->field_8->coord.t[1] - 0x1F4;
        work->obj3C0.field_14           = arg1->field_2C->field_8->coord.t[2];
        work->obj388.field_10           = arg1->field_2C->field_8->coord.t[0];
        work->obj388.field_12           = arg1->field_2C->field_8->coord.t[1];
        work->obj388.field_14           = arg1->field_2C->field_8->coord.t[2];
        D_actor_204000_80156350.field_4 = 2;
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, &D_actor_204000_80156350, 0);
        work->field_6 = 0;
    }
    if ((s16)work->field_6 < 0x13) {
        Gfx_MatrixCol0(&arg1->field_2C->field_8->coord, &dir);
        d      = &dir;
        dir.vy = 0;
        VectorNormalSS(d, d);
        gte_lddp(0x15);
        gte_ldsv(d);
        gte_gpf12_real();
        gte_stsv(d);
        arg1->field_2C->field_8->coord.t[0] += dir.vx;
        arg1->field_2C->field_8->coord.t[2] += dir.vz;
        arg1->field_2C->field_8->flg         = 0;
    }
    func_actor_204000_8014AC8C(arg1);
    switch ((s16)(work->field_6 - 0x5B)) {
        case 0:
            if (work->field_496 == 1) {
                if (((GameActor*)((Task*)Game_GetPtrSlot(3))->work)->field_954 == 2) {
                    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
                }
                work->field_496 = 0;
            }
            arg1->field_2C->field_C = 2;
            break;
        case 1:
            Gp_SpawnScript18Ex((s32)&D_actor_204000_80150EB4, (s32)&D_actor_204000_80150EC0,
                               (s16)Gp_GetObjDepth((GpObj38*)arg1->field_2C->field_8));
            work->obj388.field_1C = 0x3E8;
            work->obj3C0.field_1C = 0xFA;
            work->obj388.flags   |= 0x8000;
            work->obj3C0.flags   |= 0x8000;
            Gp_SpawnEff(0x6009C, &arg1->field_2C->field_8[2], 1, NULL);
            break;
        case 2:
            work->obj3C0.field_1C = 0x1F4;
            break;
        case 3:
            work->obj3C0.field_1C = 0x3E8;
            work->obj388.flags   &= 0x7FFF;
            break;
        case 5:
            Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0xC);
            work->obj3C0.flags &= 0x7FFF;
            break;
        case 7:
            if ((s8)work->field_479 == 0) {
                Gp_SpawnEff(0x6009E, arg1->field_2C->field_8, 0, NULL);
            }
            obj->field_C = 0x80;
            id           = ((arg0->field_8 >> 12) << 8) | 0x40280004;
            pan          = (s8)Gp_GetObjPan((GpObj38*)arg1->field_2C->field_8);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)arg1->field_2C->field_8));
            break;
        case 28:
            work->field_0 = 0;
            break;
        default:
            work->colorMtx = work->savedColorMtx;
            break;
    }
    work->colorMtx = work->savedColorMtx;
    if ((u16)(work->field_6 - 0x17) < 0x44) {
        work->colorMtx.t[0] += ((s16)work->field_6 - 0x16) * 0x60;
    }
    if ((u16)(work->field_6 - 0x5C) < 9) {
        s = 0xBB8 - ((s16)work->field_6 - 0x5C) * 600;
        if (s < 0x4B0) {
            scale.vx = scale.vy = scale.vz = 0;
            Actor204000_FaceScale(arg1->field_2C->field_8, 0x1000);
            ScaleMatrix(&work->colorMtx, &scale);
            work->colorMtx.t[0] = work->colorMtx.t[1] = work->colorMtx.t[2] = 0;
            Actor204000_FaceScale(arg1->field_2C->field_8, 0x1000);
        } else {
            scale.vx = scale.vy = scale.vz = s;
            work->colorMtx                 = work->savedColorMtx;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(s);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12_real();
            gte_stlvl(work->colorMtx.t);
            s = ((s16)work->field_6 - 0x5A) * 0x400 + 0x1000;
            if (s > 0x2000) {
                s = 0x2000;
            }
            Actor204000_FaceScale(arg1->field_2C->field_8, s);
        }
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}

/// Restarts the actor when `field_4` is set; otherwise steps it, occasionally
/// switches to state 3 on a random roll, and arms the player state when the
/// camera target comes within 2000 units.
void func_actor_204000_8014C51C(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work*  work;
    GsCOORDINATE2*    coord;
    SVECTOR           delta;
    SVECTOR*          d;
    Actor104000Obj2C* obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                 = arg1->field_2C;
        arg0->field_14      = 0;
        obj->field_C        = 0;
        work->field_174     = 5;
        work->field_170     = 1;
        work->field_178     = 0;
        work->obj350.flags |= 0x8000;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj270.flags |= 0x4000;
        func_actor_204000_8014AC8C(arg1);
        return;
    }
    func_actor_204000_8014AC8C(arg1);
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
    if (!Actor204000_OutOfRange(d, 2000)) {
        Gp_ArmStateF0(1);
        work->field_0 = 3;
    }
}

s32 func_actor_204000_8014A06C(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* d);
s32 func_actor_204000_8014A5B8(GsCOORDINATE2* coord, GpRec18* recs, s32 count);

/// Chasing state: restarts the actor when `field_4` is set; otherwise turns
/// toward the camera target by at most 0x10 a frame and steps forward, counting
/// frames spent more than 1000 units away (state 8 after 240), and switches to
/// state 10 within 600 units and an eighth turn of the facing.
void func_actor_204000_8014C710(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work*        work;
    Actor104000TurnScratch* head;
    Actor104000TurnScratch* sc;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          target;
    GsCOORDINATE2*          pos;
    Actor104000Obj2C*       obj;
    s16                     angle;
    s32                     mag;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                 = arg1->field_2C;
        arg0->field_14      = 0;
        obj->field_C        = 0;
        work->field_174     = 3;
        work->field_170     = 1;
        work->field_178     = 0x10;
        work->obj350.flags |= 0x8000;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj270.flags |= 0x4000;
        Gp_ArmStateF0(1);
        func_actor_204000_8014AC8C(arg1);
        work->field_494 = 0;
        return;
    }
    head = (Actor104000TurnScratch*)SCRATCH_SP;
    sc   = (Actor104000TurnScratch*)(SCRATCH_SP -= sizeof(Actor104000TurnScratch));
    func_actor_204000_8014AC8C(arg1);
    pos           = arg1->field_2C->field_8;
    head[-1].d.vx = Player_Status.coordMtx->t[0] - pos->coord.t[0];
    sc->d.vy      = Player_Status.coordMtx->t[1] - pos->coord.t[1];
    sc->d.vz      = Player_Status.coordMtx->t[2] - pos->coord.t[2];
    coord         = arg1->field_2C->field_8;
    angle         = ratan2(head[-1].d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle     = Actor204000_WrapAngle(angle);
    if (sc->angle > 0x10) {
        sc->angle = 0x10;
    }
    if (sc->angle < -0x10) {
        sc->angle = -0x10;
    }
    sc->angle += ratan2(-arg1->field_2C->field_8->coord.m[2][0], arg1->field_2C->field_8->coord.m[2][2]);
    Gfx_RotMatrixY(&arg1->field_2C->field_8->coord, sc->angle, 1);
    Actor204000_StepForward(arg1->field_2C->field_8, 0x14);
    func_actor_204000_8014A5B8(arg1->field_2C->field_8, work->rec1B0, 8);
    if (Actor204000_OutOfRange(&sc->d, 1000)) {
        work->field_494++;
    } else {
        work->field_494 = 0;
    }
    func_actor_204000_8014A06C(arg1->field_2C->field_8, work->hits, 8, &sc->d);
    arg1->field_2C->field_8->flg = 0;
    sc->d.vx                     = work->origin.vx - arg1->field_2C->field_8->coord.t[0];
    sc->d.vy                     = 0;
    sc->d.vz                     = work->origin.vz - arg1->field_2C->field_8->coord.t[2];
    Actor204000_OutOfRange(&sc->d, 3000);
    if (work->field_494 > 0xF0) {
        work->field_0 = 8;
    }
    target    = arg1->field_2C->field_8;
    sc->d.vx  = Player_Status.coordMtx->t[0] - target->coord.t[0];
    sc->d.vy  = Player_Status.coordMtx->t[1] - target->coord.t[1];
    sc->d.vz  = Player_Status.coordMtx->t[2] - target->coord.t[2];
    coord     = arg1->field_2C->field_8;
    angle     = ratan2(sc->d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = Actor204000_WrapAngle(angle);
    if (!Actor204000_OutOfRange(&sc->d, 600)) {
        mag = (sc->angle >= 0) ? sc->angle : -sc->angle;
        if (mag < 0x200) {
            work->field_0 = 0xA;
        }
    }
    SCRATCH_SP += sizeof(Actor104000TurnScratch);
}

/// Frames 0x28 onward of the collapse: steps the effects keyed on `field_6`,
/// then fades the colour matrix out and grows the model over frames 0x2A-0x32.
void func_actor_204000_8014CD68(Actor104000Ctx* arg0, Actor104000* arg1)
{
    VECTOR            scale;
    Actor104000Work*  work;
    Actor104000Obj2C* obj;
    s16               s;
    s32               pan;
    s32               id;

    work = arg1->field_1C;
    obj  = arg1->field_2C;
    if (work->field_4 != 0) {
        arg0->field_14        = 1;
        obj->field_C          = 0;
        work->obj350.flags   |= 0x8000;
        work->obj388.flags   &= 0x7FFF;
        work->obj3C0.flags   &= 0x7FFF;
        work->obj388.field_18 = Gp_PackObjPair((GpObj50*)arg0, 1);
        work->obj3C0.field_18 = 0x22222;
        work->field_6         = 0;
        work->obj270.flags   |= 0x4000;
        work->savedColorMtx   = work->colorMtx;
        work->field_178       = 0;
        func_actor_204000_8014AC8C(arg1);
        work->obj3C0.field_10 = arg1->field_2C->field_8->coord.t[0];
        work->obj3C0.field_12 = arg1->field_2C->field_8->coord.t[1] - 0x1F4;
        work->obj3C0.field_14 = arg1->field_2C->field_8->coord.t[2];
        work->obj388.field_10 = arg1->field_2C->field_8->coord.t[0];
        work->obj388.field_12 = arg1->field_2C->field_8->coord.t[1];
        work->obj388.field_14 = arg1->field_2C->field_8->coord.t[2];
        if (work->field_194 == 0x1003) {
            D_actor_204000_80156538[arg0->field_8 >> 12] = NULL;
        }
        return;
    }
    func_actor_204000_8014AC8C(arg1);
    switch ((s16)(work->field_6 - 0x28)) {
        case 0:
            work->obj350.flags &= 0x7FFF;
            break;
        case 1:
            if (work->field_496 == 1) {
                if (((GameActor*)((Task*)Game_GetPtrSlot(3))->work)->field_954 == 2) {
                    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
                }
                work->field_496 = 0;
            }
            arg1->field_2C->field_C = 2;
            break;
        case 2:
            Gp_SpawnScript18Ex((s32)&D_actor_204000_80150EB4, (s32)&D_actor_204000_80150EC0,
                               (s16)Gp_GetObjDepth((GpObj38*)arg1->field_2C->field_8));
            work->obj388.field_1C = 0x3E8;
            work->obj3C0.field_1C = 0xFA;
            Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, 0, 0x7DE);
            work->obj388.flags |= 0x8000;
            work->obj3C0.flags |= 0x8000;
            Gp_SpawnEff(0x6009C, &arg1->field_2C->field_8[2], 1, NULL);
            break;
        case 3:
            work->obj3C0.field_1C = 0x1F4;
            break;
        case 4:
            work->obj3C0.field_1C = 0x3E8;
            break;
        case 6:
            Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0xC);
            work->obj3C0.flags &= 0x7FFF;
            break;
        case 8:
            if ((s8)work->field_479 == 0) {
                Gp_SpawnEff(0x6009E, arg1->field_2C->field_8, 0, NULL);
            }
            id  = ((arg0->field_8 >> 12) << 8) | 0x40280004;
            pan = (s8)Gp_GetObjPan((GpObj38*)arg1->field_2C->field_8);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)arg1->field_2C->field_8));
            break;
        case 10:
            work->obj388.flags &= 0x7FFF;
            obj->field_C        = 0x80;
            break;
        case 12:
            obj->field_C |= 4;
            break;
        case 29:
            work->field_0  = 0;
            arg0->field_40 = 0;
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
        s = 0xBB8 - ((s16)work->field_6 - 0x2A) * 600;
        if (s < 0x4B0) {
            scale.vx = scale.vy = scale.vz = 0;
            Actor204000_FaceScale(arg1->field_2C->field_8, 0x1000);
            ScaleMatrix(&work->colorMtx, &scale);
            work->colorMtx.t[0] = work->colorMtx.t[1] = work->colorMtx.t[2] = 0;
            Actor204000_FaceScale(arg1->field_2C->field_8, 0x1000);
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
            Actor204000_FaceScale(arg1->field_2C->field_8, s);
        }
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}

/// Death state: saves the colour matrix, then fades it and grows the model
/// over frames 13-21 while stepping through the collapse effects.
void func_actor_204000_8014D5B8(Actor104000Ctx* arg0, Actor104000* arg1)
{
    VECTOR            scale;
    Actor104000Work*  work;
    Actor104000Obj2C* obj;
    s16               s;
    s32               pan;
    s32               id;

    work = arg1->field_1C;
    obj  = arg1->field_2C;
    if (work->field_4 != 0) {
        arg0->field_14        = 1;
        obj->field_C          = 0;
        work->obj350.flags   &= 0x7FFF;
        work->obj388.flags   &= 0x7FFF;
        work->obj3C0.flags   &= 0x7FFF;
        work->obj388.field_18 = Gp_PackObjPair((GpObj50*)arg0, 1);
        work->obj3C0.field_18 = 0x22222;
        work->field_6         = 0;
        work->obj270.flags   &= 0xBFFF;
        work->savedColorMtx   = work->colorMtx;
        work->field_174       = 0xA;
        work->field_170       = 1;
        work->field_178       = 0;
        work->field_176       = 0x2C;
        func_actor_204000_8014AC8C(arg1);
        work->obj3C0.field_10 = arg1->field_2C->field_8->coord.t[0];
        work->obj3C0.field_12 = arg1->field_2C->field_8->coord.t[1] - 0x1F4;
        work->obj3C0.field_14 = arg1->field_2C->field_8->coord.t[2];
        work->obj388.field_10 = arg1->field_2C->field_8->coord.t[0];
        work->obj388.field_12 = arg1->field_2C->field_8->coord.t[1];
        work->obj388.field_14 = arg1->field_2C->field_8->coord.t[2];
        Gp_ArmStateF0(1);
        if (work->field_194 == 0x1003) {
            D_actor_204000_80156538[arg0->field_8 >> 12] = NULL;
        }
    }
    func_actor_204000_8014AC8C(arg1);
    switch ((s16)(work->field_6 - 0xD)) {
        case 0:
            id  = ((arg0->field_8 >> 12) << 8) | 0x40280004;
            pan = (s8)Gp_GetObjPan((GpObj38*)arg1->field_2C->field_8);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)arg1->field_2C->field_8));
            arg1->field_2C->field_C = 2;
            break;
        case 1:
            work->obj388.field_1C = 0x3E8;
            work->obj388.flags   |= 0x8000;
            Gp_SpawnEff(0x6009C, &arg1->field_2C->field_8[2], 1, NULL);
            Gp_SpawnScript18((s32)&D_actor_204000_80150EB4, (s32)&D_actor_204000_80150EC0);
            break;
        case 2:
            Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0xC);
            work->obj3C0.field_1C = 0xFA;
            work->obj388.flags   &= 0x7FFF;
            work->obj3C0.flags   |= 0x8000;
            break;
        case 3:
            work->obj3C0.field_1C = 0x1F4;
            break;
        case 4:
            work->obj3C0.field_1C = 0x3E8;
            break;
        case 6:
            work->obj3C0.flags &= 0x7FFF;
            if ((s8)work->field_479 == 0) {
                Gp_SpawnEff(0x6009E, arg1->field_2C->field_8, 0, NULL);
            }
            break;
        case 8:
            obj->field_C = 0x80;
            break;
        case 10:
            obj->field_C |= 4;
            break;
        case 25:
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
        Actor204000_FaceScale(arg1->field_2C->field_8, s);
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}

/// Picks a random offset and coordinate index for an effect from the hit
/// angle `arg1` (front, back, right or left), copies it into `work->eff` and
/// spawns the effect for hit id `arg2`.
void func_actor_204000_8014DB50(Actor104000* arg0, s16 arg1, u32 arg2)
{
    SVECTOR*         sc;
    Actor104000Work* work;
    s32              mag;
    GsCOORDINATE2*   coord;

    sc   = (SVECTOR*)(SCRATCH_SP -= sizeof(SVECTOR));
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->field_1C;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 2;
            sc->vx  = 80;
            sc->vy  = -180;
            sc->vz  = 330;
        } else {
            sc->pad = 2;
            sc->vx  = -60;
            sc->vy  = -150;
            sc->vz  = 300;
        }
    } else if (mag > 0x600) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 1;
            sc->vx  = 0;
            sc->vy  = 0;
            sc->vz  = -180;
        } else {
            sc->pad = 2;
            sc->vx  = 2;
            sc->vy  = -50;
            sc->vz  = -50;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 5;
            sc->vx  = 100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 5;
            sc->vx  = 120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 4;
            sc->vx  = -100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 4;
            sc->vx  = -120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    }
    work->effOfs      = *sc;
    coord             = &arg0->field_2C->field_8[sc->pad];
    work->eff.field_4 = 0x100;
    work->eff.field_6 = 1;
    work->eff.field_0 = coord;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->field_2C->field_8[sc->pad], &work->effOfs, &work->eff);
    SCRATCH_SP += sizeof(SVECTOR);
}

/// Applies the first type-2 hit in `work->hits`: computes its damage, turns the
/// model toward the hit, plays the impact sound and subtracts the damage from
/// `arg0->field_40`, switching to state 6 once it runs out.
void func_actor_204000_8014DDD4(GpEnemy* arg0, Actor104000* arg1)
{
    Actor104000HitScratch* sc;
    Actor104000Work*       work;
    GpRec18*               recs;
    SVECTOR*               pos;
    s32                    mask;
    s32                    kind;
    s32                    id;
    s16                    angle;
    s32                    snd;
    s32                    pan;
    s16                    i;

    work = arg1->field_1C;
    sc   = (Actor104000HitScratch*)(SCRATCH_SP -= sizeof(Actor104000HitScratch));
    pos  = &sc->pos;
    recs = work->hits;
    i    = 0;
    mask = 0xFFFF0000;
    kind = 0x20000;
scan:
    if (recs[i].field_4 == 0) {
        goto missed;
    }
    if ((recs[i].field_4 & mask) == kind) {
        pos->vx = recs[i].field_8;
        pos->vy = recs[i].field_A;
        pos->vz = recs[i].field_C;
        id      = recs[i].field_4;
        goto found;
    }
    i++;
    if (i < 8) {
        goto scan;
    }
missed:
    id = 0;
found:
    sc->id = id;

    if (id != 0) {
        sc->dmg                      = Gp_ComputeDamage(sc->id, 0, 0, 0x1000);
        arg1->field_2C->field_8->flg = 0;
        Gp_UpdateCoord(arg1->field_2C->field_8);
        sc->d.vx = arg1->field_2C->field_8->workm.t[0];
        sc->d.vy = arg1->field_2C->field_8->workm.t[1];
        sc->d.vz = arg1->field_2C->field_8->workm.t[2];
        sc->d.vx = sc->pos.vx - arg1->field_2C->field_8->workm.t[0];
        sc->d.vy = sc->pos.vy - arg1->field_2C->field_8->workm.t[1];
        sc->d.vz = sc->pos.vz - arg1->field_2C->field_8->workm.t[2];
        angle    = ratan2(sc->d.vx, sc->d.vz) -
                ratan2(-arg1->field_2C->field_8->workm.m[2][0], arg1->field_2C->field_8->workm.m[2][2]);
        sc->angle = angle;
        sc->angle = Actor204000_WrapAngle(angle);
        func_actor_204000_8014DB50(arg1, sc->angle, sc->id);
        snd = ((arg0->field_8 >> 12) << 8) | 0x40280003;
        pan = (s8)Gp_GetObjPan((GpObj38*)arg1->field_2C->field_8);
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)arg1->field_2C->field_8));
        func_800E2C78((GpObj40*)arg0, sc->id, sc->dmg, 0);
        func_800DA6E8(&arg0->node, sc->dmg, 0);
        arg0->field_40 -= sc->dmg;
        if (arg0->field_40 <= 0) {
            work->field_0 = 6;
        }
        if (work->field_496 == 1) {
            if (((GpActorWork*)Game_GetPtrSlot(3))->actor->field_954 == 2) {
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
            }
            work->field_496 = 0;
        }
    }
    SCRATCH_SP += sizeof(Actor104000HitScratch);
}

/// Patrol state: restarts the actor when `field_4` is set; otherwise turns the
/// model toward the current patrol point by at most 0x20 a frame and steps it
/// forward, swapping patrol points within 400 units or after 97 blocked frames,
/// switching to state 4 when the camera target is within 2000 units and either
/// inside a quarter turn of the facing or within 1000 units, and occasionally to
/// state 1 once `field_17C` passes 20.
void func_actor_204000_8014E14C(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work*        work;
    Actor104000TurnScratch* head;
    Actor104000TurnScratch* sc;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          target;
    Actor104000Obj2C*       obj;
    s16                     angle;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                 = arg1->field_2C;
        arg0->field_14      = 0;
        obj->field_C        = 0;
        work->field_174     = 2;
        work->field_170     = 1;
        work->field_178     = 0;
        work->patrolIdx     = 0;
        work->obj350.flags |= 0x8000;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj270.flags |= 0x4000;
        func_actor_204000_8014AC8C(arg1);
        work->field_6 = 0;
        return;
    }
    head          = (Actor104000TurnScratch*)SCRATCH_SP;
    sc            = (Actor104000TurnScratch*)(SCRATCH_SP -= sizeof(Actor104000TurnScratch));
    head[-1].d.vx = work->patrol[work->patrolIdx].vx - arg1->field_2C->field_8->coord.t[0];
    sc->d.vy      = 0;
    sc->d.vz      = work->patrol[work->patrolIdx].vz - arg1->field_2C->field_8->coord.t[2];
    coord         = arg1->field_2C->field_8;
    angle         = ratan2(head[-1].d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle     = Actor204000_WrapAngle(angle);
    if (sc->angle > 0x20) {
        sc->angle = 0x20;
    }
    if (sc->angle < -0x20) {
        sc->angle = -0x20;
    }
    sc->angle += ratan2(-arg1->field_2C->field_8->coord.m[2][0], arg1->field_2C->field_8->coord.m[2][2]);
    Gfx_RotMatrixY(&arg1->field_2C->field_8->coord, sc->angle, 1);
    Actor204000_StepForward(arg1->field_2C->field_8, 5);
    if (func_actor_204000_8014A5B8(arg1->field_2C->field_8, work->rec1B0, 8)) {
        work->field_6++;
    }
    if (!Actor204000_OutOfRange(&sc->d, 400) || (s16)work->field_6 > 0x60) {
        if (work->patrolIdx == 0) {
            work->patrolIdx = 1;
        } else {
            work->patrolIdx = 0;
        }
        work->field_6 = 0;
    }
    func_actor_204000_8014A06C(arg1->field_2C->field_8, work->hits, 8, &sc->d);
    target   = arg1->field_2C->field_8;
    sc->d.vx = Player_Status.coordMtx->t[0] - target->coord.t[0];
    sc->d.vy = Player_Status.coordMtx->t[1] - target->coord.t[1];
    sc->d.vz = Player_Status.coordMtx->t[2] - target->coord.t[2];
    if (!Actor204000_OutOfRange(&sc->d, 2000)) {
        coord = arg1->field_2C->field_8;
        angle = ratan2(sc->d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (Actor204000_WrapAngle(angle) < 0x400 || !Actor204000_OutOfRange(&sc->d, 1000)) {
            work->field_0 = 4;
        }
    }
    func_actor_204000_8014AC8C(arg1);
    arg1->field_2C->field_8->flg = 0;
    if ((work->field_58 & 2) && work->field_17C > 0x14) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 0x10) & 7)) {
            work->field_0 = 1;
        }
    }
    SCRATCH_SP += sizeof(Actor104000TurnScratch);
}

/// Walking state: restarts the actor when `field_4` is set; otherwise turns the
/// model toward its spawn point by at most 0x10 a frame and steps it forward,
/// switching to state 1 within 80 units of the spawn point and to state 4 when
/// the camera target is within 2000 units and either inside a quarter turn of
/// the facing or within 1000 units.
void func_actor_204000_8014E7E0(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work*        work;
    Actor104000TurnScratch* head;
    Actor104000TurnScratch* sc;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          target;
    Actor104000Obj2C*       obj;
    s16                     angle;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                 = arg1->field_2C;
        arg0->field_14      = 0;
        obj->field_C        = 0;
        work->field_174     = 2;
        work->field_170     = 1;
        work->field_178     = 0;
        work->obj350.flags |= 0x8000;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj270.flags |= 0x4000;
        func_actor_204000_8014AC8C(arg1);
        work->field_494 = 0;
        return;
    }
    head = (Actor104000TurnScratch*)SCRATCH_SP;
    sc   = (Actor104000TurnScratch*)(SCRATCH_SP -= sizeof(Actor104000TurnScratch));
    func_actor_204000_8014AC8C(arg1);
    arg1->field_2C->field_8->flg = 0;
    head[-1].d.vx                = work->origin.vx - arg1->field_2C->field_8->coord.t[0];
    sc->d.vy                     = 0;
    sc->d.vz                     = work->origin.vz - arg1->field_2C->field_8->coord.t[2];
    coord                        = arg1->field_2C->field_8;
    angle                        = ratan2(head[-1].d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle                    = Actor204000_WrapAngle(angle);
    if (sc->angle > 0x10) {
        sc->angle = 0x10;
    }
    if (sc->angle < -0x10) {
        sc->angle = -0x10;
    }
    sc->angle += ratan2(-arg1->field_2C->field_8->coord.m[2][0], arg1->field_2C->field_8->coord.m[2][2]);
    Gfx_RotMatrixY(&arg1->field_2C->field_8->coord, sc->angle, 1);
    Actor204000_StepForward(arg1->field_2C->field_8, 8);
    func_actor_204000_8014A5B8(arg1->field_2C->field_8, work->rec1B0, 8);
    if (!Actor204000_OutOfRange(&sc->d, 80)) {
        work->field_0 = 1;
    }
    func_actor_204000_8014A06C(arg1->field_2C->field_8, work->hits, 8, &sc->d);
    target   = arg1->field_2C->field_8;
    sc->d.vx = Player_Status.coordMtx->t[0] - target->coord.t[0];
    sc->d.vy = Player_Status.coordMtx->t[1] - target->coord.t[1];
    sc->d.vz = Player_Status.coordMtx->t[2] - target->coord.t[2];
    if (!Actor204000_OutOfRange(&sc->d, 2000)) {
        coord = arg1->field_2C->field_8;
        angle = ratan2(sc->d.vx, sc->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (Actor204000_WrapAngle(angle) < 0x400 || !Actor204000_OutOfRange(&sc->d, 1000)) {
            work->field_0 = 4;
        }
    }
    SCRATCH_SP += sizeof(Actor104000TurnScratch);
}

/// Restarts the actor when `field_4` is set; otherwise waits 50 frames, then
/// drops the model with growing speed, unwinding its Z roll by at most 0x92 a
/// frame, and on landing plays the impact sound and switches to state 3.
void func_actor_204000_8014EDC4(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work* work;
    s32              id;
    s32              pan;
    s32              mag;
    s32              rot;
    s32              step;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        arg1->field_2C->field_C = 0;
        work->field_174         = 5;
        work->field_170         = 1;
        work->field_178         = 0;
        work->obj350.flags     |= 0x8000;
        work->obj388.flags     &= 0x7FFF;
        work->obj3C0.flags     &= 0x7FFF;
        work->obj270.flags     |= 0x4000;
        func_actor_204000_8014AC8C(arg1);
        ratan2(-arg1->field_2C->field_8->coord.m[2][0], arg1->field_2C->field_8->coord.m[2][2]);
        arg1->field_2C->field_8->flg = 0;
        work->field_19A              = 10;
        work->field_198              = 0;
        work->field_6                = 0;
        work->field_19C              = 0x800;
        work->field_479              = 1;
        return;
    }
    if ((s16)work->field_6 < 0x32) {
        work->field_6++;
        return;
    }
    work->field_19A                     += 4;
    work->field_198                     += work->field_19A;
    arg1->field_2C->field_8->coord.t[1] += (s16)work->field_198;
    if (arg1->field_2C->field_8->coord.t[1] >= -0x12B) {
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x03100000) {
            id  = ((arg0->field_8 >> 12) << 8) | 0x53100006;
            pan = (s8)Gp_GetObjPan((GpObj38*)arg1->field_2C->field_8);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)arg1->field_2C->field_8));
        }
        arg1->field_2C->field_8->coord.t[1] = 0;
        work->field_0                       = 3;
        work->field_479                     = 0;
        Gfx_RotMatrixZ(&arg1->field_2C->field_8->coord, -work->field_19C, 0);
    } else {
        rot = work->field_19C;
        if (rot != 0) {
            mag  = __builtin_abs(rot);
            step = rot;
            SOFT_TOUCH_REG(step);
            step = -step;
            if (mag >= 0x93) {
                step = -0x92;
                if (rot < 0) {
                    step = 0x92;
                }
            }
            Gfx_RotMatrixZ(&arg1->field_2C->field_8->coord, (s16)step, 0);
            work->field_19C += step;
        }
    }
    arg1->field_2C->field_8->flg = 0;
    func_actor_204000_8014AC8C(arg1);
}

/// Restarts the actor when `field_4` is set; otherwise cycles `field_6` through
/// a 32-frame loop that resets the model position, steps it back and forth
/// and changes `field_176`, then spins it and raises `field_14` in view 5.
void func_actor_204000_8014F04C(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work* work;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        arg1->field_2C->field_C = 0;
        work->obj350.flags     |= 0x8000;
        work->obj388.flags     &= 0x7FFF;
        work->obj3C0.flags     &= 0x7FFF;
        work->obj270.flags     |= 0x4000;
        arg0->field_14          = 0;
        work->field_174         = 3;
        work->field_170         = 2;
        work->field_178         = 0;
        func_actor_204000_8014AC8C(arg1);
        Gfx_RotMatrixX(&arg1->field_2C->field_8->coord, 0x400, 0);
        arg1->field_2C->field_8->flg = 0;
        work->field_479              = 1;
        work->obj350.field_8         = &Gfx_ViewCoord;
        work->obj350.field_10        = -0x3AC;
        work->obj350.field_12        = -0xF0;
        work->field_6                = 0;
        work->obj350.field_14        = 0x166C;
        return;
    }
    switch ((s16)++work->field_6 % 32) {
        case 0:
            work->field_176                     = 0x40;
            arg1->field_2C->field_8->coord.t[0] = -0x3AC;
            arg1->field_2C->field_8->coord.t[1] = -0xF0;
            arg1->field_2C->field_8->coord.t[2] = 0x166C;
            break;
        case 1:
        case 2:
        case 4:
        case 5:
        case 7:
        case 8:
            Actor204000_StepForward(arg1->field_2C->field_8, -0x78);
            break;
        case 11:
        case 12:
        case 14:
            Actor204000_StepForward(arg1->field_2C->field_8, 0xC8);
            break;
        case 17:
            work->field_176 = 0x10;
            break;
        case 25:
            work->field_176 = 8;
            break;
    }
    Gfx_RotMatrixY(&arg1->field_2C->field_8->coord, 0x44C, 1);
    Gfx_RotMatrixX(&arg1->field_2C->field_8->coord, 0x190, 0);
    arg1->field_2C->field_8->flg = 0;
    func_actor_204000_8014AC8C(arg1);
    if ((u8)Gp_GetViewIndex() == 5) {
        arg0->field_14 = 1;
        Gp_ClearNodeSlots(&((GpEnemy*)arg0)->node);
        return;
    }
    arg0->field_14 = 0;
}

/// Restarts the actor when `field_4` is set; otherwise runs state 0xC (drop the
/// model to the ground, then hop forward and tip it over), state 0x10 (wait for
/// the flag or 80 frames) and state 0x11 (slide along `dir` while rolling).
void func_actor_204000_8014F3E8(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          sv;
    s32              y;
    u16              h;
    s16              t;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        arg1->field_2C->field_C = 0;
        work->obj350.flags     |= 0x8000;
        work->obj388.flags     &= 0x7FFF;
        work->obj3C0.flags     &= 0x7FFF;
        work->obj270.flags     &= 0xBFFF;
        arg0->field_14          = 0;
        work->field_174         = 0xC;
        work->field_170         = 2;
        work->field_178         = 0;
        work->field_176         = 1;
        func_actor_204000_8014AC8C(arg1);
        func_actor_204000_8014AC8C(arg1);
        work->field_176              = 0;
        arg1->field_2C->field_8->flg = 0;
        work->field_6                = 0;
        work->field_479              = 1;
        work->field_19A              = 0xA;
        work->field_198              = 0;
        work->field_6                = 0;
        work->field_8                = 0;
        Gfx_MatrixCol0(&arg1->field_2C->field_8->coord, &work->dir);
        VectorNormalSS(&work->dir, &work->dir);
    }
    work->field_6++;
    switch (work->field_174) {
        case 12:
            if (work->field_176 == 0) {
                work->field_19A += 2;
                work->field_198 += work->field_19A;
                h                = work->field_198;
                coord            = arg1->field_2C->field_8;
                y                = coord->coord.t[1];
                if (y >= 0 || (y < 0 ? -y : y) < (s16)h) {
                    coord->coord.t[1] = 0;
                    work->field_176   = 0x10;
                    work->field_6     = 0;
                } else {
                    coord->coord.t[1] = y + (s16)h;
                }
                arg1->field_2C->field_8->flg = 0;
                return;
            }
            func_actor_204000_8014AC8C(arg1);
            if ((s16)work->field_6 < 0xA) {
                Actor204000_StepForward(arg1->field_2C->field_8, 0x23);
            }
            if ((s16)work->field_6 < 4) {
                arg1->field_2C->field_8->coord.t[1] -= 0x67;
            }
            if ((u32)(work->field_6 - 4) < 8) {
                arg1->field_2C->field_8->coord.t[1] -= 0xB;
                arg1->field_2C->field_8->flg         = 0;
                Gfx_RotMatrixX(&arg1->field_2C->field_8->coord, -0x100, 0);
            }
            if ((s16)work->field_6 == 0xC) {
                work->field_170 = 2;
                work->field_174 = 0x10;
                work->field_6   = 0;
                work->field_176 = 0x10;
            }
            break;
        case 16:
            func_actor_204000_8014AC8C(arg1);
            if (!((arg0->field_8 >> 0xC) & 1)) {
                t = work->field_6;
                if (t < 0x14) {
                    break;
                }
                if (t < 0x50) {
                    break;
                }
            }
            work->field_174 = 0x11;
            work->field_170 = 2;
            work->field_6   = 0;
            break;
        case 17:
            func_actor_204000_8014AC8C(arg1);
            if ((u32)(work->field_6 - 0xD) < 0x10) {
                arg1->field_2C->field_8->coord.t[1] += 0xD;
                sv                                   = work->dir;
                gte_lddp(0x14);
                gte_ldsv(&sv);
                __asm__ volatile("nop; nop; .word 0x4B98003D");
                gte_stsv(&sv);
                arg1->field_2C->field_8->coord.t[0] += sv.vx;
                arg1->field_2C->field_8->coord.t[2] += sv.vz;
                Gfx_RotMatrixZ(&arg1->field_2C->field_8->coord, -0x88, 0);
                arg1->field_2C->field_8->flg = 0;
            }
            if (work->field_58 & 1) {
                work->field_0   = 4;
                work->field_479 = 0;
            }
            break;
    }
}

/// Resets the actor when `field_4` is set; otherwise advances the `field_6`
/// timer, stepping the model forward in three speed bands and switching to
/// state 0x11 once it passes 48.
void func_actor_204000_8014F908(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work* work;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        arg1->field_2C->field_C = 0;
        work->obj350.flags     |= 0x8000;
        work->obj388.flags     &= 0x7FFF;
        work->obj3C0.flags     &= 0x7FFF;
        work->obj270.flags     &= 0xBFFF;
        arg0->field_14          = 0;
        work->field_174         = 0xB;
        work->field_170         = 2;
        work->field_178         = 0;
        work->field_176         = 1;
        func_actor_204000_8014AC8C(arg1);
        work->field_176              = 0x10;
        arg1->field_2C->field_8->flg = 0;
        work->field_6                = 0;
        work->field_479              = 1;
        work->field_19A              = 0xA;
        work->field_198              = 0;
        work->field_6                = 0;
        work->field_8                = 0;
        return;
    }
    work->field_6++;
    func_actor_204000_8014AC8C(arg1);
    if (work->field_6 >= 0x13 && work->field_6 < 0x23) {
        Actor204000_StepForward(arg1->field_2C->field_8, 4);
    }
    if (work->field_6 >= 0x23 && work->field_6 < 0x28) {
        Actor204000_StepForward(arg1->field_2C->field_8, 0xC);
    }
    if (work->field_6 >= 0x28 && work->field_6 < 0x31) {
        Actor204000_StepForward(arg1->field_2C->field_8, 0x18);
        arg1->field_2C->field_8->coord.t[1] += 0x28;
    }
    if ((s16)work->field_6 > 0x30) {
        work->field_0 = 0x11;
    }
    arg1->field_2C->field_8->flg = 0;
}

void ActorsShared8015087c(GpEnemy* enemy, Task* task);
void func_actor_204000_801509E8(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_204000_80150AA0(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_204000_8014C710(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_204000_8014CD68(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_204000_8014D5B8(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_204000_8014E14C(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_204000_8014E7E0(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_204000_801508E4(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_204000_8014B4AC(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_204000_80150698(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_204000_801507B4(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_204000_8014BC3C(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_204000_80150B58(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_204000_8014F04C(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_204000_8014F3E8(Actor104000Ctx* arg0, Actor104000* arg1);
void func_actor_204000_8014F908(Actor104000Ctx* arg0, Actor104000* arg1);

const Actor104000StateTable D_actor_204000_8014A014 = {
    {
        (Actor104000StateFn)ActorsShared8015087c,
        func_actor_204000_801509E8,
        func_actor_204000_8014C51C,
        func_actor_204000_80150AA0,
        func_actor_204000_8014C710,
        func_actor_204000_8014CD68,
        func_actor_204000_8014D5B8,
        func_actor_204000_8014E14C,
        func_actor_204000_8014E7E0,
        func_actor_204000_801508E4,
        func_actor_204000_8014B4AC,
        func_actor_204000_80150698,
        func_actor_204000_801507B4,
        func_actor_204000_8014BC3C,
        func_actor_204000_80150B58,
        func_actor_204000_8014EDC4,
        func_actor_204000_8014F04C,
        func_actor_204000_8014F3E8,
        func_actor_204000_8014F908,
    }
};

/// Per-frame tick: tints the model from its position, draws the ground shadow
/// for the current light mode, runs the state handler (flagging a state change
/// in `field_4`), applies pending hits and plays the queued sound.
void func_actor_204000_8014FD2C(GpEnemy* arg0, Actor104000* arg1)
{
    VECTOR                pos;
    SVECTOR               unused; // never written; retail's frame keeps 8 bytes here
    Actor104000StateTable table;
    GsCOORDINATE2         coord;
    Actor104000Work*      work;
    Actor104000MatWords*  mw;
    s32                   snd;
    s32                   pan;
    s32                   id;

    work                         = arg1->field_1C;
    table                        = D_actor_204000_8014A014;
    arg1->field_2C->field_8->flg = 0;
    Gp_UpdateCoord(arg1->field_2C->field_8);
    pos.vx = arg1->field_2C->field_8->workm.t[0];
    pos.vy = arg1->field_2C->field_8->workm.t[1];
    pos.vz = arg1->field_2C->field_8->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    switch (D_801153F4) {
        case 0:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 5 && work->field_0 != 0xD &&
                work->field_0 != 0xF && work->field_0 != 0x10 && work->field_0 != 0x11) {
                arg1->field_2C->field_C = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->field_2C->field_8->workm.t, 0x100, Gp_State1C->field_8);
            }
            if (work->field_0 == 0xF) {
                mw                                            = (Actor104000MatWords*)&coord.coord;
                mw->m00_m01                                   = 0x1000;
                ((Actor104000MatWords*)&coord.coord)->m02_m10 = 0;
                mw->m11_m12                                   = 0x1000;
                ((Actor104000MatWords*)&coord.coord)->m20_m21 = 0;
                mw->m22                                       = 0x1000;
                coord.coord.t[0]                              = arg1->field_2C->field_8->coord.t[0];
                coord.coord.t[1]                              = 0;
                coord.coord.t[2]                              = arg1->field_2C->field_8->coord.t[2];
                coord.sub                                     = &Gfx_ViewCoord;
                coord.flg                                     = 0;
                Gp_UpdateCoord(&coord);
                Gp_DrawEffGroundQuad((VECTOR3*)coord.workm.t, 0x60, Gp_State1C->field_8);
            }
            break;
        case 1:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 0xD && work->field_0 != 5 &&
                work->field_0 != 0xF && work->field_0 != 0x10 && work->field_0 != 0x11) {
                arg1->field_2C->field_C = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->field_2C->field_8->workm.t, 0x180, Gp_State1C->field_8);
            }
            Gp_ClearRec18Occupied(work->rec1B0);
            Gp_ClearRec18Occupied(work->hits);
            Gp_ClearRec18Occupied(&work->rec370);
            return;
        case 2:
            arg1->field_2C->field_C = 0x80;
            Gp_ClearRec18Occupied(work->rec1B0);
            Gp_ClearRec18Occupied(work->hits);
            Gp_ClearRec18Occupied(&work->rec370);
            return;
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    table.fn[work->field_0]((Actor104000Ctx*)arg0, arg1);
    if (work->field_496 == 1) {
        if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0 || arg0->field_40 < 0) {
            if (((GpActorWork*)Game_GetPtrSlot(3))->actor->field_954 == 2) {
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
            }
            work->field_496 = 0;
        }
    }
    if (arg0->field_40 > 0) {
        func_actor_204000_8014DDD4(arg0, arg1);
        if (arg0->field_40 <= 0) {
            work->field_0 = 6;
        }
    }
    Gp_ClearRec18Occupied(work->rec1B0);
    Gp_ClearRec18Occupied(work->hits);
    Gp_ClearRec18Occupied(&work->rec370);
    id = ActorsShared8014adfc(work);
    if (id != 0) {
        snd = id | ((arg0->field_8 >> 12) << 8);
        pan = (s8)Gp_GetObjPan((GpObj38*)arg1->field_2C->field_8);
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)arg1->field_2C->field_8));
    }
    if (Game_Session->field_4D != 0) {
        arg1->field_2C->field_8->flg = 0;
    }
}

/// Task-like caller whose `field_30` counts the frames no candidate was found.
typedef struct Actor204000Task {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  field_30;
} Actor204000Task;

/// Refills the two lead slots: a slot whose actor's `field_40` has run out is
/// cleared, and an empty one takes the pooled actor in state 0xE farthest from
/// the player, moving it to state 0xF.
void func_actor_204000_801501A0(Actor204000Task* arg0)
{
    s32            dist[8];
    SVECTOR        d;
    GsCOORDINATE2* coord;
    MATRIX*        m;
    PlayerStatus*  cfg;
    s32            best;
    s16            i;
    s16            j;
    s16            bi;

    bi = 0;
    for (i = 0; i < 2; i++) {
        if (D_actor_204000_80156530[i] != NULL) {
            if (D_actor_204000_80156530[i]->field_20->field_40 <= 0) {
                D_actor_204000_80156530[i] = NULL;
            }
            if (D_actor_204000_80156530[i] != NULL) {
                continue;
            }
        }
        j   = 0;
        cfg = &Player_Status;
        for (; j < 6; j++) {
            if (D_actor_204000_80156538[j] != NULL && D_actor_204000_80156538[j]->field_1C->field_0 == 0xE) {
                coord    = D_actor_204000_80156538[j]->field_2C->field_8;
                m        = cfg->coordMtx;
                d.vx     = m->t[0] - coord->coord.t[0];
                d.vy     = m->t[1] - coord->coord.t[1];
                d.vz     = m->t[2] - coord->coord.t[2];
                dist[j]  = d.vx * d.vx;
                dist[j] += d.vz * d.vz;
            } else {
                dist[j] = -1;
            }
        }
        best = -1;
        for (j = 0; j < 6; j++) {
            if (best < dist[j]) {
                best = dist[j];
                bi   = j;
            }
        }
        if (best == -1) {
            arg0->field_30++;
            return;
        }
        D_actor_204000_80156538[bi]->field_1C->field_0 = 0xF;
        D_actor_204000_80156530[i]                     = D_actor_204000_80156538[bi];
        D_actor_204000_80156538[bi]                    = NULL;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_204000/actor_204000_2", ActorsShared80135df4Table);
