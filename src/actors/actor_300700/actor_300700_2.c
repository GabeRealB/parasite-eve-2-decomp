#include "common.h"

#include "actors/actor_300700.h"
#include "actors/actor_300700_spawn.h"
#include "actors/actor_300700_spawn2.h"
#include "actors/actors_shared_80135b58.h"

#include "gameplay/1BC.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "psyq/inline_c.h"

#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_300700_80161E24;
extern GpEnemyTaskFuncTable3 D_actor_300700_80161E30;

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_300700_801637E4(Actor300700* arg0);
void func_actor_300700_80164794(Actor300700* arg0);
void func_actor_300700_80163D64(Actor300700* arg0);
void func_actor_300700_80164070(Actor300700* arg0);
void func_actor_300700_801643D0(Actor300700* arg0);
void func_actor_300700_801645F8(Actor300700* arg0);
void func_actor_300700_80164E38(Actor300700* arg0, Actor300700Obj2C* arg1, s32 arg2);
void func_actor_300700_80164F68(Actor300700* arg0);
void func_actor_300700_80165000(Actor300700* arg0);
void func_actor_300700_801650C0(Actor300700* arg0);
void func_actor_300700_801651A0(Actor300700* arg0);
void func_actor_300700_80165230(Actor300700* arg0);
void func_actor_300700_801652F4(Actor300700* arg0);
void func_actor_300700_8016534C(Actor300700* arg0);
void func_actor_300700_8016539C(Actor300700* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void func_800B4114(Actor300700Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

void func_actor_300700_801622B4(Actor300700* arg0);
void func_actor_300700_8016252C(Actor300700* arg0);
void func_actor_300700_801626C0(Actor300700* arg0);
void func_actor_300700_801628C8(Actor300700* arg0);
void func_actor_300700_801633B8(Actor300700* arg0);
void func_actor_300700_80162EFC(Actor300700* arg0);

extern u8  D_801153F4;
extern s8  D_80115408;
extern s32 Gp_LcgState;
/// `D_80073B8C` is the camera-target matrix positions are measured from.
extern MATRIX* D_80073B8C;
/// Per-variant base speed, indexed by `GpAreaPlace::rowIndex`.
extern u16 D_actor_300700_80165B78[];

/// Spawn pair table packed by `Gp_PackPair`, and the enemy record whose
/// `pairTable` points at it and whose `hpMax` seeds the enemy's `field_40`.
extern GpU16Pair  D_actor_300700_80165B64;
extern GpPairSrcE D_actor_300700_80165B68;
/// Pose source handed to `func_800B3F84` as its animation data record.
extern u32 D_actor_300700_80165B94;

/// The second variant's pair of the same kind, and its pose source.
extern GpU16Pair  D_actor_300700_80169328;
extern GpPairSrcE D_actor_300700_8016932C;
extern u32        D_actor_300700_801693B8;

/// Per-`field_F` roll thresholds and the timer tables picked by the second
/// roll, for the two idle transitions of `func_actor_300700_80163D64`.
extern s16 D_actor_300700_8016933C[];
extern u16 D_actor_300700_8016934C[];
extern s16 D_actor_300700_8016936C[];
extern u16 D_actor_300700_8016937C[];
extern s16 D_actor_300700_8016939C[];

void func_actor_300700_801637E4(Actor300700* actor)
{
    Actor300700Ctx*  ctx;
    u32              lastId;
    Actor300700Work* work;
    Actor300700Work* contactWork;
    GpDeltaScratch*  allocated;
    GpDeltaScratch*  oldScratch;
    GpDeltaScratch*  scratch;
    GpDeltaScratch*  normal;
    GsCOORDINATE2*   coord;
    s16              cooldown;
    s16              health;
    s32              cooldownParam;
    s32              wallDx;
    s32              wallDy;
    s32              wallDz;
    s32              result;
    s32              dx;
    s32              dy;
    s32              dz;
    s32              depth;
    s32              push;
    s32              z;
    s32              boundedDepth;
    s8*              effectRec;
    s8*              contactRec;
    u32              id;
    u32              kind;
    u32              hitId;
    u32              effect;
    u32              damage;
    GsCOORDINATE2*   sourceCoord;

    push       = 0;
    lastId     = 0;
    oldScratch = *(GpDeltaScratch**)0x1F8003FC;
    work       = actor->field_1C;
    allocated  = oldScratch - 3;
    SOFT_TOUCH_REG(allocated);
    scratch                       = allocated;
    *(GpDeltaScratch**)0x1F8003FC = scratch;
    coord                         = actor->field_2C->field_8;
    ctx                           = actor->field_20;
    result                        = func_800E0C10((GpRec18*)&work->field_27C[0x20], scratch, 4, NULL);
    USE_REG(oldScratch);
    if (result == 1)
        goto move_delta;
    if (result < 2)
        goto move_done;
    if (result == 2)
        goto move_absolute;
    goto move_done;
move_delta:
    coord->coord.t[0] += oldScratch[-3].vx.h.hi;
    coord->coord.t[1] += scratch->vy.h.hi;
    z                  = coord->coord.t[2] + scratch->vz.h.hi;
    goto move_z;
move_absolute:
    coord->coord.t[0] = work->field_360;
    coord->coord.t[1] = work->field_364;
    z                 = work->field_368;
move_z:
    coord->coord.t[2] = z;
move_done:
    Gp_ClearRec18Occupied(&work->field_27C[0x20]);
    if (work->field_378 != 0) {
        cooldown        = (u16)work->field_378 - 1;
        work->field_378 = cooldown;
        normal          = scratch + 1;
        if ((cooldown << 0x10) <= 0) {
            work->field_378 = 0;
            goto cooldown_done;
        }
    } else {
    cooldown_done:
        normal = scratch + 1;
    }
    USE_REG4(scratch, scratch, scratch, scratch);
    USE_REG4(scratch, scratch, scratch, scratch);
    USE_REG2(scratch, scratch);
    contactWork = work;
contact_loop: {
    USE_REG2(contactWork, contactWork);
    id   = contactWork->field_22C.contacts.recs[0].key;
    kind = id >> 0x10;
    if (kind == 1)
        goto physical_contact;
    if (kind == 0)
        goto next_contact;
    if (kind == 2)
        goto damage_contact;
    if (kind == 3)
        goto physical_contact;
    contactWork = (Actor300700Work*)((u8*)contactWork + 0x18);
    goto contact_test;
damage_contact:
    if (work->field_378 == 0) {
        sourceCoord   = Gp_ActorSlots[(id >> 7) & 1]->extra->coords;
        dx            = sourceCoord->coord.t[0] - coord->coord.t[0];
        scratch->vx.w = dx;
        dy            = sourceCoord->coord.t[1] - coord->coord.t[1];
        scratch->vy.w = dy;
        dz            = sourceCoord->coord.t[2] - coord->coord.t[2];
        scratch->vz.w = dz;
        damage        = Gp_ComputeDamage(contactWork->field_22C.contacts.recs[0].key, SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
        USE_REG(damage);
        if (Gp_RollEnemyChance((GpEnemy*)actor->field_20, contactWork->field_22C.contacts.recs[0].key, 0) != 0) {
            damage *= 4;
            Gp_SpawnEff(0x6009C, actor->field_2C->field_8, 0, NULL);
        }
        func_800DA6E8(&actor->field_20->field_10, (s32)damage, 0);
        func_800E2C78((GpObj40*)actor->field_20, (s32)contactWork->field_22C.contacts.recs[0].key, (s32)damage, 0);
        health        = (u16)ctx->field_40 - damage;
        ctx->field_40 = health;
        if ((health << 0x10) <= 0) {
            work->field_37A = 5;
            work->field_37C = 0;
            actor->field_30 = (s32)kind;
        } else if (work->field_398 == 0) {
            work->field_37A = 4;
            work->field_37C = 0;
        }
        work->field_31A &= 0x7FFF;
        effect           = Gp_GetIdParam0(contactWork->field_22C.contacts.recs[0].key) & 0xFFFF;
        switch (effect) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
                break;
            case 2:
                Gp_SetObjFlag2((GpObj5D*)actor->field_20, contactWork->field_22C.contacts.recs[0].key, 0);
                break;
            case 3:
                Gp_SetObjFlag4((GpObj5C*)actor->field_20, contactWork->field_22C.contacts.recs[0].key, 0);
                break;
            case 1:
            case 9:
                Gp_SetObjFlag1((GpObj4C*)actor->field_20);
                break;
        }
        hitId = contactWork->field_22C.contacts.recs[0].key;
        if (lastId != hitId) {
            lastId = hitId;
            func_800FDB18(Gp_GetIdParam1((s32)hitId) & 0xFFFF, coord, NULL, &work->field_334);
        }
        cooldownParam = Gp_GetIdParam2(contactWork->field_22C.contacts.recs[0].key);
        if (cooldownParam > 0) {
            work->field_378 = cooldownParam;
        }
    }
    goto next_contact;
physical_contact:
    wallDx        = coord->workm.t[0] - contactWork->field_22C.contacts.recs[0].point.vx;
    scratch->vx.w = wallDx;
    wallDy        = coord->workm.t[1] - contactWork->field_22C.contacts.recs[0].point.vy;
    scratch->vy.w = wallDy;
    wallDz        = coord->workm.t[2] - contactWork->field_22C.contacts.recs[0].point.vz;
    scratch->vz.w = wallDz;
    depth         = contactWork->field_22C.contacts.recs[0].depth - SquareRoot0((wallDx * wallDx) + (wallDy * wallDy) + (wallDz * wallDz));
    boundedDepth  = depth;
    if (depth <= 0) {
        boundedDepth = 0;
    }
    SOFT_TOUCH_REG_USE(boundedDepth, depth);
    depth = boundedDepth;
    if (push < depth) {
        push = depth;
        VectorNormal((VECTOR*)scratch, (VECTOR*)normal);
        ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, (VECTOR*)normal, (VECTOR*)(scratch + 2));
    }
next_contact:
    contactWork = (Actor300700Work*)((u8*)contactWork + 0x18);
}
contact_test:
    if ((s32)contactWork < (s32)&work->pad_0[0x48])
        goto contact_loop;
    if (push > 0) {
        coord->coord.t[0] += (s32)(push * scratch[2].vx.w) >> 0xC;
        coord->coord.t[2] += (s32)(push * scratch[2].vz.w) >> 0xC;
    }
    Gp_ClearRec18Occupied((s8*)work->field_22C.contacts.recs);
    effectRec = work->pad_31C;
    if (Gp_FindRec18(effectRec, 0) != 0) {
        work->field_31A &= 0x7FFF;
        Gp_ClearRec18Occupied(effectRec);
    }
    contactRec = work->field_1FC;
    if (Gp_CountRec18Hi(contactRec, 0x10000) != 0) {
        sourceCoord      = Gp_ActorSlots[(u8)work->field_1FC[4] >> 7]->extra->coords;
        work->field_394  = 1;
        work->field_1FA &= 0x7FFF;
        work->field_33C  = sourceCoord;
    }
    Gp_ClearRec18Occupied(contactRec);
    *(GpDeltaScratch**)0x1F8003FC += 3;
}

void func_actor_300700_80163D64(Actor300700* arg0)
{
    Actor300700Work*  work;
    Actor300700Obj2C* obj;
    GsCOORDINATE2*    coord;
    s32               state;
    s32               one;
    s32               rng0;
    s32               rng1;
    s32               rng2;
    s32               rng3;
    s32               rng4;
    s32               rng5;
    s32               rng6;
    s32               timer;
    s32               next;
    s32               flags;
    s32               ang;
    s32               snd;
    s32               pan;

    one   = 1;
    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    state = work->field_37C;
    coord = obj->field_8;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto tail;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto tail;
case0:
    flags           = work->field_1FA;
    work->field_384 = 0;
    work->field_1FA = flags | 0x8000;
    timer           = work->field_38C + 1;
    work->field_38C = timer;
    if ((s16)timer < 0x1E) {
        goto tail;
    }
    rng0        = Gp_LcgState * 5 + 0x71357911;
    Gp_LcgState = rng0;
    if ((s32)(((u32)rng0 >> 16) & 0xF) <
        D_actor_300700_8016933C[arg0->field_20->field_3C->rowIndex]) {
        work->field_37E = 7;
        next            = D_actor_300700_8016934C[((u32)(rng1 = rng0 * 5 + 0x71357911) >> 16) & 0xF];
        Gp_LcgState     = rng1;
        work->field_37C = one;
        work->field_38C = next;
        goto tail;
    }
    rng2        = rng0 * 5 + 0x71357911;
    Gp_LcgState = rng2;
    if ((s32)(((u32)rng2 >> 16) & 0xF) <
        D_actor_300700_8016936C[arg0->field_20->field_3C->rowIndex]) {
        work->field_37E = 2;
        next            = D_actor_300700_8016937C[((u32)(rng3 = rng2 * 5 + 0x71357911) >> 16) & 0xF];
        Gp_LcgState     = rng3;
        work->field_37C = 2;
        work->field_38C = next;
        goto tail;
    }
    work->field_38C = 0;
    goto tail;
case1:
    work->field_384 = 0x14;
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto tail;
    }
    work->field_37E = one;
    work->field_38C = 0;
    work->field_37C = 0;
    goto tail;
case2:
    work->field_384 = 0x32;
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto tail;
    }
    work->field_37E = one;
    work->field_38C = 0;
    work->field_37C = 0;
tail:
    work->field_38E = work->field_38E - 1;
    if ((s16)work->field_38E > 0) {
        goto post;
    }
    work->field_386 = 0x19;
    rng4            = Gp_LcgState * 5 + 0x71357911;
    rng5            = rng4 * 5 + 0x71357911;
    ang             = ((u32)rng5 >> 16) & 0x3FF;
    Gp_LcgState     = rng4;
    work->field_38E = ((u32)rng4 >> 16) & 0x1F;
    Gp_LcgState     = rng5;
    if ((((u32)rng5 >> 16) & 0x400) == 0) {
        ang = -ang;
    }
    work->field_38A = ((u16)work->field_388 + ang) & 0xFFF;
post:
    if (work->field_394 != 0) {
        work->field_37A = 1;
        work->field_394 = 0;
        work->field_37C = 0;
        work->field_37E = 2;
        rng6            = Gp_LcgState * 5 + 0x71357911;
        work->field_38C = (((u32)rng6 >> 16) & 0x1F) + 0x3C;
        snd             = ((arg0->field_20->field_8 >> 12) << 8) | 0x40070003;
        Gp_LcgState     = rng6;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
    }
    func_actor_300700_80165000(arg0);
}

void func_actor_300700_80164070(Actor300700* arg0)
{
    VECTOR*           vec;
    Actor300700Work*  work;
    Actor300700Obj2C* obj;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    target;
    s32               state;
    s32               one;
    s32               dist;
    s32               raw;
    s16               diff;
    s32               adiff;
    s32               ang;
    s32               vel;
    s32               pan;
    s32               snd;

    one   = 1;
    vec   = (VECTOR*)(SCRATCH_SP -= 0x10);
    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    state = work->field_37C;
    coord = obj->field_8;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto pop;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto pop;
case0:
    Gp_ArmStateF0(1);
    if (work->field_33C == 0) {
        work->field_33C = ((Actor300700*)gameGetPtrSlot(3))->field_2C->field_8;
    }
    target          = work->field_33C;
    vec->vx         = target->coord.t[0] - coord->coord.t[0];
    vec->vy         = 0;
    vec->vz         = target->coord.t[2] - coord->coord.t[2];
    work->field_38A = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
    work->field_386 = 0x19;
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto dist;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = one;
    work->field_38C = 0;
dist:
    dist = SquareRoot0(vec->vx * vec->vx + vec->vz * vec->vz);
    if (dist < 0x2BC) {
        raw   = work->field_38A - (u16)work->field_388;
        diff  = raw;
        adiff = diff >= 0 ? diff : -diff;
        if (adiff < 0x800) {
            ang = adiff;
            goto wrap_done;
        }
        if (diff > 0) {
            ang = 0x1000 - raw;
            goto wrap_done;
        }
        ang = raw + 0x1000;
    wrap_done:
        if ((s16)ang < 0x32) {
            work->field_37E = 4;
            work->field_384 = 0;
            work->field_386 = 0;
            work->field_37C = 1;
            goto pop;
        }
        work->field_384 = 0;
        goto pop;
    }
    work->field_384 = 0x32;
    goto pop;
case1:
    if ((s16)work->field_382 == 0x14) {
        work->field_31A |= 0x8000;
    }
    if ((s16)work->field_382 < 0x20) {
        goto pop;
    }
    work->field_37E  = 3;
    work->field_37C  = 2;
    work->field_31A &= 0x7FFF;
    goto pop;
case2:
    vel = 0;
    if ((s16)work->field_382 < 0xB) {
        vel = -0x78;
    }
    work->field_384 = vel;
    if ((s16)work->field_382 < 0x1F) {
        goto pop;
    }
    snd = ((arg0->field_20->field_8 >> 12) << 8) | 0x40070004;
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if ((s32)(((u32)Gp_LcgState >> 16) & 0xF) < D_actor_300700_8016939C[arg0->field_20->field_3C->rowIndex]) {
        work->field_37C = 0;
        work->field_37E = state;
        goto pop;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = one;
    work->field_38C = 0;
    work->field_38E = 0;
    work->field_394 = 0;
pop:
    SCRATCH_SP += 0x10;
}

/// Three-state launcher. State 0 arms the timer from `Gp_LcgState` and stores
/// the direction from the camera target to `field_2C->field_8` into
/// `field_370` with `VectorNormalS`; state 1 pushes the coordinate along that
/// normal while `field_382` is below `0xF`, runs the `field_38C` countdown and
/// hands over to the teardown state 3 (or 2) when it expires; state 2 clears
/// the state machine once `field_382` reaches `0x20`.
void func_actor_300700_801643D0(Actor300700* arg0)
{
    VECTOR            vec;
    Actor300700Work*  work;
    Actor300700Obj2C* obj;
    GsCOORDINATE2*    coord;
    s32               state;
    s32               one;
    s32               rng;
    s32               posX;

    one   = 1;
    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    state = work->field_37C;
    coord = obj->field_8;
    switch (state) {
        case 0:
            work->field_37E = 0xA;
            work->field_380 = one;
            work->field_384 = 0;
            work->field_386 = 0;
            work->field_396 = one;
            work->field_37C = one;
            rng             = Gp_LcgState * 5 + 0x71357911;
            work->field_38C = (((u32)rng >> 16) & 0x1F) + 0xF;
            Gp_LcgState     = rng;
            posX            = coord->coord.t[0];
            vec.vx          = D_80073B8C->t[0] - posX;
            vec.vy          = D_80073B8C->t[1] - coord->coord.t[1];
            vec.vz          = D_80073B8C->t[2] - coord->coord.t[2];
            VectorNormalS(&vec, &work->field_370);
            return;
        case 1:
            if ((s16)work->field_382 < 0xF) {
                coord->coord.t[0] += -(work->field_370.vx * 50) >> 12;
                coord->coord.t[2] += -(work->field_370.vz * 50) >> 12;
            }
            if ((u32)(work->field_382 - 6) < 9) {
                work->field_386 = 0x93;
                work->field_38A = (work->field_38A + 0x5C7) & 0xFFF;
            } else {
                work->field_386 = 0;
            }
            work->field_38C = work->field_38C - 1;
            if ((s16)work->field_38C <= 0) {
                if ((arg0->field_20->field_4C & 2) != 0) {
                    work->field_37E = 8;
                    work->field_37A = 3;
                    work->field_37C = 3;
                    return;
                }
                work->field_37E = 9;
                work->field_37C = 2;
            }
            return;
        case 2:
            if ((s16)work->field_382 >= 0x20) {
                work->field_37A = 0;
                work->field_37C = 0;
                work->field_37E = one;
                work->field_38C = 0;
                work->field_394 = one;
                work->field_396 = 0;
            }
            break;
    }
}

/// Per-frame tick. State 0 arms the timer and latches `field_396`; state 1
/// waits it out; state 2 counts `field_38C` down and moves to the teardown
/// state 3, which waits for `Gp_TickObjFlag2` on the spawn block and then
/// clears the hit flag and resets the state machine.
void func_actor_300700_801645F8(Actor300700* arg0)
{
    Actor300700Ctx*  ctx;
    Actor300700Work* work;
    s16              state;
    s32              rng;
    s32              rng2;
    u16              timer;

    work  = arg0->field_1C;
    state = work->field_37C;
    switch (state) {
        case 0:
            work->field_384 = 0;
            work->field_386 = 0;
            if (work->field_396 == 0) {
                work->field_37C = 1;
                work->field_37E = 6;
            } else {
                work->field_37C = 2;
                rng             = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = rng;
                work->field_38C = ((u32)rng >> 0x10) & 0xF;
            }
            work->field_396 = 1;
            work->field_380 = 1;
            return;
        case 1:
            if ((s16)work->field_382 >= 0x1D) {
                work->field_37C = 2;
                rng2            = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = rng2;
                work->field_38C = ((u32)rng2 >> 0x10) & 0xF;
                return;
            }
            return;
        case 2:
            timer           = work->field_38C - 1;
            work->field_38C = timer;
            if ((timer << 0x10) <= 0) {
                work->field_37E = 8;
                work->field_37C = 3;
                return;
            }
            break;
        case 3:
            if (Gp_TickObjFlag2((GpObj5D*)arg0->field_20) != 0) {
                ctx             = arg0->field_20;
                ctx->field_4C  &= 0xFD;
                work->field_37A = 0;
                work->field_37C = 0;
                work->field_37E = 1;
                work->field_38C = 0;
                work->field_394 = 1;
                work->field_396 = 0;
                work->field_398 = 0;
            }
            break;
    }
}

void func_actor_300700_80164794(Actor300700* arg0)
{
    Actor300700Work*       work;
    GsCOORDINATE2*         coord;
    Actor300700RotScratch* sc;
    s32                    ang;
    u16                    want;
    s16                    diff;
    s32                    adiff;
    s32                    step;
    s32                    cur;
    s32                    next;
    s32                    wrapStep;

    sc    = (Actor300700RotScratch*)(SCRATCH_SP -= 0x18);
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_38A;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_388 = ang;
    if (adiff < 0x800) {
        step = work->field_386;
        if (step >= adiff) {
            work->field_388 = want;
        } else {
            next = work->field_388;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_388 = next;
        }
    } else {
        step = work->field_386;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_388 = work->field_38A;
        goto done;
    turn:
        wrapStep = work->field_386;
        cur      = work->field_388;
        if (diff > 0) {
            work->field_388 = cur - wrapStep;
        } else {
            work->field_388 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_388;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}

void func_actor_300700_801648E4(GpEnemy* arg0, Actor300700* arg1)
{
    Actor300700Work*  work;
    Actor300700Obj2C* obj;
    GsCOORDINATE2*    coord;
    Actor300700Work*  work2;
    GsCOORDINATE2*    c;
    VECTOR            vec;
    s32               state;
    s32               i;
    s16               st;
    s16               phase;
    s16               val;
    s32               snd;
    s32               pan;

    obj   = arg1->field_2C;
    work  = arg1->field_1C;
    state = D_801153F4;
    coord = obj->field_8;
    if (state == 1) {
        goto case1;
    }
    if (state < 2) {
        goto default_body;
    }
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case1:
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg1->field_20, &vec, 0, 0);
    return;
case2:
    obj->field_C = 0x80;
    return;
default_body:
    st = work->field_37C;
    if (st == 1) {
        goto dying;
    }
    if (st >= 2) {
        goto ge2;
    }
    if (st == 0) {
        goto death;
    }
    return;
ge2:
    if (st == 2) {
        goto destroy;
    }
    return;
death:
    work->field_37E = 6;
    work->field_38C = 0;
    work->field_390 = 0x1000;
    work->field_340 = coord->coord;
    arg0->recs      = 0;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj(&((Actor300700Spawn2Work*)work)->obj1);
    Gp_UnlinkObj(&((Actor300700Spawn2Work*)work)->obj2);
    Gp_UnlinkObj(&((Actor300700Spawn2Work*)work)->obj3);
    Gp_UnlinkObj(&((Actor300700Spawn2Work*)work)->obj4);
    Gp_SetLightMode((GpObj4C*)arg0, 1);
    Gp_ReleaseStateF0Add((GpObj20E*)arg1, 7);
    work->field_37C = 1;
    work2           = arg1->field_1C;
    i               = 1;
    if ((s16)work2->field_37E != work2->field_380) {
        work2->field_380 = work2->field_37E;
        work2->field_382 = 0;
        val              = D_actor_300700_801693E4[(s16)work2->field_37E];
        do {
            func_800B4114(work2, i, (s16)work2->field_37E, 0, val);
            i++;
        } while (i < 7);
    } else {
        TOUCH_REG(i);
        work2->field_382 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work2, i);
            i++;
        } while (i < 7);
    }
    c      = arg1->field_2C->field_8;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg1->field_20, &vec, 0, 0);
    snd = ((arg1->field_20->field_8 >> 12) << 8) | 0x40070005;
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
    return;
dying:
    func_actor_300700_8016539C(arg1);
    phase           = work->field_38C + 1;
    work->field_38C = phase;
    if (phase == 10) {
        obj->field_C = 2;
    }
    if ((s16)work->field_38C == 15) {
        Gp_SpawnEff(0x600A5, coord, 1, NULL);
    }
    if ((s16)work->field_38C >= 0x3C) {
        work->field_37C = 2;
    }
    work2 = arg1->field_1C;
    i     = 1;
    if ((s16)work2->field_37E != work2->field_380) {
        work2->field_380 = work2->field_37E;
        work2->field_382 = 0;
        val              = D_actor_300700_801693E4[(s16)work2->field_37E];
        do {
            func_800B4114(work2, i, (s16)work2->field_37E, 0, val);
            i++;
        } while (i < 7);
    } else {
        TOUCH_REG(i);
        work2->field_382 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work2, i);
            i++;
        } while (i < 7);
    }
    c      = arg1->field_2C->field_8;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg1->field_20, &vec, 0, 0);
    return;
destroy:
    Gp_DestroyEnemy(arg0, (Task*)arg1);
    return;
}

void func_actor_300700_80164CE0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_300700_80161E30;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_300700_80164D3C(Actor300700Ctx* arg0, Actor300700* arg1)
{
    GsCOORDINATE2*    coord;
    Actor300700Obj2C* obj;
    Actor300700Work*  work;
    s32               state;
    s32               one;

    obj   = arg1->field_2C;
    state = D_801153F4;
    work  = arg1->field_1C;
    coord = obj->field_8;
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
    obj->field_C   = 0;
    arg0->field_14 = 0;
    goto default_body;
case2:
    obj->field_C   = 0x80;
    arg0->field_14 = one;
    return;
default_body:
    if (arg0->field_4C != 0) {
        func_actor_300700_80164E38(arg1, obj, one);
    }
    func_actor_300700_801637E4(arg1);
    func_actor_300700_80164F68(arg1);
    SOFT_USE_REG(work);
    if (work->field_386 != 0) {
        func_actor_300700_80164794(arg1);
    }
    func_actor_300700_801651A0(arg1);
    func_actor_300700_80165230(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    func_actor_300700_801652F4(arg1);
    func_actor_300700_8016534C(arg1);
}

/// Damage tick: folds the generic hit flags into the work state, applies the
/// pending hit and drops the actor to its death state once the hit points run
/// out.
void func_actor_300700_80164E38(Actor300700* arg0, Actor300700Obj2C* arg1, s32 arg2)
{
    Actor300700Ctx*  ctx;
    Actor300700Work* work;
    s32              tick;
    u16              health;
    u8               flags;

    ctx   = arg0->field_20;
    flags = ctx->field_4C;
    work  = arg0->field_1C;
    if (flags & 1) {
        ctx->field_4C   = flags & 0xFE;
        work->field_37A = 2;
        work->field_37C = 0;
    }
    if ((ctx->field_4C & 2) && ((u32)((u16)work->field_37A - 2) >= 2U)) {
        work->field_37A = 3;
        work->field_37C = 0;
        work->field_398 = 1;
    }
    if (ctx->field_4C & 0xC) {
        tick = Gp_TickObjFlag4((GpObj5C*)ctx);
        if (tick != 0) {
            func_800DA6E8(&ctx->field_10, tick, 0);
            health        = ctx->field_40 - tick;
            ctx->field_40 = health;
            if ((health << 0x10) <= 0) {
                work->field_37A = 5;
                work->field_37C = 0;
                arg0->field_30  = 2;
            } else {
                work->field_37A = 4;
                work->field_37C = 0;
            }
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)ctx) != 0) {
            ctx->field_4C &= 0xF3;
        }
    }
}

void func_actor_300700_80164F68(Actor300700* arg0)
{
    switch (arg0->field_1C->field_37A) {
        case 0:
            func_actor_300700_80163D64(arg0);
            break;
        case 1:
            func_actor_300700_80164070(arg0);
            break;
        case 2:
            func_actor_300700_801643D0(arg0);
            break;
        case 3:
            func_actor_300700_801645F8(arg0);
            break;
        case 4:
            func_actor_300700_801650C0(arg0);
            break;
        case 5:
            break;
    }
}

/// Randomised footstep timer. Each tick decrements the counter and, when it
/// runs out, reseeds it from the shared LCG and plays the step sound at the
/// object's pan and depth.
void func_actor_300700_80165000(Actor300700* arg0)
{
    Actor300700Work* work;
    GsCOORDINATE2*   coord;
    s32              snd;
    s32              pan;
    u16              timer;
    u32              random;

    work            = arg0->field_1C;
    coord           = arg0->field_2C->field_8;
    timer           = work->field_392 - 1;
    work->field_392 = timer;
    if ((s16)timer <= 0) {
        random          = (Gp_LcgState * 5) + 0x71357911;
        work->field_392 = (u16)(((random >> 0x10) & 0x7F) + 0x96);
        Gp_LcgState     = (s32)random;
        snd             = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40070001;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(snd, (s32)pan, (s8)gpGetObjDepth(coord));
    }
}

/// State machine for the actor's contact sound: state 0 arms the timer and
/// plays the hit sound once, state 1 clears the state pair once the
/// countdown reaches 0x18.
void func_actor_300700_801650C0(Actor300700* arg0)
{
    Actor300700Work*  work;
    Actor300700Obj2C* obj;
    GsCOORDINATE2*    coord;
    s32               state;
    s32               snd;
    s32               pan;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    state = work->field_37C;
    coord = obj->field_8;
    if (state == 0) {
        goto case0;
    }
    if (state == 1) {
        goto case1;
    }
    return;
case0:
    work->field_37E = 5;
    work->field_380 = 1;
    work->field_384 = 0;
    work->field_386 = 0;
    work->field_37C = 1;
    snd             = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40070002;
    pan             = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
    return;
case1:
    if ((s16)work->field_382 < 0x18) {
        return;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = state;
    work->field_38C = 0;
    work->field_394 = state;
}

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700_2", func_actor_300700_801651A0);

void func_actor_300700_80165230(Actor300700* arg0)
{
    Actor300700Work* work;
    s32              i;
    s32              value;

    work = arg0->field_1C;
    i    = 1;
    if ((s16)work->field_37E != work->field_380) {
        work->field_380 = work->field_37E;
        work->field_382 = 0;
        value           = D_actor_300700_801693E4[(s16)work->field_37E];
        for (; i < 7; i++) {
            func_800B4114(work, i, (s16)work->field_37E, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_382 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 7);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700_2", func_actor_300700_801652F4);

void func_actor_300700_8016534C(Actor300700* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR3        vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x1C0, 0x80);
}

void func_actor_300700_8016539C(Actor300700* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor300700Work*            work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->field_1C;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = arg0->field_2C->field_8;
    if (work->field_390 >= 0x201) {
        work->field_390 = (u16)work->field_390 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_390;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_340;
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
