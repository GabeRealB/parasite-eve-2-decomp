#include "common.h"

#include "actors/actor_105500.h"
#include "actors/actors_shared_80135b58.h"
#include "actors/actors_shared_80135c4c.h"

#include "main/mem.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "psyq/inline_c.h"

extern void* D_80067704[1];
extern u8    Actor05500_D05F18[];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

void Actor05500_Fn0006C(Actor105500* arg0);
void Actor05500_Fn00754(Actor105500* arg0);
void Actor05500_Fn00914(Actor105500* arg0);
void Actor05500_Fn00A94(Actor105500* arg0);
void Actor05500_Fn00FA0(Actor105500* arg0);
void Actor05500_Fn012E8(Actor105500* arg0);
void Actor05500_Fn0143C(Actor105500* arg0);
void Actor05500_Fn01A0C(Actor105500* arg0);
void Actor05500_Fn01B30(Actor105500* arg0);
void Actor05500_Fn020D4(Actor105500* arg0);
void Actor05500_Fn02214(Actor105500* arg0);
void Actor05500_Fn02954(Actor105500* arg0, s32 arg1);
void Actor05500_Fn03674(Actor105500* arg0, Actor105500Obj2C* arg1, s32 arg2);
void Actor05500_Fn0378C(Actor105500* arg0);
void Actor05500_Fn03864(Actor105500* arg0);
void Actor05500_Fn03918(Actor105500* arg0);
void Actor05500_Fn039AC(Actor105500* arg0);
void Actor05500_Fn03A70(Actor105500* arg0);
void Actor05500_Fn03AC8(Actor105500* arg0);
void Actor05500_Fn03B60(Actor105500* arg0);
void Actor05500_Fn03C54(Actor105500* arg0);
void Actor05500_Fn03D40(Actor105500* arg0);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern u8 D_801153F4;

void Actor05500_Fn0006C(Actor105500* arg0)
{
    u32              lastId;
    GpDeltaScratch*  normal;
    Actor105500Work* work;
    s32              i;

    Actor105500HitScratch* hit;
    Actor105500HitScratch* scratch;
    Actor105500HitScratch* allocated;
    s32                    amount;
    GsCOORDINATE2*         coord;
    s32                    id;
    s32                    wallDz;
    s32                    dx;
    s32                    dy;
    s32                    dz;
    s32                    wallDx;
    s32                    wallDy;
    s32                    hitId;

    s32 mask;
    s32 push;
    s32 result;
    s32 boundedDepth;
    s32 value;
    s32 one;
    s16 cooldown;

    u32             kind;
    u32             effect;
    u32             damage;
    s32             flag;
    GsCOORDINATE2*  sourceCoord;
    Actor105500Ctx* ctx;

    push      = 0;
    lastId    = 0;
    work      = arg0->field_1C;
    damage    = *(u32*)0x1F8003FC;
    allocated = (Actor105500HitScratch*)(damage - 0x38);
    SOFT_TOUCH_REG(allocated);
    scratch                       = allocated;
    coord                         = arg0->field_2C->field_8;
    ctx                           = arg0->field_20;
    *(GpDeltaScratch**)0x1F8003FC = (GpDeltaScratch*)scratch;
    work->field_3CC               = 0;
    result                        = func_800E0C10(work->field_234, &scratch->delta, 4, NULL);
    if (result != 0) {
        if (work->field_39A == 2) {
            work->field_3CC = 1;
        }
        switch (result) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += *(s16*)(damage - 0x36);
                coord->coord.t[1] += scratch->delta.vy.h.hi;
                coord->coord.t[2] += scratch->delta.vz.h.hi;
                break;
            case 2:
                coord->coord.t[0] = work->field_35C.vx;
                coord->coord.t[1] = work->field_35C.vy;
                coord->coord.t[2] = work->field_35C.vz;
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_234);
    if (work->field_390 != 0) {
        cooldown        = (u16)work->field_390 - 1;
        work->field_390 = cooldown;
        if ((cooldown << 0x10) <= 0) {
            work->field_390 = 0;
        }
    }
    one = 1;
    hit = scratch;
    __asm__ volatile("" : "+r"(result) : "r"(hit));
    normal          = (GpDeltaScratch*)&hit->normal;
    work->field_3D0 = 0;
    work->field_3BA = 0;
    for (i = 0; i < 2; i++) {
        id   = work->field_2B4[i].key;
        kind = (u32)id >> 0x10;
        if (kind == one)
            goto physical;
        if (kind == 0)
            goto block_59;
        if (kind == 2)
            goto damage;
        if (kind == 3)
            goto physical;
        goto block_59;
    damage: {
        if (work->field_390 == 0) {
            result = 0;
            if ((((u32)id >> 8) & 0x3F) == 0x24) {
                if ((id & 0x3F) == 0x24)
                    result = 1;
            }
            if ((result != one) || (work->field_3B2 == 0)) {
                sourceCoord     = ((Actor105500*)Gp_ActorSlots[((u32)id >> 7) & 1])->field_2C->field_8;
                dx              = sourceCoord->coord.t[0] - coord->coord.t[0];
                hit->delta.vx.w = dx;
                dy              = sourceCoord->coord.t[1] - coord->coord.t[1];
                hit->delta.vy.w = dy;
                dz              = sourceCoord->coord.t[2] - coord->coord.t[2];
                hit->delta.vz.w = dz;
                value           = SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
                TOUCH_REG(value);
                amount = Gp_ComputeDamage((u32)work->field_2B4[i].key, value, 0, 0);
                damage = amount;
                if (result == 0) {
                    if (work->field_3CA != 0) {
                        damage = (u32)(amount << 0x10) >> 0xF;
                        Gp_SpawnEff(0x6009C, arg0->field_2C->field_8 + 1, 3, NULL);
                    }
                    if (Gp_RollEnemyChance((GpEnemy*)ctx, (u32)work->field_2B4[i].key, 0) != 0) {
                        damage = (u32)(damage << 0x10) >> 0xE;
                        if (work->field_3CA == 0) {
                            Gp_SpawnEff(0x6009C, arg0->field_2C->field_8 + 1, 0, NULL);
                        }
                    }
                    func_800E2C78((GpObj40*)ctx, work->field_2B4[i].key, (s32)(s16)damage, 0);
                }
                func_800DA6E8(&ctx->field_10, (s32)(s16)damage, 0);
                value         = (u16)ctx->field_40 - damage;
                ctx->field_40 = value;
                if (work->field_3C8 != one) {
                    if ((s16)value <= 0) {
                        work->field_39A = 9;
                        work->field_39C = 0;
                        arg0->field_30  = 2;
                    } else if (result == 0) {
                        work->field_39A = 6;
                        work->field_39C = 0;
                    }
                }
                if (work->field_3C8 == 2) {
                    if ((work->field_39A == 9) || (result == 0)) {
                        work->field_3C8 = 0;
                        work->field_3A2 = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
                        hit->rot.vx     = 0;
                        hit->rot.vy     = (s16)((u16)work->field_3A2 + 0x800);
                        hit->rot.vz     = 0;
                        RotMatrix(&hit->rot, &coord->coord);
                        goto block_41;
                    }
                } else {
                block_41:
                    if (result == 0) {
                        work->field_3D0        = one;
                        work->field_2E4.flags &= 0x3FFF;
                    }
                }
                effect = Gp_GetIdParam0(work->field_2B4[i].key) & 0xFFFF;
                switch (effect) {
                    case 0:
                    case 1:
                    case 5:
                    case 8:
                    case 9:
                        break;
                    case 2:
                        Gp_SetObjFlag2((GpObj5D*)ctx, work->field_2B4[i].key, 0);
                        break;
                    case 3:
                        Gp_SetObjFlag4((GpObj5C*)ctx, work->field_2B4[i].key, 0);
                        break;
                    case 4:
                    case 6:
                        if (work->field_3C8 != one) {
                            work->field_3BA = one;
                        }
                        break;
                    case 7:
                        if (work->field_3B0 == 0) {
                            work->field_3B0        = one;
                            work->field_3BE        = 0;
                            work->field_3B2        = 0;
                            work->field_31C.flags |= 0x8000;
                            Gp_SetLightMode((GpObj4C*)arg0->field_20, 3);
                        }
                        break;
                }
                hitId = work->field_2B4[i].key;
                if (lastId != hitId) {
                    hit->rot.vx = 0;
                    hit->rot.vy = -0xC8;
                    hit->rot.vz = 0;
                    lastId      = hitId;
                    func_800FDB18(Gp_GetIdParam1(work->field_2B4[i].key) & 0xFFFF, arg0->field_2C->field_8 + 1, &hit->rot, &work->field_354);
                }
                result = Gp_GetIdParam2(work->field_2B4[i].key);
                if (result > 0) {
                    work->field_390 = (s16)result;
                }
            }
        }
    }
        goto block_59;
    physical: {
        wallDx          = coord->workm.t[0] - work->field_2B4[i].point.vx;
        hit->delta.vx.w = wallDx;
        wallDy          = coord->workm.t[1] - work->field_2B4[i].point.vy;
        hit->delta.vy.w = wallDy;
        wallDz          = coord->workm.t[2] - work->field_2B4[i].point.vz;
        hit->delta.vz.w = wallDz;
        amount          = work->field_2B4[i].depth - SquareRoot0((wallDx * wallDx) + (wallDy * wallDy) + (wallDz * wallDz));
        boundedDepth    = amount;
        if (amount <= 0) {
            boundedDepth = 0;
        }
        TOUCH_REG_USE(boundedDepth, amount);
        amount = boundedDepth;
        if (push < amount) {
            push = amount;
            VectorNormal((VECTOR*)hit, (VECTOR*)normal);
            ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, (VECTOR*)normal, &hit->local);
        }
    }
    block_59:;
    }
    if (push > 0) {
        flag               = 1;
        coord->coord.t[0] += (s32)(push * hit->local.vx) >> 0xC;
        coord->coord.t[2] += (s32)(push * hit->local.vz) >> 0xC;
    } else {
        flag = 1;
    }
    Gp_ClearRec18Occupied(work->field_2B4);
    work->field_3CE = 0;
    if (work->field_304[0].flags & 1) {
        mask = work->field_304[0].key & 0xFFFF0000;
        if ((mask == 0x10000) || ((mask == 0x100000) && (work->field_304[0].at10.normal.vy == 0))) {
            work->field_3CE = flag;
        }
        work->field_2E4.flags &= 0x3FFF;
        Gp_ClearRec18Occupied(work->field_304);
    }
    *(u8**)0x1F8003FC += 0x38;
}
INCLUDE_RODATA("actors/nonmatchings/lib/actor_105500_text", Actor05500_D0002C);
INCLUDE_RODATA("actors/nonmatchings/lib/actor_105500_text", Actor05500_D00038);

void Actor05500_Fn00754(Actor105500* arg0)
{
    Actor105500Work* work;
    GsCOORDINATE2*   coord;
    s32              state;
    s32              dx;
    s32              dz;
    u32              random;
    s32              index;
    VECTOR*          delta;
    VECTOR*          scratchEnd;

    scratchEnd                         = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC);
    delta                              = scratchEnd - 1;
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = delta;
    work                               = arg0->field_1C;
    state                              = work->field_39C;
    coord                              = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            scratchEnd[-1].vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
            delta->vy         = 0;
            dz                = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            delta->vz         = dz;
            dx                = scratchEnd[-1].vx;
            if ((SquareRoot0((dx * dx) + (dz * dz)) < 0x7D0) || (work->field_3D0 != 0) || (D_801153F2[1] == 2)) {
                work->field_39C = 1;
                work->field_392 = 0xD;
                Gp_ArmStateF0(1);
            }
            break;
        case 1:
            if ((u32)(work->field_396 - 0xB) < 0x32U) {
                coord->coord.t[2] += 4;
            }
            if ((s16)work->field_396 >= 0x4B) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = state;
                index           = ((Actor105500Ctx*)arg0->field_20)->field_3C->rowIndex;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_39E = Actor05500_D08980[index] + ((random >> 0x10) & 0xF);
            }
            break;
    }
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) + 1;
}

void Actor05500_Fn00914(Actor105500* arg0)
{
    Actor105500Work* work;
    GsCOORDINATE2*   coord;
    s16              angle;
    s32              magnitude;
    s16              wrapped;
    s16              difference;
    s32              distance;
    s32              dx;
    s32              dz;
    VECTOR*          delta;
    VECTOR*          scratchEnd;

    scratchEnd                         = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC);
    coord                              = arg0->field_2C->field_8;
    delta                              = scratchEnd - 1;
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = delta;
    work                               = arg0->field_1C;
    work->field_3A2                    = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
    scratchEnd[-1].vx                  = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
    delta->vy                          = 0;
    dz                                 = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    delta->vz                          = dz;
    dx                                 = scratchEnd[-1].vx;
    distance                           = SquareRoot0((dx * dx) + (dz * dz));
    angle                              = (u16)work->field_3A2 - (ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)delta->vz) & 0xFFF);
    magnitude                          = __builtin_abs((s32)angle);
    if (magnitude < 0x800) {
        difference = magnitude;
    } else {
        if (angle > 0) {
            wrapped = 0x1000 - angle;
        } else {
            wrapped = angle + 0x1000;
        }
        difference = wrapped;
    }
    if ((distance < 0x8FC) && (difference < 0x80)) {
        work->field_39A = 5;
        work->field_39C = 0;
        work->field_392 = 4;
        Gp_ArmStateF0(1);
    }
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) + 1;
}

void Actor05500_Fn00A94(Actor105500* arg0)
{
    register Actor105500* actor asm("s5") = arg0;
    Actor105500Work*      work;
    GsCOORDINATE2*        coord;
    s32                   state;
    s32                   pan0;
    s32                   pan1;
    s32                   pan2;
    s32                   sessionFlags;
    s32                   dx;
    s32                   dz;
    s32                   value;
    u32                   random;
    s32                   index;
    VECTOR*               delta;
    VECTOR*               scratchEnd;

    SOFT_TOUCH_REG(actor);
    scratchEnd                         = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC);
    delta                              = scratchEnd - 1;
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = delta;
    work                               = actor->field_1C;
    coord                              = actor->field_2C->field_8;
    state                              = work->field_39C;
    sessionFlags                       = *(s32*)&gGameSession->at4.loc.view;
    value                              = 0;
    switch (state) {
        case 0:
            if (work->field_3C6 == 0) {
                scratchEnd[-1].vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
                delta->vy         = 0;
                dz                = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                delta->vz         = dz;
                dx                = scratchEnd[-1].vx;
                if (SquareRoot0((dx * dx) + (dz * dz)) < 0x5DC) {
                    value = 1;
                }
            }
            if ((value != 0) || ((s8)Gp_StateF0.field_22 != 0) || (Gp_StateF0.field_8 != 0)) {
                if (work->field_3C6 == 0) {
                    D_80115412 = 1;
                }
                Gp_ArmStateF0(1);
                work->field_39C        = 1;
                work->field_392        = 7;
                work->field_3A8        = Actor05500_D089A0[((Actor105500Ctx*)actor->field_20)->field_3C->rowIndex];
                work->field_2E4.coord  = coord;
                work->field_2E4.radius = 0x12C;
                work->field_2E4.pos.vy = -0x12C;
                work->field_2E4.key    = Gp_PackPair(&Actor05500_D08958, 5);
                work->field_2E4.flags |= 0x8000;
                if ((sessionFlags & 0xFFFF0000) == 0x05200000) {
                    value = (((u16)((Actor105500Ctx*)actor->field_20)->field_8 >> 0xC) << 8) | 0x55200006;
                    pan0  = (s8)Gp_GetObjPan((GpObj38*)coord);
                    SndEvt_EnqueueType6(value, (s32)pan0, (s8)Gp_GetObjDepth((GpObj38*)coord));
                }
            } else if (work->field_3D0 != 0) {
                if (work->field_3C6 == 0) {
                    Gp_StateF0.field_22 = 1;
                }
                Gp_ArmStateF0(1);
                work->field_39C        = 2;
                work->field_392        = 9;
                work->field_3A8        = Actor05500_D089A0[((Actor105500Ctx*)actor->field_20)->field_3C->rowIndex];
                work->field_3BC        = 0x2D;
                work->field_2E4.radius = 0x12C;
                work->field_2E4.coord  = coord;
                work->field_2E4.pos.vy = -0x12C;
                work->field_2E4.key    = Gp_PackPair(&Actor05500_D08958, 5);
                work->field_2E4.flags |= 0x8000;
                if ((sessionFlags & 0xFFFF0000) == 0x05200000) {
                    value = (((u16)((Actor105500Ctx*)actor->field_20)->field_8 >> 0xC) << 8) | 0x55200006;
                    pan1  = (s8)Gp_GetObjPan((GpObj38*)coord);
                    SndEvt_EnqueueType6(value, (s32)pan1, (s8)Gp_GetObjDepth((GpObj38*)coord));
                }
            }
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            work->field_370 = coord->workm;
            break;
        case 1:
            work->field_3A0 = (u16)work->field_3A0 + ((u16)work->field_35C.vy - (u16)coord->coord.t[1]);
            if (((D_80073B8C[0]->t[1] - 0x3E8) < coord->coord.t[1]) || (work->field_3D0 != 0) || (work->field_3CE != 0)) {
                work->field_39C = 2;
                work->field_392 = 9;
                work->field_3BC = 0x2D;
            }
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            work->field_370 = coord->workm;
            break;
        case 2:
            work->field_3A8 = ((s16)work->field_396 >= 0xC) << 7;
            if (work->field_3CC != 0) {
                work->field_39C        = 3;
                work->field_392        = 0xA;
                work->field_3A8        = 0x80;
                work->field_2E4.flags &= 0x7FFF;
                value                  = (((u16)((Actor105500Ctx*)actor->field_20)->field_8 >> 0xC) << 8) | 0x401A0002;
                pan2                   = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(value, (s32)pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            break;
        case 3:
            if ((s16)work->field_396 >= 0x1E) {
                Gp_ArmStateF0(1);
                work->field_39A        = state;
                work->field_39C        = 0;
                work->field_392        = 1;
                index                  = ((Actor105500Ctx*)actor->field_20)->field_3C->rowIndex;
                work->field_39E        = Actor05500_D08980[index] + (((random = (Gp_LcgState * 5) + 0x71357911) >> 0x10) & 0xF);
                work->field_2E4.coord  = actor->field_2C->field_8 + 4;
                work->field_2E4.radius = 0xC8;
                work->field_2E4.pos.vy = 0;
                work->field_3C8        = 0;
                Gp_LcgState            = random;
                if (((Actor105500Ctx*)actor->field_20)->field_40 <= 0) {
                    work->field_39A = 9;
                    work->field_39C = 0;
                    actor->field_30 = 2;
                }
            }
            break;
    }
    Actor05500_Fn02C94(actor);
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) + 1;
}

void Actor05500_Fn00FA0(Actor105500* arg0)
{
    Actor105500Work*       work;
    GsCOORDINATE2*         coord;
    s32                    state;
    s16                    timer;
    s16                    timer2;
    s32                    distance;
    s32                    sound;
    s32                    dx;
    s32                    dz;
    s32                    pan;
    u32                    random;
    u32                    random2;
    Actor105500RotScratch* delta;
    Actor105500RotScratch* scratchEnd;

    scratchEnd                                        = *(Actor105500RotScratch**)PSX_SCRATCH_ADDR(0x3FC);
    delta                                             = scratchEnd - 1;
    *(Actor105500RotScratch**)PSX_SCRATCH_ADDR(0x3FC) = delta;
    work                                              = arg0->field_1C;
    state                                             = work->field_39C;
    coord                                             = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            work->field_3C8 = 0;
            work->field_398 = 0;
            work->field_3A6 = 0;
            timer           = (u16)work->field_39E - 1;
            work->field_39E = timer;
            if (timer <= 0) {
                work->field_39C = 1;
                work->field_392 = 2;
                random          = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = Actor05500_D08990[((Actor105500Ctx*)arg0->field_20)->field_3C->rowIndex] + ((random >> 0x10) & 0x3FF);
                Gp_LcgState     = random;
                return;
            }
            return;
        case 1:
            scratchEnd[-1].vec.vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
            delta->vec.vy         = 0;
            delta->vec.vz         = (s32)(Player_Status.coordMtx->t[2] - coord->coord.t[2]);
            work->field_3A4       = ratan2((s32)(s16)scratchEnd[-1].vec.vx, (s32)(s16)delta->vec.vz) & 0xFFF;
            work->field_3A6       = 0x12;
            if ((s16)work->field_396 >= 0xB) {
                work->field_398 = 0x17;
            }
            timer2          = (u16)work->field_39E - (u16)work->field_398;
            work->field_39E = timer2;
            if (timer2 <= 0) {
                work->field_39C = 0;
                work->field_392 = state;
                random2         = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = Actor05500_D08980[((Actor105500Ctx*)arg0->field_20)->field_3C->rowIndex] + ((random2 >> 0x10) & 0xF);
                Gp_LcgState     = random2;
                return;
            }
            if ((s16)work->field_396 == 0xC) {
                sound = (((u16)((Actor105500Ctx*)arg0->field_20)->field_8 >> 0xC) << 8) | 0x401A0001;
                pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if ((s16)work->field_396 >= 0x29) {
                work->field_396 = 0xB;
            }
            if (work->field_3A4 == work->field_3A2) {
                scratchEnd[-1].vec.vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
                delta->vec.vy         = 0;
                dz                    = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                delta->vec.vz         = dz;
                dx                    = scratchEnd[-1].vec.vx;
                distance              = SquareRoot0((dx * dx) + (dz * dz));
                if ((work->field_3C0 == 0) && (distance < 0x578) && (work->field_3B0 == 0) && !(Player_Status.peStateFlags & 1)) {
                    work->field_39A = 4;
                    work->field_39C = 0;
                    work->field_392 = 3;
                    work->field_3AC = 0;
                } else if (distance < 0x8FC) {
                    work->field_39A = 5;
                    work->field_39C = 0;
                    work->field_392 = 4;
                }
            }
            break;
    }
}

void Actor05500_Fn012E8(Actor105500* arg0)
{
    Actor105500Work* work;
    GsCOORDINATE2*   coord;
    s32              sound;
    s32              pan;
    u32              random;

    work            = arg0->field_1C;
    coord           = arg0->field_2C->field_8;
    work->field_398 = 0;
    work->field_3A6 = 0;
    if ((s16)work->field_396 == 0x28) {
        sound = (((u16)((Actor105500Ctx*)arg0->field_20)->field_8 >> 0xC) << 8) | 0x401A0003;
        pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
    if ((u32)(work->field_396 - 0x2B) < 7U) {
        Gp_SpawnEnemyFromTable(work->field_36C, 1, 0, (GpEnemy*)arg0->field_20);
        work->field_3AC = (u16)(work->field_3AC + 1);
    }
    if ((s16)work->field_396 >= (Actor05500_D08A1E + 0x3C)) {
        work->field_39A = 3;
        work->field_39C = 0;
        work->field_392 = 1;
        random          = (Gp_LcgState * 5) + 0x71357911;
        work->field_39E = Actor05500_D08980[((Actor105500Ctx*)arg0->field_20)->field_3C->rowIndex] + ((random >> 0x10) & 0xF);
        Gp_LcgState     = random;
    }
}

void Actor05500_Fn0143C(Actor105500* arg0)
{
    Actor105500Work*  work;
    GsCOORDINATE2*    coord;
    SVECTOR*          scratchEnd;
    register SVECTOR* allocated asm("v1");
    SVECTOR*          rotation;
    s16(*motion0)[2];
    s16(*motion1)[2];
    s16 state;
    s32 sound;
    s32 index;
    s32 pan;
    s32 pan1;
    u32 random;

    scratchEnd                          = *(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC);
    allocated                           = scratchEnd - 1;
    rotation                            = allocated;
    *(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC) = allocated;
    work                                = arg0->field_1C;
    state                               = work->field_39C;
    coord                               = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            work->field_3C8 = 0;
            work->field_3CA = 0;
            work->field_398 = 0;
            work->field_3A6 = 0;
            if ((u32)(work->field_396 - 0x1B) < 0xDU) {
                work->field_3C8        = 1;
                work->field_3CA        = 1;
                work->field_2E4.flags |= 0xC000;
                if (work->field_3B0 == 0) {
                    index = (s16)work->field_396 < 0x22;
                } else {
                    index = 3;
                    if ((s16)work->field_396 < 0x22) {
                        index = 4;
                    }
                }
                work->field_2E4.key = Gp_PackPair(&Actor05500_D08958, index);
                if (((s16)work->field_396 < 0x23) && ((work->field_3D0 != 0) || (work->field_3CE != 0))) {
                    work->field_39C        = 1;
                    work->field_3CA        = 0;
                    work->field_392        = 5;
                    work->field_2E4.flags &= 0x3FFF;
                    break;
                }
            } else {
                work->field_2E4.flags &= 0x3FFF;
            }
            index   = 0;
            motion0 = Actor05500_D08A38;
            for (; index < 9; index++, motion0++) {
                if ((s16)work->field_396 <= (motion0[0][0] + Actor05500_D08A20)) {
                    coord->coord.t[0] += (s32)(motion0[0][1] * rsin((s32)work->field_3A2)) >> 0xC;
                    coord->coord.t[2] += (s32)(motion0[0][1] * rcos((s32)work->field_3A2)) >> 0xC;
                    break;
                }
            }
            if ((s16)work->field_396 == 0x28) {
                sound = (((u16)((Actor105500Ctx*)arg0->field_20)->field_8 >> 0xC) << 8) | 0x401A0002;
                pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
                work->field_3C8 = 0;
                if (((Actor105500Ctx*)arg0->field_20)->field_40 <= 0) {
                    work->field_39A = 9;
                    work->field_39C = 0;
                    arg0->field_30  = 2;
                }
            }
            if ((s16)work->field_396 >= (Actor05500_D08A20 + 0x46)) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                random          = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = Actor05500_D08980[((Actor105500Ctx*)arg0->field_20)->field_3C->rowIndex] + ((random >> 0x10) & 0xF);
                Gp_LcgState     = random;
            }
            break;
        case 1:
            index           = 0;
            motion1         = Actor05500_D08A5C;
            work->field_398 = 0;
            work->field_3A6 = 0;
            for (; index < 9; index++, motion1++) {
                if ((s16)work->field_396 <= motion1[0][0]) {
                    coord->coord.t[0] += (s32)(motion1[0][1] * rsin((s32)work->field_3A2)) >> 0xC;
                    coord->coord.t[2] += (s32)(motion1[0][1] * rcos((s32)work->field_3A2)) >> 0xC;
                    break;
                }
            }
            if ((u32)(work->field_396 - 0x1F) < 0xFU) {
                coord->coord.t[0] += (s32)(rcos((s32)work->field_3A2) * 0xB) >> 0xC;
                coord->coord.t[2] += (s32)(rsin((s32)work->field_3A2) * 0xB) >> 0xC;
            }
            if ((s16)work->field_396 == 0x10) {
                sound = (((u16)((Actor105500Ctx*)arg0->field_20)->field_8 >> 0xC) << 8) | 0x401A0002;
                pan1  = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(sound, (s32)pan1, (s8)Gp_GetObjDepth((GpObj38*)coord));
                work->field_3C8 = 2;
                if (((Actor105500Ctx*)arg0->field_20)->field_40 <= 0) {
                    work->field_3A2 = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
                    rotation->vx    = 0;
                    rotation->vy    = (u16)work->field_3A2 + 0x800;
                    rotation->vz    = 0;
                    RotMatrix(rotation, &coord->coord);
                    work->field_39A = 9;
                    work->field_39C = 0;
                    arg0->field_30  = 2;
                }
            }
            if ((s16)work->field_396 >= 0x46) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                work->field_39E = Actor05500_D08980[((Actor105500Ctx*)arg0->field_20)->field_3C->rowIndex] + (((Gp_LcgState = (Gp_LcgState * 5) + 0x71357911) >> 0x10) & 0xF);
                work->field_3AA = 1;
                if (work->field_3C8 == 2) {
                    work->field_3A2 = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
                    rotation->vx    = 0;
                    rotation->vy    = (u16)work->field_3A2 + 0x800;
                    rotation->vz    = 0;
                    RotMatrix(rotation, &coord->coord);
                    work->field_3C8 = 0;
                }
                for (index = 1; index < 8; index++) {
                    func_800B4114(work, index, (s32)work->field_392, 0, 0);
                }
            }
            break;
    }
    *(s32*)PSX_SCRATCH_ADDR(0x3FC) += 8;
}

#include "actors_shared_fn01a0c.c"

void Actor05500_Fn01B30(Actor105500* arg0)
{
    Actor105500Obj2C* obj;
    Actor105500Work*  work;
    GsCOORDINATE2*    coord;
    s16(*motion)[2];
    SVECTOR*        scratchEnd;
    SVECTOR*        velocity;
    s32             state;
    s32             one;
    s32             pan1;
    s32             pan2;
    s32             pan3;
    s16             timer;
    Actor105500Ctx* ctx;
    s32             indexOrSound;
    u32             randomY;
    u32             randomZ;
    u32             randomX;
    u32             randomDelay;
    u32             randomRise;

    obj        = arg0->field_2C;
    work       = arg0->field_1C;
    ctx        = arg0->field_20;
    scratchEnd = *(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC);
    velocity   = (*(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC) = scratchEnd - 1);
    state      = work->field_39C;
    coord      = obj->field_8;
    one        = 1;
    switch (state) {
        case 0:
            work->field_294.flags &= 0x7FFF;
            work->field_214.flags &= 0xBFFF;
            obj->field_C           = (u16)obj->field_C | 0x84;
            ctx->field_14          = one;
            if (D_8011540E == one) {
                if (work->field_3C2 == 0) {
                    work->field_39E = Actor05500_D089B0[work->field_3C4];
                } else {
                    work->field_39E = Actor05500_D089E0[work->field_3C4];
                }
                work->field_39C = 1;
            }
            break;
        case 1:
            timer           = (u16)work->field_39E - 1;
            work->field_39E = timer;
            if (timer <= 0) {
                work->field_39E = 0;
                work->field_39C = 2;
            }
            break;
        case 2:
            Tmd_AllocBuffers((TmdObject*)obj);
            obj->field_C = (u16)obj->field_C & 0xFFFB;
            indexOrSound = 0;
            if (work->field_3C2 == 0) {
                work->field_39C = 3;
                work->field_392 = 4;
                work->field_398 = 0;
                work->field_3A6 = 0;
                work->field_3A2 = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
            } else {
                work->field_392        = 7;
                work->field_39A        = state;
                work->field_39C        = 1;
                work->field_3A8        = Actor05500_D089E8[work->field_3C4];
                work->field_294.flags |= 0x8000;
                work->field_214.flags |= 0x4000;
                do {
                    velocity->vx = 0;
                    velocity->vz = 0;
                    randomRise   = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomRise;
                    velocity->vy = ((randomRise >> 0x10) & 0x1FF) + 0x2EE;
                    Gp_SpawnEff(0x6017C, coord, 0, velocity);
                    indexOrSound++;
                } while (indexOrSound < 5);
                indexOrSound = (((u16)((Actor105500Ctx*)arg0->field_20)->field_8 >> 0xC) << 8) | 0x510D0012;
                pan1         = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(indexOrSound, (s32)pan1, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            break;
        case 3:
            if ((s16)work->field_396 == 0x1E) {
                indexOrSound = (((u16)((Actor105500Ctx*)arg0->field_20)->field_8 >> 0xC) << 8) | 0x51090007;
                pan2         = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(indexOrSound, (s32)pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            indexOrSound = 0;
            if ((s16)work->field_396 == 0x27) {
                do {
                    randomX      = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomX;
                    velocity->vx = -((coord->coord.m[0][2] * (s32)(((randomX >> 16) & 0x3F) + 0xAF)) >> 12);
                    randomY      = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomY;
                    velocity->vy = ((randomY >> 16) & 0x1FF) - 0x6D6;
                    randomZ      = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState  = randomZ;
                    velocity->vz = -((coord->coord.m[2][2] * (s32)(((randomZ >> 16) & 0x3F) + 0xAF)) >> 12);
                    Gp_SpawnEff(0x60051, coord, 0, velocity);
                    indexOrSound++;
                } while (indexOrSound < 3);
                indexOrSound = 0;
            }
            motion = Actor05500_D08A38;
            do {
                indexOrSound++;
                if ((s16)work->field_396 <= ((*motion)[0] + Actor05500_D08A20)) {
                    coord->coord.t[0] += ((*motion)[1] * rsin(work->field_3A2)) >> 12;
                    coord->coord.t[2] += ((*motion)[1] * rcos(work->field_3A2)) >> 12;
                    break;
                }
                motion++;
            } while (indexOrSound < 9);
            if ((s16)work->field_396 == 0x28) {
                indexOrSound = (((u16)((Actor105500Ctx*)arg0->field_20)->field_8 >> 0xC) << 8) | 0x401A0002;
                pan3         = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(indexOrSound, (s32)pan3, (s8)Gp_GetObjDepth((GpObj38*)coord));
                work->field_3A8        = 0x80;
                work->field_294.flags |= 0x8000;
                work->field_214.flags |= 0x4000;
            }
            if ((s16)work->field_396 >= (Actor05500_D08A20 + 0x46)) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                randomDelay     = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = Actor05500_D08980[((Actor105500Ctx*)arg0->field_20)->field_3C->rowIndex] + ((randomDelay >> 0x10) & 0xF);
                Gp_LcgState     = randomDelay;
                Gp_ArmStateF0(1);
            }
            break;
    }
    *(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC) = *(SVECTOR**)PSX_SCRATCH_ADDR(0x3FC) + 1;
}

#define ActorsSharedFn020d4 Actor05500_Fn020D4
#include "actors_shared_fn020d4.c"
#undef ActorsSharedFn020d4

#define ActorsSharedFn02214 Actor05500_Fn02214
#include "actors_shared_fn02214.c"
#undef ActorsSharedFn02214

void Actor05500_Fn02364(Actor105500Ctx* arg0, Actor105500* arg1)
{
    VECTOR            vec;
    Actor105500Work*  work;
    Actor105500Work*  initialWork;
    Actor105500Work*  dyingWork;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    colorCoord;
    Actor105500Obj2C* obj;
    s16               initialAnim;
    s16               dyingAnim;
    s16               releasePhase;
    s16               state;
    s32               releaseId;
    s32               initialIndex;
    s32               dyingIndex;
    u16               age;
    u16               destroyAge;

    obj   = arg1->field_2C;
    work  = arg1->field_1C;
    coord = obj->field_8;
    switch ((s32)D_801153F4) {
        case 1:
            vec.vx = coord->workm.t[0];
            vec.vy = coord->workm.t[1];
            vec.vz = coord->workm.t[2];
            Gp_UpdateActorColor((GpEnemy*)arg1->field_20, &vec, 0, 0);
            return;
        case 2:
            obj->field_C = 0x80;
            return;
        case 0:
        default:
            state = work->field_39C;
            switch (state) {
                case 0:
                    work->field_3A0 = 0x1000;
                    work->field_370 = coord->coord;
                    arg0->field_54  = 0;
                    Gp_UnlinkNode((GpLinkNode*)&arg0->field_10);
                    Gp_UnlinkObj(&work->field_214);
                    Gp_UnlinkObj(&work->field_294);
                    Gp_UnlinkObj(&work->field_2E4);
                    Gp_UnlinkObj(&work->field_31C);
                    releaseId = 0x37;
                    if (work->field_3C0 == 0) {
                        releaseId = 0x1A;
                    }
                    Gp_ReleaseStateF0Add((GpObj20E*)arg1, releaseId);
                    Gp_SetStateF0Byte3(2);
                    work->field_39E = 0U;
                    work->field_39C = 1;
                    Gp_SetLightMode((GpObj4C*)arg0, 1);
                    if (work->field_3BA != 0) {
                        obj->field_C = 0x80;
                    }
                    work->field_392 = 0xB;
                    initialWork     = arg1->field_1C;
                    initialIndex    = 1;
                    if (initialWork->field_392 != initialWork->field_394) {
                        initialWork->field_394 = (s16)(u16)initialWork->field_392;
                        initialWork->field_396 = 0U;
                        initialAnim            = Actor05500_D08A18[initialWork->field_392];
                        do {
                            func_800B4114(initialWork, initialIndex, (s32)initialWork->field_392, 0, (s32)initialAnim);
                            initialIndex += 1;
                        } while (initialIndex < 8);
                    } else {
                        TOUCH_REG(initialIndex);
                        initialWork->field_396 += initialIndex;
                        do {
                            Gp_AnimTickIndex((GpAnimCtx*)initialWork, initialIndex);
                            initialIndex += 1;
                        } while (initialIndex < 8);
                    }
                    colorCoord = arg1->field_2C->field_8;
                    vec.vx     = colorCoord->workm.t[0];
                    vec.vy     = colorCoord->workm.t[1];
                    vec.vz     = colorCoord->workm.t[2];
                    Gp_UpdateActorColor((GpEnemy*)arg1->field_20, &vec, 0, 0);
                    return;
                case 1:
                    releasePhase = work->field_3BA;
                    if (releasePhase != 0) {
                        if (releasePhase >= 2) {
                            work->field_3BA = 0;
                            Tmd_FreeBuffers((TmdObject*)obj);
                            obj->field_C |= 4;
                            Actor05500_Fn03C54(arg1);
                            Actor05500_Fn03D40(arg1);
                        } else {
                            work->field_3BA = (s16)((u16)work->field_3BA + 1);
                        }
                    }
                    Actor05500_Fn03B60(arg1);
                    age             = work->field_39E + 1;
                    work->field_39E = age;
                    if ((s16)age == 0xA) {
                        obj->field_C = 2;
                    }
                    if ((s16)work->field_39E == 0xF) {
                        Gp_SpawnEff(0x600A5, coord, 2, NULL);
                    }
                    if ((s16)work->field_39E >= 0x3C) {
                        work->field_39C = 2;
                        work->field_39E = 0U;
                        obj->field_C    = 0x80;
                    }
                    dyingWork  = arg1->field_1C;
                    dyingIndex = 1;
                    if (dyingWork->field_392 != dyingWork->field_394) {
                        dyingWork->field_394 = (s16)(u16)dyingWork->field_392;
                        dyingWork->field_396 = 0U;
                        dyingAnim            = Actor05500_D08A18[dyingWork->field_392];
                        do {
                            func_800B4114(dyingWork, dyingIndex, (s32)dyingWork->field_392, 0, (s32)dyingAnim);
                            dyingIndex += 1;
                        } while (dyingIndex < 8);
                    } else {
                        TOUCH_REG(dyingIndex);
                        dyingWork->field_396 += dyingIndex;
                        do {
                            Gp_AnimTickIndex((GpAnimCtx*)dyingWork, dyingIndex);
                            dyingIndex += 1;
                        } while (dyingIndex < 8);
                    }

                    colorCoord = arg1->field_2C->field_8;
                    vec.vx     = colorCoord->workm.t[0];
                    vec.vy     = colorCoord->workm.t[1];
                    vec.vz     = colorCoord->workm.t[2];
                    Gp_UpdateActorColor((GpEnemy*)arg1->field_20, &vec, 0, 0);
                    return;

                case 2:
                    destroyAge      = work->field_39E + 1;
                    work->field_39E = destroyAge;
                    if ((s16)destroyAge >= 0x3C) {
                        Gp_DestroyEnemy((GpEnemy*)arg0, (Task*)arg1);
                    }
                    return;
            }
            break;
    }
}

void Actor05500_Fn02780(Actor105500Ctx* arg0, Actor105500* arg1)
{
    Actor105500Work* work;
    GsCOORDINATE2*   coord;
    s16              age;
    s16              speed;
    s32              contact;
    u16              flags;
    u32              random;

    coord = arg1->field_2C->field_8;
    work  = arg1->field_1C;
    switch ((s32)D_801153F4) {
        case 1:
            Actor05500_Fn02954(arg1, work->field_38);
            return;
        default:
        default_case:
            contact = work->rec.key;
            if (contact != 0) {
                if ((contact & 0xFFFF0000) != 0x100000) {
                    work->obj.flags &= 0x7FFF;
                    Gp_ClearRec18Occupied(&work->rec);
                    goto block_7;
                }
                goto block_11;
            }
        block_7:
            if (!((u16)work->field_38 & 3)) {
                flags = work->obj.flags | 0xC000;
            } else {
                flags = work->obj.flags & 0x3FFF;
            }
            work->obj.flags    = flags;
            coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_3A) >> 0xC;
            coord->coord.t[1] += (s32)(coord->coord.m[1][2] * work->field_3A) >> 0xC;
            coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_3A) >> 0xC;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            Actor05500_Fn02954(arg1, work->field_38);
            age            = (u16)work->field_38 + 1;
            work->field_38 = age;
            if (age >= 0xF) {
            block_11:
                Gp_UnlinkObj(&work->obj);
                arg1->field_30 = 2;
                return;
            }
            random         = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState    = random;
            speed          = (u16)work->field_3A - ((random >> 0x10) & 0x1F);
            work->field_3A = speed;
            if (speed < 0) {
                work->field_3A = 0;
            }
            return;
        case 0:
            goto default_case;
        case 2:
            return;
    }
}

void Actor05500_Fn02954(Actor105500* actor, s32 frame)
{
    POLY_FT4*               poly;
    GsCOORDINATE2*          coord;
    s32                     depth;
    s32                     screen;
    s32                     y;
    s32                     radius;
    s32                     x;
    s32                     bottom;
    s32                     top;
    s32                     left;
    s32                     right;
    Actor105500QuadScratch* scratchEnd;
    Actor105500QuadScratch* s;
    Actor105500TextureObj*  texture;
    Actor105500Uv*          uv;
    SVECTOR*                projection;

    scratchEnd                     = (Actor105500QuadScratch*)*(u8**)PSX_SCRATCH_ADDR(0x3FC);
    coord                          = actor->field_2C->field_8;
    actor                          = (Actor105500*)((Task*)actor)->parent;
    texture                        = (Actor105500TextureObj*)actor->field_2C;
    scratchEnd[-1].p[0].vx         = (u16)coord->workm.t[0];
    s                              = scratchEnd - 1;
    s->p[0].vy                     = (u16)coord->workm.t[1];
    *(u8**)PSX_SCRATCH_ADDR(0x3FC) = (u8*)s;
    s->p[0].vz                     = (u16)coord->workm.t[2];
    projection                     = &s->p[0];
    SOFT_TOUCH_REG(projection);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldv0(projection);
    __asm__ volatile("nop; nop; .word 0x4a180001");
    gte_stsxy(&scratchEnd[-1].screen);
    gte_stszotz(&scratchEnd[-1].depth);
    depth = s->depth;
    if (depth >= 0x14) {
        radius                 = (s32)(Actor05500_D08AA0[frame] * 0x300) / depth;
        poly                   = Gpu_PrimCursor;
        screen                 = s->screen;
        Gpu_PrimCursor         = (u8*)poly + 0x28;
        x                      = screen & 0xFFFF;
        y                      = screen >> 0x10;
        left                   = x - radius;
        top                    = y - radius;
        right                  = x + radius;
        bottom                 = y + radius;
        scratchEnd[-1].p[0].vx = left;
        s->p[0].vy             = top;
        s->p[0].vz             = 0;
        s->p[1].vx             = right;
        s->p[1].vy             = top;
        s->p[1].vz             = 0;
        s->p[2].vx             = left;
        s->p[2].vy             = bottom;
        s->p[2].vz             = 0;
        s->p[3].vx             = right;
        s->p[3].vy             = bottom;
        s->p[3].vz             = 0;
        setPolyFT4(poly);
        setSemiTrans(poly, 1);
        setRGB0(poly, 0x80, 0x80, 0x80);
        setShadeTex(poly, 1);
        poly->tpage = (s16)(((s32)(((texture->field_24 << 6) + 0x180) & 0x3FF) >> 6) | 0xB0);
        poly->clut  = (s16)(((s32)(texture->field_25 << 0x18) >> 0x12) + 0x3D40);
        uv          = &Actor05500_D08A80[frame >> 1];
        poly->u0    = (u8)uv->u;
        poly->v0    = (u8)uv->v;
        poly->u1    = (s8)(uv->u + 0x1F);
        poly->v1    = (u8)uv->v;
        poly->u2    = (u8)uv->u;
        poly->v2    = (s8)(uv->v + 0x1F);
        poly->u3    = (s8)(uv->u + 0x1F);
        poly->v3    = (s8)(uv->v + 0x1F);
        poly->x0    = (u16)scratchEnd[-1].p[0].vx;
        poly->y0    = (u16)s->p[0].vy;
        poly->x1    = (u16)s->p[1].vx;
        poly->y1    = (u16)s->p[1].vy;
        poly->x2    = (u16)s->p[2].vx;
        poly->y2    = (u16)s->p[2].vy;
        poly->x3    = (u16)s->p[3].vx;
        poly->y3    = (u16)s->p[3].vy;
        addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), poly);
    }
    *(u8**)PSX_SCRATCH_ADDR(0x3FC) += 0x28;
}

#define ActorsSharedFn02c94 Actor05500_Fn02C94
#include "actors_shared_fn02c94.c"
#undef ActorsSharedFn02c94

void Actor05500_Fn02FFC(GpEnemy* ctx, Task* actor)
{
    SVECTOR           rot;
    GpRec18*          rec0;
    GpRec18*          rec1;
    GpRec18*          rec2;
    GpRec18*          rec3;
    SVECTOR*          positions;
    MATRIX*           matrix;
    Actor105500Work*  work;
    s32               variant;
    s32               quotient;
    s32               i;
    s32               mode;
    GpAreaPlace*      params;
    GsCOORDINATE2*    coord;
    Actor105500Obj2C* obj;

    obj   = actor->extra;
    coord = obj->field_8;
    work  = memCalloc(sizeof(Actor105500Work), 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work     = (void*)work;
    obj->field_C    = 0;
    coord->flg      = 0;
    obj->field_1C   = &work->field_1F4;
    obj->field_20   = &work->field_1D4;
    matrix          = &coord->coord;
    work->field_3C0 = 1;
    work->field_36C = &Actor05500_D08ABC;
    ctx->field_4    = matrix;
    ctx->field_48   = 0;
    Gp_LinkNode(&ctx->node);
    ctx->coord                 = ((Actor105500Obj2C*)actor->extra)->field_8 + 1;
    ctx->bodyPos.vy            = -0x64;
    ctx->recs                  = work->field_2B4;
    ctx->bodyPos.vx            = 0;
    ctx->bodyPos.vz            = 0;
    ctx->param                 = &Actor05500_D08970;
    ctx->hp                    = (s16)Actor05500_D08970.hpMax;
    work->field_354.coord      = coord;
    work->field_354.spawnArgLo = 0x100;
    work->field_354.spawnArgHi = 1;
    work->field_3C4            = (s16)ctx->place->variant;
    params                     = ctx->place;
    mode                       = params->mode;
    if (mode < 10) {
        switch (mode) {
            case 0:
                work->field_392 = 0xC;
                work->field_39A = 0;
                work->field_3A8 = 0x80;
                work->field_3C6 = 0;
                break;
            case 1:
                work->field_39A = mode;
                work->field_392 = mode;
                work->field_3A8 = 0x80;
                work->field_3C6 = 0;
                break;
            case 2:
                work->field_39A    = mode;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C6    = 0;
                work->field_3C8    = 1;
                coord->coord.t[1] += 0x3E8;
                break;
            case 3:
                work->field_39A    = 2;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C6    = 1;
                work->field_3C8    = 1;
                coord->coord.t[1] += 0x3E8;
                break;
        }
    } else {
        work->field_3C4 = (s16)params->variant;
        quotient        = mode / 10;
        variant         = mode - quotient * 10;
        switch (variant) {
            case 0:
                if (GameFlag_GetNibble(0xCC) == 1) {
                    work->field_392 = 0xC;
                    work->field_39A = 0;
                    work->field_3A8 = 0x80;
                    rot.vx          = 0;
                    rot.vy          = Actor05500_D089D8[work->field_3C4];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = Actor05500_D089B8;
                    coord->coord.t[0] = positions[work->field_3C4].vx;
                    coord->coord.t[1] = positions[work->field_3C4].vy;
                    coord->coord.t[2] = positions[work->field_3C4].vz;
                } else {
                    work->field_3C2 = 0;
                    work->field_39A = 8;
                    work->field_392 = 1;
                    work->field_3A8 = 0;
                }
                break;
            case 1:
                if (GameFlag_GetNibble(0xCB) == 2) {
                    work->field_392 = 0xC;
                    work->field_39A = 0;
                    work->field_3A8 = 0x80;
                    rot.vx          = 0;
                    rot.vy          = Actor05500_D08A10[work->field_3C4];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = Actor05500_D089F0;
                    coord->coord.t[0] = positions[work->field_3C4].vx;
                    coord->coord.t[1] = positions[work->field_3C4].vy;
                    coord->coord.t[2] = positions[work->field_3C4].vz;
                    break;
                }
                work->field_39A    = 8;
                work->field_3C2    = variant;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C8    = variant;
                coord->coord.t[1] += 0x3E8;
        }
    }
    func_800B3F84((GpAnimCtx*)work, Actor05500_D08AD4, (TmdObject*)obj, work->field_154, ((Actor105500Anim*)work)->slots);
    for (i = 1; i < 8; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    rec0                     = work->field_234;
    work->field_214.coord    = coord;
    work->field_214.ctx.recs = rec0;
    work->field_214.pos.vx   = 0;
    work->field_214.pos.vy   = -0x12C;
    work->field_214.pos.vz   = 0;
    work->field_214.key      = 0x30037;
    work->field_214.radius   = 0x12C;
    work->field_214.flags    = 1;
    Gp_LinkObj(2, &work->field_214);
    Gp_InitRec18Table(rec0, 4, 0);
    work->field_214.flags   |= 0x4200;
    work->field_294.coord    = ((Actor105500Obj2C*)actor->extra)->field_8 + 1;
    rec1                     = work->field_2B4;
    work->field_294.ctx.recs = rec1;
    work->field_294.pos.vx   = 0;
    work->field_294.pos.vy   = -0x64;
    work->field_294.pos.vz   = 0;
    work->field_294.key      = 0x30037;
    work->field_294.radius   = 0x12C;
    work->field_294.flags    = 1;
    Gp_LinkObj(2, &work->field_294);
    Gp_InitRec18Table(rec1, 2, 0);
    work->field_294.flags   |= 0x8000;
    work->field_2E4.coord    = ((Actor105500Obj2C*)actor->extra)->field_8 + 4;
    rec2                     = work->field_304;
    work->field_2E4.ctx.recs = rec2;
    work->field_2E4.pos.vx   = 0;
    work->field_2E4.pos.vy   = 0;
    work->field_2E4.pos.vz   = 0;
    work->field_2E4.key      = 0;
    work->field_2E4.radius   = 0xC8;
    work->field_2E4.flags    = 1;
    Gp_LinkObj(3, &work->field_2E4);
    Gp_InitRec18Table(rec2, 1, 0);
    work->field_2E4.flags   &= 0x7FFF;
    work->field_31C.coord    = ((Actor105500Obj2C*)actor->extra)->field_8 + 4;
    rec3                     = work->field_33C;
    work->field_31C.ctx.recs = rec3;
    work->field_31C.pos.vx   = 0;
    work->field_31C.pos.vy   = 0;
    work->field_31C.pos.vz   = 0;
    work->field_31C.key      = 0x22424;
    work->field_31C.radius   = 0x1F4;
    work->field_31C.flags    = 1;
    Gp_LinkObj(1, &work->field_31C);
    Gp_InitRec18Table(rec3, 1, 0);
    work->field_31C.flags &= 0x7FFF;
    actor->state           = 1;
}

void Actor05500_Fn03560(Actor105500Ctx* arg0, Actor105500* arg1)
{
    GsCOORDINATE2*    coord;
    Actor105500Obj2C* obj;
    Actor105500Work*  work;
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
        Actor05500_Fn03674(arg1, obj, one);
    }
    Actor05500_Fn0006C(arg1);
    Actor05500_Fn0378C(arg1);
    if (work->field_3B0 != 0) {
        Actor05500_Fn020D4(arg1);
    }
    if (work->field_3A6 != 0) {
        Actor05500_Fn02214(arg1);
    }
    Actor05500_Fn03918(arg1);
    Actor05500_Fn039AC(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    Actor05500_Fn03A70(arg1);
    Actor05500_Fn03AC8(arg1);
}

#define ActorsSharedFn03674 Actor05500_Fn03674
#include "actors_shared_fn03674.c"
#undef ActorsSharedFn03674

void Actor05500_Fn0378C(Actor105500* arg0)
{
    s16 state;

    state = arg0->field_1C->field_39A;
    switch (state) {
        case 0:
            Actor05500_Fn00754(arg0);
            break;
        case 1:
            Actor05500_Fn00914(arg0);
            break;
        case 2:
            Actor05500_Fn00A94(arg0);
            break;
        case 3:
            Actor05500_Fn00FA0(arg0);
            break;
        case 4:
            Actor05500_Fn012E8(arg0);
            break;
        case 5:
            Actor05500_Fn0143C(arg0);
            break;
        case 6:
            Actor05500_Fn01A0C(arg0);
            break;
        case 7:
            Actor05500_Fn03864(arg0);
            break;
        case 8:
            Actor05500_Fn01B30(arg0);
            break;
        case 9:
            break;
    }
}

#define ActorsShared801355a4_Fn3567C Actor05500_Fn03864
#include "actors_shared_fn03864.c"
#undef ActorsShared801355a4_Fn3567C

void Actor05500_Fn03918(Actor105500* arg0)
{
    GsCOORDINATE2*   coord;
    Actor105500Work* work;

    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;

    work->field_35C.vx = coord->coord.t[0];
    work->field_35C.vy = coord->coord.t[1];
    work->field_35C.vz = coord->coord.t[2];

    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_398) >> 12;
    coord->coord.t[1] += work->field_3A8;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_398) >> 12;
}

void Actor05500_Fn039AC(Actor105500* arg0)
{
    Actor105500Work* work;
    s32              i;
    s32              value;

    work = arg0->field_1C;
    i    = 1;
    if (work->field_392 != work->field_394) {
        work->field_394 = work->field_392;
        work->field_396 = 0;
        value           = Actor05500_D08A18[work->field_392];
        for (; i < 8; i++) {
            func_800B4114(work, i, work->field_392, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_396 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 8);
    }
}

void Actor05500_Fn03A70(Actor105500* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03AC8);
