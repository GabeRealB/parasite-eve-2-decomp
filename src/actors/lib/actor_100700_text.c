#include "common.h"

#include "actors/actor_100700.h"
#include "main/session.h"
#include "main/sound.h"

#define SCRATCH_SP (*(u32*)0x1F8003FC)

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn00060);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn00334);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01148);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01830);

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Actor00700_Fn00334(Actor00700* arg0);
void Actor00700_Fn01AB8(Actor00700* arg0);
void Actor00700_Fn01988(Actor00700* arg0, Actor00700Obj2C* arg1, s32 arg2);
void Actor00700_Fn01CF0(Actor00700* arg0);
void Actor00700_Fn01D80(Actor00700* arg0);
void Actor00700_Fn01E44(Actor00700* arg0);
void Actor00700_Fn01E9C(Actor00700* arg0);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01988);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01B50);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01CF0);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01E44);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01E9C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01EEC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01FE0);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn02414);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn0268C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn02820);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn02A28);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn0305C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn034BC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn03518);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn03570);
