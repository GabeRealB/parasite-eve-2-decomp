#include "common.h"
#include <psyq/libgte.h>

#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "actors/actor.h"
#include "actors/actors_shared_8014df20.h"
#include "actors/actor_207200.h"

extern GpPairSrcE D_actor_207200_8014DBBC;
extern u8         D_actor_207200_8014E7B0[];
extern SVECTOR    D_actor_207200_8014E7BC;
extern SVECTOR    D_actor_207200_8014E7C4;

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

void func_actor_207200_8014ACF8(GpEnemy* arg0, Task* arg1);
void func_actor_207200_8014A1C4(Task* arg0);
void func_actor_207200_8014AE08(Task* arg0);
void func_actor_207200_8014AE70(Task* task);
void func_actor_207200_8014AF2C(Task* arg0);
void func_actor_207200_8014B04C(Task* task);
void func_actor_207200_8014B128(Task* arg0);
void func_actor_207200_8014B21C(Task* task);
void func_actor_207200_8014AFDC(GpEnemy* arg0, Task* task);

/// 0x38-byte block `func_actor_207200_8014A588` takes from `G_SCRATCH_HEAD`:
/// `delta` receives the `func_800E0C10` push-back and is then reused for the
/// offset to the player.
typedef struct Actor207200HitScratch {
    /* 0x00 */ byte           pad_0[0x20];
    /* 0x20 */ GpDeltaScratch delta;
    /* 0x30 */ byte           pad_30[8];
} Actor207200HitScratch;
STATIC_ASSERT_SIZEOF(Actor207200HitScratch, 0x38);

void func_actor_207200_80149E84(GpEnemy* arg0, Task* arg1)
{
    ActorShared8014df20Work* work;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           part;
    u32                      seed;
    GpRec18*                 records1;
    GpRec18*                 records2;
    GpRec18*                 records3;
    s32                      i;

    obj   = arg1->extra;
    coord = obj->coords;
    part  = &coord[1];
    work  = memCalloc(0x2B0U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = (TaskIdMap*)work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_DC;
    obj->colorMtx  = &work->field_BC;
    arg0->field_4  = &coord[1].coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord      = part;
    arg0->node.flags = 0;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->param      = &D_actor_207200_8014DBBC;
    arg0->recs       = work->field_1A4;
    arg0->hp         = D_actor_207200_8014DBBC.hpMax;
    func_800B3F84(&work->context, D_actor_207200_8014E7B0, obj, work->field_8C, work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot(&work->context, i, 1);
        i += 1;
    } while (i < 3);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_28C  = 1;
    work->field_28E  = 1;
    work->field_2A6  = 1;
    work->field_2A4  = 0x12;
    arg0->node.flags = 1;
    obj->flags       = 0x80;
    seed             = Gp_LcgState * 5 + 0x71357911;
    work->field_2A8  = ((seed >> 16) & 0x3F) + 0x64;
    Gp_LcgState      = seed;
    Gp_SetLightMode(arg1->spawnArg2, 2);
    work->field_11C.end0.vz    = 0x1388;
    work->field_11C.end0Radius = 0xFA0;
    work->field_11C.end1Radius = 0x7D0;
    records1                   = work->field_134;
    work->field_11C.recs       = records1;
    work->field_FC.ctx.d4rec   = &work->field_11C;
    work->field_FC.coord       = coord;
    work->field_FC.pos.vx      = 0;
    work->field_FC.pos.vy      = 0;
    work->field_FC.pos.vz      = 0;
    work->field_FC.key         = 0;
    work->field_FC.radius      = 0;
    work->field_FC.flags       = 3;
    Gp_LinkObj(3, &work->field_FC);
    Gp_InitRec18Table(records1, 1, 0);
    work->field_14C.coord    = coord;
    records2                 = work->field_16C;
    work->field_14C.ctx.recs = records2;
    work->field_14C.pos.vx   = 0;
    work->field_14C.pos.vy   = 0;
    work->field_14C.pos.vz   = 0;
    work->field_14C.key      = 0;
    work->field_14C.radius   = 0x7D0;
    work->field_14C.flags    = 1;
    work->field_FC.flags     = work->field_FC.flags | 0x8000;
    Gp_LinkObj(3, &work->field_14C);
    Gp_InitRec18Table(records2, 1, 0);
    records3                 = work->field_1A4;
    work->field_184.coord    = coord;
    work->field_184.ctx.recs = records3;
    work->field_184.pos.vx   = 0;
    work->field_184.pos.vy   = -0xC8;
    work->field_184.pos.vz   = 0;
    work->field_184.key      = 0x3002F;
    work->field_184.radius   = 0xC8;
    work->field_184.flags    = 1;
    work->field_14C.flags    = work->field_14C.flags | 0x8000;
    Gp_LinkObj(2, &work->field_184);
    Gp_InitRec18Table(records3, 4, 0);
    work->field_184.flags = work->field_184.flags | 0xC200;
    work->field_2AC       = arg0->place->mode;
    if (work->field_2AC == 1 && arg1->spawnType == work->field_2AC) {
        obj->tpage++;
        obj->clut++;
        if (obj->buffer != NULL) {
            tmdProcessStream(obj);
            tmdProcessStream(obj);
        }
    }
    arg1->exitCallback = func_actor_207200_8014B21C;
    arg1->state++;
}

/// State-0 tick of the small enemy. A 0x10000-class hit on either of its two
/// single-record tables sets `Gp_StateF0.field_3`, latches `field_2AA` and selects
/// animation 2; if the light blend is fully up, one sound plays, the blend is
/// turned to fall and a new 0x12..0x31 frame wait is rolled. A latched hit
/// plays a second sound, clears the 0x8000 bit of both nodes and arms state
/// 0xF0. Under animation 1 the frame counter reaching `field_2A8` turns the
/// blend down (with the first sound) when it is fully up, or back up after a
/// new 0x64..0xA3 frame wait when it has bottomed out; under animation 2 the
/// second sound repeats every 0x28 frames. `field_2AC` picks between two sets
/// of sound ids.
void func_actor_207200_8014A1C4(Task* arg0)
{
    ActorShared8014df20Work* work;
    GsCOORDINATE2*           obj;
    s32                      snd;
    s16                      mode;
    s32                      id;
    GpEnemy*                 ctx;

    work                   = (ActorShared8014df20Work*)arg0->work;
    *(u8**)G_SCRATCH_HEAD -= 8;
    obj                    = ((TmdObject*)arg0->extra)->coords;
    if (Gp_CountRec18Hi(work->field_16C, 0x10000) != 0 || Gp_CountRec18Hi(work->field_134, 0x10000) != 0) {
        Gp_StateF0.field_3 = 1;
        work->field_2AA    = 1;
        work->field_28C    = 2;
        if (work->field_2A6 != 0 && work->field_2A4 == 0x12) {
            if (work->field_2AC != 0) {
                ctx = arg0->spawnArg2;
                id  = 0x40480007;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            } else {
                ctx = arg0->spawnArg2;
                id  = 0x402E0006;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            }
            work->field_290 = 0;
            work->field_2A6 = 0;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_2A8 = ((Gp_LcgState >> 16) & 0x1F) + 0x12;
        }
    }
    if (work->field_2AA != 0) {
        if (work->field_2AC != 0) {
            ctx = arg0->spawnArg2;
            id  = 0x40480008;
            snd = ((ctx->placeKey >> 12) << 8) | id;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
        } else {
            ctx = arg0->spawnArg2;
            id  = 0x402E0007;
            snd = ((ctx->placeKey >> 12) << 8) | id;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
        }
        work->field_14C.flags &= 0x7FFF;
        work->field_FC.flags  &= 0x7FFF;
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(work->field_16C);
    mode = work->field_28C;
    if (mode == 1) {
        work->field_29A = 1;
        work->field_292 = 0;
        if (work->field_290 > work->field_2A8) {
            if (work->field_2A6 != 0 && work->field_2A4 == 0x12) {
                if (work->field_2AC != 0) {
                    ctx = arg0->spawnArg2;
                    id  = 0x40480007;
                    snd = ((ctx->placeKey >> 12) << 8) | id;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
                } else {
                    ctx = arg0->spawnArg2;
                    id  = 0x402E0006;
                    snd = ((ctx->placeKey >> 12) << 8) | id;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
                }
                work->field_290 = 0;
                work->field_2A6 = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_2A8 = ((Gp_LcgState >> 16) & 0x1F) + 0x12;
            } else if (*(s32*)&work->field_2A4 == 0) {
                work->field_290 = 0;
                work->field_2A6 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_2A8 = ((Gp_LcgState >> 16) & 0x3F) + 0x64;
            }
        }
    } else if (mode == 2) {
        work->field_2AA = 0;
        if (work->field_290 >= 0x28) {
            if (work->field_2AC != 0) {
                ctx = arg0->spawnArg2;
                id  = 0x40480008;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            } else {
                ctx = arg0->spawnArg2;
                id  = 0x402E0007;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            }
            work->field_290 = 0;
        }
    }
    *(u8**)G_SCRATCH_HEAD += 8;
}

/// Per-frame hit handler. Applies the `func_800E0C10` push-back from the four
/// `field_1A4` records to the root coordinate (restoring `field_254` when two
/// records conflict), then walks the records: a kind-1 hit or a kind-2 hit
/// whose distance-scaled damage is nonzero plays the hit sound and sparks and
/// puts the task into its death state after 5 frames; a zero-damage kind-2 hit
/// applies the id's side effect instead.
void func_actor_207200_8014A588(Task* arg0)
{
    ActorShared8014df20Work* work;
    Actor207200HitScratch*   sc;
    Actor207200HitScratch*   head;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    GpEnemy*                 enemy;
    s32                      i;
    s32                      sndHit;
    s32                      sndHit2;
    u32                      damage;
    s32                      snd;

    work                                     = (ActorShared8014df20Work*)arg0->work;
    head                                     = *(Actor207200HitScratch**)G_SCRATCH_HEAD;
    *(Actor207200HitScratch**)G_SCRATCH_HEAD = head - 1;
    sc                                       = head - 1;
    obj                                      = arg0->extra;
    coord                                    = obj->coords;
    enemy                                    = arg0->spawnArg2;

    switch (func_800E0C10(work->field_1A4, &head[-1].delta, 4, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += sc->delta.vx.h.hi;
            coord->coord.t[1] += sc->delta.vy.h.hi;
            coord->coord.t[2] += sc->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_254;
            coord->coord.t[1] = work->field_258;
            coord->coord.t[2] = work->field_25C;
            break;
    }
    i       = 0;
    sndHit  = 0x40480009;
    sndHit2 = 0x402E0008;
    do {
        switch (work->field_1A4[i].key & 0xFFFF0000) {
            case 0x10000:
                if (work->field_2AC != 0) {
                    snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | sndHit;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | sndHit2;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &D_actor_207200_8014E7BC);
                Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &D_actor_207200_8014E7BC);
                Gp_SpawnEff(0x6009E, ((TmdObject*)arg0->extra)->coords, 0, &D_actor_207200_8014E7C4);
                Gp_SpawnPadLerp(0xA, 0x60, 0x60);
                obj->flags          = 0x80;
                work->field_2A0     = 0x500;
                work->field_28C     = 1;
                enemy->hp           = 0;
                work->field_2A6     = 1;
                arg0->killCountdown = 5;
                arg0->state         = 2;
                break;
            case 0x20000:
                sc->delta.vx.w = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                sc->delta.vy.w = Player_Status.coordMtx->t[1] - coord->coord.t[1];
                sc->delta.vz.w = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                damage         = Gp_ComputeDamage(work->field_1A4[i].key,
                                                  SquareRoot0(sc->delta.vx.w * sc->delta.vx.w +
                                                              sc->delta.vy.w * sc->delta.vy.w +
                                                              sc->delta.vz.w * sc->delta.vz.w),
                                                  0, 0);
                if (Gp_RollEnemyChance(arg0->spawnArg2, work->field_1A4[i].key, 0) != 0) {
                    damage *= 4;
                }
                func_800E2C78(enemy, work->field_1A4[i].key, damage, 0);
                func_800DA6E8(&enemy->node, damage, 0);
                if (damage != 0) {
                    if (work->field_2AC != 0) {
                        snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | sndHit;
                        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    } else {
                        snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | sndHit2;
                        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    }
                    Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &D_actor_207200_8014E7BC);
                    Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &D_actor_207200_8014E7BC);
                    Gp_SpawnEff(0x6009E, ((TmdObject*)arg0->extra)->coords, 0, &D_actor_207200_8014E7C4);
                    obj->flags          = 0x80;
                    work->field_2A0     = 0x1000;
                    work->field_2A6     = 1;
                    work->field_28C     = 1;
                    enemy->hp           = 0;
                    arg0->killCountdown = 5;
                    arg0->state         = 2;
                    break;
                }
                switch ((u16)Gp_GetIdParam0(work->field_1A4[i].key)) {
                    case 2:
                    case 9:
                        Gp_SetObjFlag2(enemy, work->field_1A4[i].key, 0);
                        break;
                    case 8:
                        work->field_2A6 = 1;
                        break;
                }
                break;
        }
        i++;
    } while (i < 4);
    Gp_ClearRec18Occupied(work->field_1A4);
    *(Actor207200HitScratch**)G_SCRATCH_HEAD = *(Actor207200HitScratch**)G_SCRATCH_HEAD + 1;
}

/// Dying-state tick of the small enemy, under the shared `Gp_StateF0.field_4` mode
/// byte: 1 does nothing and 2 hides the model. Otherwise the root part's
/// matrix is saved into `field_264` and refolded with the decaying Y scale.
/// Once `field_288` is set the enemy is destroyed after 0x3D frames; before
/// that, the kill countdown running out releases the state-0xF0 reference,
/// sets `field_288` and unlinks the enemy's node and its three objects, and
/// the two animation slots are rebound or advanced.
void func_actor_207200_8014AA74(GpEnemy* arg0, Task* arg1)
{
    Actor207200Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              i;
    Actor207200Work* anim;

    work  = arg1->work;
    obj   = arg1->extra;
    coord = obj->coords;
    switch (Gp_StateF0.field_4) {
        case 0:
            break;
        case 1:
            return;
        case 2:
            obj->flags      |= 0x80;
            arg0->node.flags = 1;
            return;
    }
    if (work->field_288 != 0) {
        work->field_264 = coord->coord;
        func_actor_207200_8014B128(arg1);
        work->field_28A++;
        if (work->field_28A >= 0x3D) {
            Gp_DestroyEnemy(arg0, arg1);
        }
        return;
    }
    work->field_264 = coord->coord;
    func_actor_207200_8014B128(arg1);
    arg1->killCountdown--;
    if (arg1->killCountdown <= 0) {
        Gp_ReleaseStateF0Add(arg1, 0x2F);
        work->field_288 = 1;
        work->field_28A = 0;
        arg0->recs      = 0;
        Gp_UnlinkNode(&arg0->node);
        Gp_UnlinkObj(&work->field_14C.obj);
        Gp_UnlinkObj(&work->field_FC.obj);
        Gp_UnlinkObj(&work->field_184.obj);
    }
    anim = arg1->work;
    i    = 1;
    if (anim->field_28C != (s16)anim->field_28E) {
        anim->field_28E = anim->field_28C;
        anim->field_290 = 0;
        do {
            func_800B4114((GpAnimCtx*)anim, i, anim->field_28C, 0, 8);
            i++;
        } while (i < 3);
        return;
    }
    TOUCH_REG(i);
    anim->field_290 = (u16)(anim->field_290 + i);
    do {
        Gp_AnimTickIndex((GpAnimCtx*)anim, i);
        i++;
    } while (i < 3);
}

/// The small enemy's state handlers - spawn, live tick and dying tick - which
/// `func_actor_207200_8014AC9C` dispatches through by task state.
const GpEnemyTaskFuncTable3 D_actor_207200_80149E24 = {
    { func_actor_207200_80149E84, func_actor_207200_8014ACF8, func_actor_207200_8014AA74 }
};

void func_actor_207200_8014AC9C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_207200_80149E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Global mode byte in the main executable shared by the enemy actors: 1 runs
/// only the tail below, 2 puts the model in its hidden pose, 0 clears the
/// node flag before falling into the update, and any other value updates
/// directly.
///
/// The update raises the root coordinate's Y translation by 0x80 and ticks the
/// five model helpers, clears the display flags of the first two parts and
/// recomputes the second part's world matrix; the tail then colours the actor
/// from that part.
void func_actor_207200_8014ACF8(GpEnemy* arg0, Task* arg1)
{
    s32 state;
    s32 one;

    state = Gp_StateF0.field_4;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    arg0->node.flags = 0;
    goto default_body;
case2:
    ((TmdObject*)arg1->extra)->flags = 0x80;
    arg0->node.flags                 = one;
    return;
default_body:
    ((TmdObject*)arg1->extra)->coords[0].coord.t[1] += 0x80;
    func_actor_207200_8014AE70(arg1);
    func_actor_207200_8014B04C(arg1);
    func_actor_207200_8014AE08(arg1);
    func_actor_207200_8014A588(arg1);
    func_actor_207200_8014AF2C(arg1);
    ((TmdObject*)arg1->extra)->coords[0].flg = 0;
    ((TmdObject*)arg1->extra)->coords[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->coords[1]);
case1:
    func_actor_207200_8014AFDC(arg0, arg1);
}

/// Consumes the pending bits of the enemy's `reactionFlags`: bit 0x1 is
/// dropped on its own, bit 0x2 puts the work into reaction state 3 with its
/// frame counter cleared, and bits 0xC are dropped last, after re-reading the
/// byte.
void func_actor_207200_8014AE08(Task* arg0)
{
    GpEnemy*         enemy;
    Actor207200Work* work;
    u8               flags;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = arg0->work;
    flags = enemy->reactionFlags;
    if (flags != 0) {
        if (flags & 1) {
            enemy->reactionFlags = flags & 0xFE;
        }
        if (enemy->reactionFlags & 2) {
            enemy->reactionFlags = enemy->reactionFlags & 0xFD;
            work->field_286      = 3;
            work->field_28A      = 0;
        }
        flags = enemy->reactionFlags;
        if (flags & 0xC) {
            enemy->reactionFlags = flags & 0xF3;
        }
    }
}

/// Per-frame dispatch on the small enemy's reaction state `field_286`: state
/// 0 runs the idle tick and state 2 does nothing. State 3 clears `field_292`
/// and turns the light blend down, resets the remembered animation id to 1
/// and the counters every fourth frame, and returns to state 0 once
/// `Gp_TickObjFlag2` reports the reaction over.
void func_actor_207200_8014AE70(Task* task)
{
    Actor207200Work* work;

    work = task->work;
    switch (work->field_286) {
        case 0:
            func_actor_207200_8014A1C4(task);
            break;
        case 2:
            break;
        case 3:
            work->field_292 = 0;
            work->field_2A6 = 0;
            work->field_28A = work->field_28A + 1;
            if (work->field_28A >= 4) {
                work->field_28E = 1;
                work->field_290 = 0;
                work->field_28A = 0;
            }
            if (Gp_TickObjFlag2(task->spawnArg2) != 0) {
                work->field_286 = 0;
            }
            break;
    }
}

/// Drives animation slots 1 and 2 from the work's animation id `field_28C`.
/// When it differs from the remembered `field_28E` it is remembered, the
/// frame counter restarts and both slots switch to it with a blend of 8;
/// otherwise the counter ticks and both slots advance.
void func_actor_207200_8014AF2C(Task* arg0)
{
    Actor207200Work* work;
    s32              i;

    work = arg0->work;
    i    = 1;
    if (work->field_28C != (s16)work->field_28E) {
        work->field_28E = work->field_28C;
        work->field_290 = 0;
        do {
            func_800B4114((GpAnimCtx*)work, i, work->field_28C, 0, 8);
            i++;
        } while (i < 3);
        return;
    }
    TOUCH_REG(i);
    work->field_290 = (u16)(work->field_290 + i);
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
        i++;
    } while (i < 3);
}

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters. `arg0` is the colour target, passed straight through.
void func_actor_207200_8014AFDC(GpEnemy* arg0, Task* task)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)task->extra)->coords[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(arg0, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// Ramps the small enemy's light blend up or down depending on the flag at
/// `field_2A6`. Rising, the first frame switches the display object to light
/// mode 2 and the counter saturates at 0x12, where it sets the enemy node's
/// flag and the model's 0x80 bit. Falling, leaving 0x12 clears the node flag
/// and returns the object to light mode 0, and the counter bottoms out at 0
/// with the model bits cleared.
void func_actor_207200_8014B04C(Task* task)
{
    ActorShared8014df20Work* work;
    GpEnemy*                 enemy;
    TmdObject*               obj;

    work  = (ActorShared8014df20Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    obj   = (TmdObject*)task->extra;

    if (work->field_2A6 != 0) {
        if (work->field_2A4 == 0) {
            work->field_2A4++;
            obj->flags = 2;
            Gp_SetLightMode(task->spawnArg2, 2);
        } else {
            work->field_2A4++;
            if (work->field_2A4 >= 0x12) {
                work->field_2A4   = 0x12;
                enemy->node.flags = 1;
                obj->flags        = 0x80;
            }
        }
    } else {
        if (work->field_2A4 == 0x12) {
            work->field_2A4--;
            enemy->node.flags = 0;
            obj->flags        = 2;
            Gp_SetLightMode(task->spawnArg2, 0);
        } else {
            work->field_2A4--;
            if (work->field_2A4 <= 0) {
                work->field_2A4 = 0;
                obj->flags      = 0;
            }
        }
    }
}

/// Rebuilds the model's root coordinate from the matrix saved in
/// `field_264`, scaled along Y by `field_2A0`, which decays by 0x50 a frame
/// while above 0x200. The scaling matrix and its vector are staged in 0x30
/// bytes of the scratch stack; the node's `flg` is cleared so the next
/// `Gp_UpdateCoord` recomputes it.
void func_actor_207200_8014B128(Task* arg0)
{
    GsCOORDINATE2*     coord;
    MATRIX*            head;
    ActorScaleScratch* scratch;
    Actor207200Work*   work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->work;
    scratch             = (ActorScaleScratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = (*(TmdObject**)&arg0->extra)->coords;
    if (work->field_2A0 >= 0x201) {
        work->field_2A0 = (u16)work->field_2A0 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_2A0;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_264;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg         = 0;
    *(u8**)0x1F8003FC += 0x30;
}

/// Exit callback of the small enemy: detaches the enemy's hit records,
/// unlinks its node and the work's three objects, then runs the common enemy
/// exit.
void func_actor_207200_8014B21C(Task* task)
{
    ActorShared8014df20Work* work;
    GpEnemy*                 enemy;

    enemy = task->spawnArg2;
    work  = (ActorShared8014df20Work*)task->work;

    enemy->recs = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_14C);
    Gp_UnlinkObj(&work->field_FC);
    Gp_UnlinkObj(&work->field_184);
    Gp_EnemyTaskExit(task);
}
