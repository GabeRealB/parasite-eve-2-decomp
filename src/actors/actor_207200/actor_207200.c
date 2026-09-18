#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/sound.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "actors/actors_shared_80135d50.h"
#include "actors/actors_shared_8014ae08.h"
#include "actors/actors_shared_8014af2c.h"
#include "actors/actors_shared_8014df20.h"
#include "actors/actor_207200.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_207200_80149E24;

extern u8 D_801153F4;

extern GpPairSrcE D_actor_207200_8014DBBC;
extern u8         D_actor_207200_8014E7B0[];
extern u32        Gp_LcgState;

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
    work  = Mem_Calloc(0x2B0U, false);
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
    arg0->field_18     = part;
    arg0->node.field_4 = 0;
    arg0->field_1C.vx  = 0;
    arg0->field_1C.vy  = 0;
    arg0->field_1C.vz  = 0;
    arg0->field_50     = &D_actor_207200_8014DBBC;
    arg0->field_54     = (s32)work->field_1A4;
    arg0->field_40     = D_actor_207200_8014DBBC.field_4;
    func_800B3F84(&work->context, D_actor_207200_8014E7B0, (GpAnimObj*)obj, work->field_8C, work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot(&work->context, i, 1);
        i += 1;
    } while (i < 3);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_28C    = 1;
    work->field_28E    = 1;
    work->field_2A6    = 1;
    work->field_2A4    = 0x12;
    arg0->node.field_4 = 1;
    obj->flags         = 0x80;
    seed               = Gp_LcgState * 5 + 0x71357911;
    work->field_2A8    = ((seed >> 16) & 0x3F) + 0x64;
    Gp_LcgState        = seed;
    Gp_SetLightMode((GpObj4C*)arg1->spawnArg2, 2);
    work->field_11C.field_4  = 0x1388;
    work->field_11C.field_10 = 0xFA0;
    work->field_11C.field_12 = 0x7D0;
    records1                 = work->field_134;
    work->field_11C.field_14 = records1;
    work->field_FC.field_C   = (GpRec18*)&work->field_11C;
    work->field_FC.field_8   = coord;
    work->field_FC.field_10  = 0;
    work->field_FC.field_12  = 0;
    work->field_FC.field_14  = 0;
    work->field_FC.field_18  = 0;
    work->field_FC.field_1C  = 0;
    work->field_FC.flags     = 3;
    Gp_LinkObj(3, &work->field_FC);
    Gp_InitRec18Table(records1, 1, 0);
    work->field_14C.field_8  = coord;
    records2                 = work->field_16C;
    work->field_14C.field_C  = records2;
    work->field_14C.field_10 = 0;
    work->field_14C.field_12 = 0;
    work->field_14C.field_14 = 0;
    work->field_14C.field_18 = 0;
    work->field_14C.field_1C = 0x7D0;
    work->field_14C.flags    = 1;
    work->field_FC.flags     = work->field_FC.flags | 0x8000;
    Gp_LinkObj(3, &work->field_14C);
    Gp_InitRec18Table(records2, 1, 0);
    records3                 = work->field_1A4;
    work->field_184.field_8  = coord;
    work->field_184.field_C  = records3;
    work->field_184.field_10 = 0;
    work->field_184.field_12 = -0xC8;
    work->field_184.field_14 = 0;
    work->field_184.field_18 = 0x3002F;
    work->field_184.field_1C = 0xC8;
    work->field_184.flags    = 1;
    work->field_14C.flags    = work->field_14C.flags | 0x8000;
    Gp_LinkObj(2, &work->field_184);
    Gp_InitRec18Table(records3, 4, 0);
    work->field_184.flags = work->field_184.flags | 0xC200;
    work->field_2AC       = ((GpAreaPlace*)arg0->field_3C)->field_2;
    if (work->field_2AC == 1 && arg1->spawnType == work->field_2AC) {
        obj->tpage++;
        obj->clut++;
        if (obj->buffer != NULL) {
            Tmd_ProcessStream(obj);
            Tmd_ProcessStream(obj);
        }
    }
    arg1->exitCallback = ActorsShared8014df20;
    arg1->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_8014A1C4);

extern SVECTOR D_actor_207200_8014E7BC;
extern SVECTOR D_actor_207200_8014E7C4;

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
        switch (work->field_1A4[i].field_4 & 0xFFFF0000) {
            case 0x10000:
                if (work->field_2AC != 0) {
                    snd = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 12) << 8) | sndHit;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
                } else {
                    snd = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 12) << 8) | sndHit2;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
                }
                Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &D_actor_207200_8014E7BC);
                Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &D_actor_207200_8014E7BC);
                Gp_SpawnEff(0x6009E, ((TmdObject*)arg0->extra)->coords, 0, &D_actor_207200_8014E7C4);
                Gp_SpawnPadLerp(0xA, 0x60, 0x60);
                obj->flags          = 0x80;
                work->field_2A0     = 0x500;
                work->field_28C     = 1;
                enemy->field_40     = 0;
                work->field_2A6     = 1;
                arg0->killCountdown = 5;
                arg0->state         = 2;
                break;
            case 0x20000:
                sc->delta.vx.w = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                sc->delta.vy.w = Player_Status.coordMtx->t[1] - coord->coord.t[1];
                sc->delta.vz.w = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                damage         = Gp_ComputeDamage(work->field_1A4[i].field_4,
                                                  SquareRoot0(sc->delta.vx.w * sc->delta.vx.w +
                                                              sc->delta.vy.w * sc->delta.vy.w +
                                                              sc->delta.vz.w * sc->delta.vz.w),
                                                  0, 0);
                if (Gp_RollEnemyChance(arg0->spawnArg2, work->field_1A4[i].field_4, 0) != 0) {
                    damage *= 4;
                }
                func_800E2C78((GpObj40*)enemy, work->field_1A4[i].field_4, damage, 0);
                func_800DA6E8(&enemy->node, damage, 0);
                if (damage != 0) {
                    if (work->field_2AC != 0) {
                        snd = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 12) << 8) | sndHit;
                        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
                    } else {
                        snd = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 12) << 8) | sndHit2;
                        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
                    }
                    Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &D_actor_207200_8014E7BC);
                    Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &D_actor_207200_8014E7BC);
                    Gp_SpawnEff(0x6009E, ((TmdObject*)arg0->extra)->coords, 0, &D_actor_207200_8014E7C4);
                    obj->flags          = 0x80;
                    work->field_2A0     = 0x1000;
                    work->field_2A6     = 1;
                    work->field_28C     = 1;
                    enemy->field_40     = 0;
                    arg0->killCountdown = 5;
                    arg0->state         = 2;
                    break;
                }
                switch ((u16)Gp_GetIdParam0(work->field_1A4[i].field_4)) {
                    case 2:
                    case 9:
                        Gp_SetObjFlag2((GpObj5D*)enemy, work->field_1A4[i].field_4, 0);
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

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void ActorsShared8014b128(Task* arg0, GsCOORDINATE2* arg1);

/// Per-frame body under the shared `D_801153F4` mode byte: 1 does nothing and
/// 2 hides the model. Otherwise the root part's matrix is copied into the work
/// area and the model helper ticks. Once dying (`work->field_288`), the enemy
/// is destroyed after 0x3D frames; before that, the kill countdown expiring
/// releases the actor, starts the dying state and unlinks its node and three
/// objects, and the two animation slots are rebound or advanced.
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
    switch (D_801153F4) {
        case 0:
            break;
        case 1:
            return;
        case 2:
            obj->flags        |= 0x80;
            arg0->node.field_4 = 1;
            return;
    }
    if (work->field_288 != 0) {
        work->field_264 = coord->coord;
        ActorsShared8014b128(arg1, coord);
        work->field_28A++;
        if (work->field_28A >= 0x3D) {
            Gp_DestroyEnemy(arg0, arg1);
        }
        return;
    }
    work->field_264 = coord->coord;
    ActorsShared8014b128(arg1, coord);
    arg1->killCountdown--;
    if (arg1->killCountdown <= 0) {
        Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0x2F);
        work->field_288 = 1;
        work->field_28A = 0;
        arg0->field_54  = 0;
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

INCLUDE_RODATA("actors/nonmatchings/actor_207200/actor_207200", D_actor_207200_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_207200/actor_207200", D_actor_207200_80149E24);

void func_actor_207200_8014AC9C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_207200_80149E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_207200_8014A588(Task* arg0);
void func_actor_207200_8014AE70(Task* arg0);
void func_actor_207200_8014AFDC(void* arg0, Task* arg1);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);

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

    state = D_801153F4;
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
    arg0->node.field_4 = 0;
    goto default_body;
case2:
    ((TmdObject*)arg1->extra)->flags = 0x80;
    arg0->node.field_4               = one;
    return;
default_body:
    ((TmdObject*)arg1->extra)->coords[0].coord.t[1] += 0x80;
    func_actor_207200_8014AE70(arg1);
    ActorsShared80135d50(arg1);
    ActorsShared8014ae08(arg1);
    func_actor_207200_8014A588(arg1);
    ActorsShared8014af2c(arg1);
    ((TmdObject*)arg1->extra)->coords[0].flg = 0;
    ((TmdObject*)arg1->extra)->coords[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->coords[1]);
case1:
    func_actor_207200_8014AFDC(arg0, arg1);
}

INCLUDE_RODATA("actors/nonmatchings/actor_207200/actor_207200", D_actor_207200_80149E30);
