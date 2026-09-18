#include "common.h"

#include "actors/actor_100700.h"
#include "actors/actor_100700_spawn.h"
#include "main/tmd.h"
#include "actors/actors_shared_80135b58.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"
#include "main/display.h"
#include "psyq/inline_c.h"

#define SCRATCH_SP (*(u32*)0x1F8003FC)

s32  Gp_ObjFlag4Expired(Actor00700Ctx* arg0);
s32  Gp_TickObjFlag4(Actor00700Ctx* arg0);
void func_800DA6E8(void* arg0, s32 arg1, s32 arg2);

s32            Gp_TickObjFlag2(Actor00700Ctx* arg0);
void           Gp_ArmStateF0(s32 arg0);
s32            Gp_GetObjPan(void* arg0);
s32            Gp_GetObjDepth(void* arg0);
void           Gp_UpdateCoord(GsCOORDINATE2* arg0);
void           Gp_UnlinkNode(void* node);
void           Gp_UnlinkObj(void* node);
void           Gp_SetLightMode(void* arg0, s32 arg1);
void           Gp_ReleaseStateF0Add(void* arg0, s32 arg1);
void           Gp_UpdateActorColor(void* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void           Gp_DestroyEnemy(void* enemy, void* task);
void           func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void           Gp_AnimTickIndex(void* arg0, s32 arg1);
void*          Gp_SpawnEff(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, void* arg3);
extern s32     Gp_LcgState;
extern MATRIX* D_80073B8C;

void Actor00700_Fn01B50(Actor00700* arg0);
void Actor00700_Fn01EEC(Actor00700* arg0);

void Actor00700_Fn008B4(Actor00700* arg0)
{
    Actor00700Work*  work;
    Actor00700Obj2C* obj;
    GsCOORDINATE2*   coord;
    s32              state;
    s32              one;
    s32              rng0;
    s32              rng1;
    s32              rng2;
    s32              rng3;
    s32              rng4;
    s32              rng5;
    s32              rng6;
    s32              timer;
    s32              next;
    s32              flags;
    s32              ang;
    s32              snd;
    s32              pan;

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
        Actor00700_D06DF0[arg0->field_20->field_3C->field_F]) {
        work->field_37E = 7;
        next            = Actor00700_D06E00[((u32)(rng1 = rng0 * 5 + 0x71357911) >> 16) & 0xF];
        Gp_LcgState     = rng1;
        work->field_37C = one;
        work->field_38C = next;
        goto tail;
    }
    rng2        = rng0 * 5 + 0x71357911;
    Gp_LcgState = rng2;
    if ((s32)(((u32)rng2 >> 16) & 0xF) <
        Actor00700_D06E20[arg0->field_20->field_3C->field_F]) {
        work->field_37E = 2;
        next            = Actor00700_D06E30[((u32)(rng3 = rng2 * 5 + 0x71357911) >> 16) & 0xF];
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
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(coord));
    }
    Actor00700_Fn01B50(arg0);
}

void Actor00700_Fn00BC0(Actor00700* arg0)
{
    VECTOR*          vec;
    Actor00700Work*  work;
    Actor00700Obj2C* obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   target;
    s32              state;
    s32              one;
    s32              dist;
    s32              raw;
    s16              diff;
    s32              adiff;
    s32              ang;
    s32              vel;
    s32              pan;
    s32              snd;

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
        work->field_33C = ((Actor00700*)Game_GetPtrSlot(3))->field_2C->field_8;
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
    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(coord));
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if ((s32)(((u32)Gp_LcgState >> 16) & 0xF) < Actor00700_D06E50[arg0->field_20->field_3C->field_F]) {
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

void Actor00700_Fn00F20(Actor00700* arg0)
{
    VECTOR           vec;
    Actor00700Work*  work;
    Actor00700Obj2C* obj;
    GsCOORDINATE2*   coord;
    s32              state;
    s32              one;
    s32              rng;
    s32              posX;

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
    goto pop;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto pop;
case0:
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
    goto pop;
case1:
    if ((s16)work->field_382 >= 0xF) {
        goto tick;
    }
    coord->coord.t[0] += -(work->field_370.vx * 50) >> 12;
    coord->coord.t[2] += -(work->field_370.vz * 50) >> 12;
tick:
    if ((u32)(work->field_382 - 6) < 9) {
        work->field_386 = 0x93;
        work->field_38A = (work->field_38A + 0x5C7) & 0xFFF;
    } else {
        work->field_386 = 0;
    }
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto pop;
    }
    if ((arg0->field_20->field_4C & 2) != 0) {
        work->field_37E = 8;
        work->field_37A = 3;
        work->field_37C = 3;
        goto pop;
    }
    work->field_37E = 9;
    work->field_37C = 2;
    goto pop;
case2:
    if ((s16)work->field_382 < 0x20) {
        goto pop;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = one;
    work->field_38C = 0;
    work->field_394 = one;
    work->field_396 = 0;
pop:;
}

void Actor00700_Fn01148(Actor00700* arg0)
{
    Actor00700Ctx*  ctx;
    Actor00700Work* work;
    s16             state;
    s32             rng;
    s32             rng2;
    u16             timer;

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
            if (Gp_TickObjFlag2(arg0->field_20) != 0) {
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

void Actor00700_Fn012E4(Actor00700* arg0)
{
    Actor00700Work*       work;
    GsCOORDINATE2*        coord;
    Actor00700RotScratch* sc;
    s32                   ang;
    u16                   want;
    s16                   diff;
    s32                   adiff;
    s32                   step;
    s32                   cur;
    s32                   next;
    s32                   wrapStep;

    sc    = (Actor00700RotScratch*)(SCRATCH_SP -= 0x18);
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

extern u8 D_801153F4;

void Actor00700_Fn01434(Actor00700Ctx* arg0, Actor00700* arg1)
{
    Actor00700Work*  work;
    Actor00700Obj2C* obj;
    GsCOORDINATE2*   coord;
    Actor00700Work*  work2;
    GsCOORDINATE2*   c;
    VECTOR           vec;
    s32              state;
    s32              i;
    s16              st;
    s16              phase;
    s16              val;
    s32              snd;
    s32              pan;

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
    Gp_UpdateActorColor(arg1->field_20, &vec, 0, 0);
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
    arg0->field_54  = 0;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj(&work->field_1DC);
    Gp_UnlinkObj(&work->field_214);
    Gp_UnlinkObj(&work->field_27C);
    Gp_UnlinkObj(&work->field_2FC);
    Gp_SetLightMode(arg0, 1);
    Gp_ReleaseStateF0Add(arg1, 7);
    work->field_37C = 1;
    work2           = arg1->field_1C;
    i               = 1;
    if ((s16)work2->field_37E != work2->field_380) {
        work2->field_380 = work2->field_37E;
        work2->field_382 = 0;
        val              = Actor00700_D06E98[(s16)work2->field_37E];
        do {
            func_800B4114(work2, i, (s16)work2->field_37E, 0, val);
            i++;
        } while (i < 7);
    } else {
        TOUCH_REG(i);
        work2->field_382 += i;
        do {
            Gp_AnimTickIndex(work2, i);
            i++;
        } while (i < 7);
    }
    c      = arg1->field_2C->field_8;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->field_20, &vec, 0, 0);
    snd = ((arg1->field_20->field_8 >> 12) << 8) | 0x40070005;
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(coord));
    return;
dying:
    Actor00700_Fn01EEC(arg1);
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
        val              = Actor00700_D06E98[(s16)work2->field_37E];
        do {
            func_800B4114(work2, i, (s16)work2->field_37E, 0, val);
            i++;
        } while (i < 7);
    } else {
        TOUCH_REG(i);
        work2->field_382 += i;
        do {
            Gp_AnimTickIndex(work2, i);
            i++;
        } while (i < 7);
    }
    c      = arg1->field_2C->field_8;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->field_20, &vec, 0, 0);
    return;
destroy:
    Gp_DestroyEnemy(arg0, arg1);
    return;
}

void Actor00700_Fn01830(Actor00700* arg0)
{
    Actor00700StateFuncTable3 sp;

    sp = Actor00700_D00004;
    sp.funcs[arg0->field_30](arg0->field_20, arg0);
}

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Actor00700_Fn00334(Actor00700* arg0);
void Actor00700_Fn01AB8(Actor00700* arg0);
void Actor00700_Fn01988(Actor00700* arg0, Actor00700Obj2C* arg1, s32 arg2);
void Actor00700_Fn01CF0(Actor00700* arg0);
void Actor00700_Fn01D80(Actor00700* arg0);
void Actor00700_Fn01E44(Actor00700* arg0);
void Actor00700_Fn01E9C(Actor00700* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

extern u8 D_801153F4;

void Actor00700_Fn0188C(Actor00700Ctx* arg0, Actor00700* arg1)
{
    GsCOORDINATE2*   coord;
    Actor00700Obj2C* obj;
    Actor00700Work*  work;
    s32              state;
    s32              one;

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
    obj->field_C       = 0;
    arg0->node.field_4 = 0;
    goto default_body;
case2:
    obj->field_C       = 0x80;
    arg0->node.field_4 = one;
    return;
default_body:
    if (arg0->field_4C != 0) {
        Actor00700_Fn01988(arg1, obj, one);
    }
    Actor00700_Fn00334(arg1);
    Actor00700_Fn01AB8(arg1);
    SOFT_USE_REG(work);
    if (work->field_386 != 0) {
        Actor00700_Fn012E4(arg1);
    }
    Actor00700_Fn01CF0(arg1);
    Actor00700_Fn01D80(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    Actor00700_Fn01E44(arg1);
    Actor00700_Fn01E9C(arg1);
}

void Actor00700_Fn01988(Actor00700* arg0, Actor00700Obj2C* arg1, s32 arg2)
{
    Actor00700Ctx*  ctx;
    Actor00700Work* work;
    s32             damage;
    u16             remaining;
    u8              flags;

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
        damage = Gp_TickObjFlag4(ctx);
        if (damage != 0) {
            func_800DA6E8(&ctx->node, damage, 0);
            remaining     = ctx->field_40 - damage;
            ctx->field_40 = remaining;
            if ((s16)remaining <= 0) {
                work->field_37A = 5;
                work->field_37C = 0;
                arg0->field_30  = 2;
            } else {
                work->field_37A = 4;
                work->field_37C = 0;
            }
        }
        if (Gp_ObjFlag4Expired(ctx) != 0) {
            ctx->field_4C &= 0xF3;
        }
    }
}

void Actor00700_Fn00F20(Actor00700* arg0);
void Actor00700_Fn01148(Actor00700* arg0);
void Actor00700_Fn01C10(Actor00700* arg0);

void Actor00700_Fn01AB8(Actor00700* arg0)
{
    switch (arg0->field_1C->field_37A) {
        case 0:
            Actor00700_Fn008B4(arg0);
            break;
        case 1:
            Actor00700_Fn00BC0(arg0);
            break;
        case 2:
            Actor00700_Fn00F20(arg0);
            break;
        case 3:
            Actor00700_Fn01148(arg0);
            break;
        case 4:
            Actor00700_Fn01C10(arg0);
            break;
        case 5:
            break;
    }
}

void Actor00700_Fn01B50(Actor00700* arg0)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;
    s32             snd;
    s32             pan;
    u16             timer;
    u32             random;

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
        SndEvt_EnqueueType6(snd, (s32)pan, (s8)Gp_GetObjDepth(coord));
    }
}

void Actor00700_Fn01C10(Actor00700* arg0)
{
    Actor00700Work*  work;
    Actor00700Obj2C* obj;
    GsCOORDINATE2*   coord;
    s32              state;
    s32              snd;
    s32              pan;

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
    snd             = ((arg0->field_20->field_8 >> 12) << 8) | 0x40070002;
    pan             = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(coord));
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

void Actor00700_Fn01CF0(Actor00700* arg0)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;

    coord              = arg0->field_2C->field_8;
    work               = arg0->field_1C;
    work->field_360    = coord->coord.t[0];
    work->field_364    = coord->coord.t[1];
    work->field_368    = coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_384) >> 0xC;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_384) >> 0xC;
}

void Actor00700_Fn01D80(Actor00700* arg0)
{
    Actor00700Work* work2;
    s32             i;
    s32             val;

    work2 = arg0->field_1C;
    i     = 1;
    if ((s16)work2->field_37E != work2->field_380) {
        work2->field_380 = work2->field_37E;
        work2->field_382 = 0;
        val              = Actor00700_D06E98[(s16)work2->field_37E];
        do {
            func_800B4114(work2, i, (s16)work2->field_37E, 0, val);
            i++;
        } while (i < 7);
    } else {
        TOUCH_REG(i);
        work2->field_382 += i;
        do {
            Gp_AnimTickIndex(work2, i);
            i++;
        } while (i < 7);
    }
}

void Actor00700_Fn01E44(Actor00700* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}

void Actor00700_Fn01E9C(Actor00700* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR3        vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x1C0, 0x80);
}

void Actor00700_Fn01EEC(Actor00700* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor00700Work*             work;

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

void Gp_AnimResetSlot(void*, s32, s32);
void Gp_IncStateF0Ref(s32);
void Gp_InitRec18Table(void*, s32, s32);
void Gp_LinkNode(void*);
void Gp_LinkObj(s32, void*);
s32  Gp_PackPair(void*, s32);
void func_800B3F84(void*, void*, TmdObject*, void*, void*);

void Actor00700_Fn01FE0(Actor00700Ctx* ctx, Actor00700* actor)
{
    GsCOORDINATE2*       coord;
    TmdObject*           obj;
    s32                  i;
    void*                rec1;
    void*                rec2;
    void*                rec3;
    Actor00700SpawnWork* work;

    obj   = (TmdObject*)actor->field_2C;
    coord = obj->field_8;
    work  = Mem_Calloc(0x2F4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->field_1C = (Actor00700Work*)work;
    obj->field_C    = 0;
    coord->flg      = 0;
    obj->field_24  += 1;
    obj->field_25  += 1;
    Tmd_ProcessStream(obj);
    Tmd_ProcessStream(obj);
    obj->field_1C = &work->field_114;
    obj->field_20 = &work->field_F4;
    ctx->field_4  = (void*)(&coord->coord);
    ctx->field_48 = 0;
    Gp_LinkNode(&ctx->node);
    ctx->field_18     = coord;
    ctx->node.field_4 = 0;
    ctx->field_1C     = 0;
    ctx->field_20     = 0;
    ctx->field_24     = 0;
    ctx->field_50     = &Actor00700_D07588;
    ctx->field_54     = (s32)&work->field_154;
    ctx->field_40     = (u16)Actor00700_D07588.field_4;
    work->field_228   = 0x100;
    work->field_22A   = 1;
    work->field_224   = coord;
    func_800B3F84(work, &Actor00700_D075B4, obj, &work->field_B4, &work->field_14);
    for (i = 1; i < 4; i++) {
        Gp_AnimResetSlot(work, i, 1);
    }
    Gp_IncStateF0Ref(0);
    work->field_2D6 = 1;
    work->field_2AC = (s32)coord->coord.t[0];
    work->field_2B0 = (s32)coord->coord.t[1];
    work->field_2B4 = (s32)coord->coord.t[2];
    work->field_2DC = (u16)actor->field_20->field_3C->field_A;
    rec1            = &work->field_154;
    work->field_13C = coord;
    work->field_140 = rec1;
    work->field_144 = 0;
    work->field_146 = 0;
    work->field_148 = 0;
    work->field_14C = 0x30008;
    work->field_150 = 0xFA;
    work->field_152 = 1U;
    Gp_LinkObj(2, &work->field_134);
    Gp_InitRec18Table(rec1, 1, 0);
    rec2            = &work->field_18C;
    work->field_174 = coord;
    work->field_178 = rec2;
    work->field_17C = 0;
    work->field_17E = 0;
    work->field_180 = 0;
    work->field_184 = 0x30008;
    work->field_188 = 0xFA;
    work->field_18A = 1U;
    work->field_152 = (u16)(work->field_152 | 0x8000);
    Gp_LinkObj(2, &work->field_16C);
    Gp_InitRec18Table(rec2, 4, 0);
    rec3            = &work->field_20C;
    work->field_1F4 = coord;
    work->field_1F8 = rec3;
    work->field_1FC = 0;
    work->field_1FE = 0;
    work->field_200 = 0;
    work->field_18A = (u16)(work->field_18A | 0x4000);
    work->field_204 = Gp_PackPair(&Actor00700_D07584, 0);
    work->field_208 = 0x190;
    work->field_20A = 1U;
    Gp_LinkObj(3, &work->field_1EC);
    Gp_InitRec18Table(rec3, 1, 0);
    work->field_20A = (u16)(work->field_20A & 0x7FFF);
    actor->field_30 = 1;
}

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Actor00700_Fn02414(Actor00700* arg0);
void Actor00700_Fn0268C(Actor00700* arg0);
void Actor00700_Fn02820(Actor00700* arg0);
void Actor00700_Fn02A28(Actor00700* arg0);
void Actor00700_Fn03518(Actor00700* arg0);

extern u8  D_801153F4;
extern s8  D_80115408;
extern s32 Gp_LcgState;

void Actor00700_Fn02290(Actor00700Ctx* arg0, Actor00700* arg1)
{
    Actor00700Obj2C* obj;
    Actor00700Work*  work;
    GsCOORDINATE2*   coord;
    s32              state;
    s32              one;

    work  = arg1->field_1C;
    obj   = arg1->field_2C;
    state = D_801153F4;
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
    obj->field_C       = 0;
    arg0->node.field_4 = 0;
    goto default_body;
case1:
    Actor00700_Fn03518(arg1);
    return;
case2:
    obj->field_C       = 0x80;
    arg0->node.field_4 = one;
    return;
default_body:
    Actor00700_Fn02414(arg1);
    Actor00700_Fn0268C(arg1);
    if (work->field_2E6 == 0 && D_80115408 != 0) {
        work->field_2E6 = 1;
        Gp_ArmStateF0(1);
    }
    Actor00700_Fn02820(arg1);
    Actor00700_Fn02A28(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    Actor00700_Fn03518(arg1);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if (((u32)Gp_LcgState >> 16 & 0x7F) == 0) {
        s32 temp;
        s32 id;

        id   = ((arg0->field_8 >> 12) << 8) | 0x40070008;
        temp = (s8)Gp_GetObjPan(arg1->field_2C->field_8);
        SndEvt_EnqueueType6(id, temp, (s8)Gp_GetObjDepth(arg1->field_2C->field_8));
    }
}

s32  func_800E0C10(GpRec18* arg0, GpDeltaScratch* arg1, s32 arg2, s32* arg3);
u32  Gp_ComputeDamage(u32 arg0, u32 arg1, s32 arg2, s32 arg3);
s32  Gp_GetIdParam1(s32 arg0);
void func_800E2C78(Actor00700Ctx* arg0, s32 arg1, s32 arg2, s32 arg3);

void Actor00700_Fn02414(Actor00700* arg0)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;
    s32             movement;
    s32             dx;
    s32             dy;
    s32             dz;
    s32             amount;
    s32             damage;
    s32             z;
    u16             state;
    GsCOORDINATE2*  target;
    GpDeltaScratch* head;
    GpDeltaScratch* delta;

    work     = arg0->field_1C;
    head     = *(void**)0x1F8003FC;
    delta    = (*(void**)0x1F8003FC = head - 1);
    coord    = arg0->field_2C->field_8;
    movement = func_800E0C10(&work->field_18C, delta, 4, 0);
    switch (movement) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += head[-1].vx.h.hi;
            coord->coord.t[1] += delta->vy.h.hi;
            z                  = coord->coord.t[2] + delta->vz.h.hi;
            coord->coord.t[2]  = z;
            break;
        case 2:
            coord->coord.t[0] = work->field_2BC;
            coord->coord.t[1] = work->field_2C0;
            coord->coord.t[2] = work->field_2C4;
            break;
    }
    Gp_ClearRec18Occupied(&work->field_18C);
    state = (u16)work->field_154.hit.id.h.hi;
    switch ((u32)state) {
        case 0:
            break;
        case 1:
            arg0->field_30           = 2;
            arg0->field_20->field_40 = 0;
            Gp_ArmStateF0(1);
            break;
        case 2:
            arg0->field_30 = (s32)state;
            target         = Gp_ActorSlots[(u8)work->field_154.hit.id.h.lo >> 7]->extra->field_8;
            dx             = target->coord.t[0] - coord->coord.t[0];
            delta->vx.w    = dx;
            dy             = target->coord.t[1] - coord->coord.t[1];
            delta->vy.w    = dy;
            dz             = target->coord.t[2] - coord->coord.t[2];
            delta->vz.w    = dz;
            damage         = Gp_ComputeDamage((s32)work->field_154.hit.id.w, SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
            amount         = damage;
            if (damage == 0) {
                damage = 1;
                amount = 1;
            }
            func_800DA6E8(&arg0->field_20->node, amount, 0);
            func_800E2C78(arg0->field_20, (s32)work->field_154.hit.id.w, damage, 0);
            arg0->field_20->field_40 = 0;
            func_800FDB18(Gp_GetIdParam1((s32)work->field_154.hit.id.w) & 0xFFFF, arg0->field_2C->field_8, 0, &work->field_224);
            break;
    }
    Gp_ClearRec18Occupied(&work->field_154.rec);
    SCRATCH_SP += 0x10;
}

void Actor00700_Fn0268C(Actor00700* arg0)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  coord2;
    SVECTOR*        sc;
    s32             direction;
    s32             direction2;
    s32             product;
    sc   = (SVECTOR*)(SCRATCH_SP -= 8);
    work = arg0->field_1C;
    if (++work->field_2E0 >= 16) {
        work->field_2E0 = 0;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_2D4 = !(((u32)Gp_LcgState >> 16) & 1);
    }
    switch (work->field_2D4) {
        case 0:
            work->field_2D8 += 0x100;
            if (work->field_2D8 >= 0x200) {
                work->field_2D8 = -0x100;
                direction       = work->field_2D6;
                work->field_2D6 = -direction;
            }
            break;
        case 1:
            work->field_2D8 = 0x100;
            direction2      = work->field_2D6;
            work->field_2D6 = -direction2;
            break;
    }
    sc->vx = 0;
    sc->vy = 0;
    sc->vz = work->field_2D8 * work->field_2D6;
    coord  = arg0->field_2C->field_8;
    RotMatrix(sc, &coord[2].coord);
    coord[2].flg = 0;
    sc->vx       = 0;
    sc->vy       = 0;
    product      = work->field_2D8 * work->field_2D6;
    sc->vz       = -product;
    coord2       = arg0->field_2C->field_8;
    RotMatrix(sc, &coord2[3].coord);
    coord2[3].flg = 0;
    SCRATCH_SP   += 8;
}

void Actor00700_Fn02820(Actor00700* arg0)
{
    Actor00700Work*       work;
    GsCOORDINATE2*        coord;
    Actor00700RotScratch* sc;
    s32                   random;
    s32                   amount;
    s32                   cur;
    s32                   cur2;
    s32                   cur3;
    s32                   random2;
    s32                   amount2;
    u16                   want;
    s16                   diff;
    s32                   adiff;
    s16                   turn;
    s16                   wrap;

    sc    = (Actor00700RotScratch*)(SCRATCH_SP -= 0x18);
    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    switch (work->field_2E6) {
        case 0:
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            random          = (u32)Gp_LcgState >> 16;
            amount          = random & 0x1F;
            cur             = work->field_2DC;
            work->field_2DC = !(random & 0x20) ? cur - amount : cur + amount;
            break;
        case 1:
            sc->vec.vx = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
            sc->vec.vy = 0;
            sc->vec.vz = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
            want       = ratan2((s16)sc->vec.vx, (s16)sc->vec.vz) & 0xFFF;
            diff       = want - (work->field_2DC & 0xFFF);
            adiff      = diff >= 0 ? diff : -diff;
            turn       = diff;
            if (adiff < 0x11) {
                work->field_2DC = want;
            } else {
                if (adiff >= 0x801) {
                    wrap = diff - 0x1000;
                    if (diff <= 0)
                        wrap = 0x1000 - diff;
                    turn = wrap;
                }
                cur2 = work->field_2DC;
                if (turn > 0) {
                    work->field_2DC = cur2 + 0x10;
                } else {
                    work->field_2DC = cur2 - 0x10;
                }
            }
            break;
    }
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    random2         = (u32)Gp_LcgState >> 16;
    amount2         = random2 & 0x3F;
    cur3            = work->field_2DA;
    work->field_2DA = !(random2 & 0x40) ? cur3 - amount2 : cur3 + amount2;
    if (work->field_2DA > 0x100) {
        work->field_2DA = 0x100;
    } else if (work->field_2DA < -0x100) {
        work->field_2DA = -0x100;
    }
    sc->rot.vx = work->field_2DA;
    sc->rot.vy = work->field_2DC;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}

void Actor00700_Fn02A28(Actor00700* arg0)
{
    Actor00700Work* work;
    GsCOORDINATE2*  coord;
    u32             random;
    u32             random2;
    u32             random3;
    s32             amount;
    s32             amountB;
    s16             delta;
    s16             speed;
    s32             y;
    s32             newY;
    s16             base;

    work            = arg0->field_1C;
    coord           = arg0->field_2C->field_8;
    work->field_2BC = coord->coord.t[0];
    work->field_2C0 = coord->coord.t[1];
    work->field_2C4 = coord->coord.t[2];
    switch (work->field_2E6) {
        case 0:
            random = (u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            amount = random & 0x1F;
            if (!(random & 0x20)) {
                amount = -amount;
            }
            delta = amount;
            if ((s16)(coord->coord.t[0] + (s16)delta) < work->field_2AC + 200 &&
                work->field_2AC - 200 < (s16)(coord->coord.t[0] + (s16)delta)) {
                coord->coord.t[0] += (s16)delta;
            } else {
                coord->coord.t[0] -= (s16)delta;
            }
            amountB = ((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x1F;
            if (work->field_2D4 != 0) {
                amountB = -amountB;
            }
            delta = amountB;
            if ((s16)(coord->coord.t[1] + (s16)delta) < work->field_2B0 + 500 &&
                work->field_2B0 - 500 < (s16)(coord->coord.t[1] + (s16)delta)) {
                coord->coord.t[1] += (s16)delta;
            } else {
                coord->coord.t[1] -= (s16)delta;
            }
            random3 = (u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            amount  = random3 & 0x1F;
            if (!(random3 & 0x20)) {
                amount = -amount;
            }
            delta = amount;
            if ((s16)random3 < work->field_2B4 + 200 && work->field_2B4 - 200 < (s16)random3) {
                coord->coord.t[2] += (s16)delta;
            } else {
                coord->coord.t[2] -= (s16)delta;
            }
            break;
        case 1:
            speed = Actor00700_D07598[arg0->field_20->field_3C->field_F] +
                    (((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x1F);
            coord->coord.t[0] += (coord->coord.m[0][2] * speed) >> 12;
            coord->coord.t[2] += (coord->coord.m[2][2] * speed) >> 12;
            base               = D_80073B8C->t[1] - 0x4B0;
            random2            = (u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            y                  = coord->coord.t[1];
            if (y >= base + 400) {
                coord->coord.t[1] = y - (random2 & 0xF);
            } else {
                if (base - 400 >= y) {
                    newY = y + (random2 & 0xF);
                } else {
                    amountB = random2 & 0x1F;
                    if (work->field_2D4 != 0) {
                        newY = y - amountB;
                    } else {
                        newY = y + amountB;
                    }
                }
                coord->coord.t[1] = newY;
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn02D28);

void Actor00700_Fn0305C(Actor00700* arg0)
{
    Actor00700QuadScratch* sc;
    Actor00700Work*        work;
    Actor00700Obj2C*       obj;
    GsCOORDINATE2*         coord;
    s32                    size, x, y;
    s16                    i;
    SVECTOR*               v;
    POLY_FT4*              prim;
    Actor00700TexEntry*    uv;
    obj         = arg0->field_2C;
    sc          = (Actor00700QuadScratch*)(SCRATCH_SP -= 0x28);
    coord       = obj->field_8;
    work        = arg0->field_1C;
    sc->v[0].vx = coord->workm.t[0];
    sc->v[0].vy = coord->workm.t[1];
    sc->v[0].vz = coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->v[0]);
    __asm__ volatile("nop; nop; .word 0x4A180001");
    gte_stsxy(&sc->sxy);
    gte_stszotz(&sc->otz);
    if (sc->otz < 20) {
        SCRATCH_SP += 0x28;
        return;
    }
    if (work->field_2E0 == 1) {
        sc->v[0].vx = 0;
        sc->v[0].vy = 0;
        sc->v[0].vz = ((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFFF;
        RotMatrix(&sc->v[0], &work->field_22C.quad.rotation);
    }
    size        = 0x7800 / sc->otz;
    x           = sc->sxy & 0xFFFF;
    y           = sc->sxy >> 16;
    sc->v[0].vx = -size;
    sc->v[0].vy = -size;
    sc->v[0].vz = 0;
    sc->v[1].vx = size;
    sc->v[1].vy = -size;
    sc->v[1].vz = 0;
    sc->v[2].vx = -size;
    sc->v[2].vy = size;
    sc->v[2].vz = 0;
    sc->v[3].vx = size;
    sc->v[3].vy = size;
    sc->v[3].vz = 0;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&work->field_22C.quad.rotation);
        v = &sc->v[i];
        gte_ldv0(v);
        __asm__ volatile("nop; nop; .word 0x4A486012");
        gte_stsv(v);
        v->vx += x;
        v->vy += y;
    }
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2E);
    setRGB0(prim, 0x80, 0x80, 0x80);
    setShadeTex(prim, 1);
    prim->tpage = (((obj->field_24 * 64 + 0x180) & 0x3FF) >> 6) | 0xD0;
    prim->clut  = (obj->field_25 << 6) + 0x3D40;
    uv          = &Actor00700_D075BC[(s16)(work->field_2E0 / 3)];
    prim->u0    = uv->u;
    prim->v0    = uv->v;
    prim->u1    = uv->u + 31;
    prim->v1    = uv->v;
    prim->u2    = uv->u;
    prim->v2    = uv->v + 31;
    prim->u3    = uv->u + 31;
    prim->v3    = uv->v + 31;
    prim->x0    = sc->v[0].vx;
    prim->y0    = sc->v[0].vy;
    prim->x1    = sc->v[1].vx;
    prim->y1    = sc->v[1].vy;
    prim->x2    = sc->v[2].vx;
    prim->y2    = sc->v[2].vy;
    prim->x3    = sc->v[3].vx;
    prim->y3    = sc->v[3].vy;
    addPrim((u_long*)(((((u32)sc->otz << Display_State.field_128) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt), prim);
    SCRATCH_SP += 0x28;
}

void Actor00700_Fn034BC(Actor00700* arg0)
{
    Actor00700StateFuncTable3 sp;

    sp = Actor00700_D00054;
    sp.funcs[arg0->field_30](arg0->field_20, arg0);
}

void Actor00700_Fn03518(Actor00700* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}

void Actor00700_Fn03570(Actor00700* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor00700Work*             work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->field_1C;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = arg0->field_2C->field_8;
    if (work->field_2E2 >= 0x201) {
        work->field_2E2 = (u16)work->field_2E2 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_2E2;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_22C.matrix;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    *(u8**)0x1F8003FC += 0x30;
}
