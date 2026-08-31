#include "common.h"

#include "actors/actor_100700.h"
#include "main/session.h"
#include "main/sound.h"

#define SCRATCH_SP (*(u32*)0x1F8003FC)

void  Gp_ArmStateF0(s32 arg0);
s32   Gp_GetObjPan(void* arg0);
s32   Gp_GetObjDepth(void* arg0);
void  Gp_UpdateCoord(GsCOORDINATE2* arg0);
void  Gp_UnlinkNode(void* node);
void  Gp_UnlinkObj(void* node);
void  Gp_SetLightMode(void* arg0, s32 arg1);
void  Gp_ReleaseStateF0Add(void* arg0, s32 arg1);
void  Gp_UpdateActorColor(void* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void  Gp_DestroyEnemy(void* enemy, void* task);
void  func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void  Gp_AnimTickIndex(void* arg0, s32 arg1);
void* Gp_SpawnEff(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, void* arg3);
void  Actor00700_Fn01EEC(Actor00700* arg0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn00060);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L000C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00164);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00310);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn00334);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L003D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00410);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L0042C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00434);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00460);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00464);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00468);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L0049C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00580);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L005DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L005F4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L0063C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00654);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L0066C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00678);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L006AC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L006C8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00740);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L0077C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00780);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L007D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L0080C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00864);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn008B4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00900);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00910);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L009C8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00A44);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00A4C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00A58);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00A5C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00A80);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00AF0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00B04);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00BA0);

extern s32 Gp_LcgState;

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn00F20);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00F5C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L00F70);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01004);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01078);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L010A8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L010AC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L010F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L0110C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01138);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01148);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01190);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L011AC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L011CC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01200);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01210);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01258);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01284);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L012CC);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L019D0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01A0C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01A74);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01A7C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01A9C);

void Actor00700_Fn008B4(Actor00700* arg0);
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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01BFC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01C10);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01C48);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01CB4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01CDC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01CF0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01D80);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01DD4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01E00);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01E14);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01E2C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01E44);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01E9C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01EEC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L01F3C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn01FE0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02044);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02104);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02268);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02498);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L024D0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L024EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L024F4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02524);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02540);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L025F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L0264C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn0268C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02714);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02734);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02764);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L0276C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02774);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02778);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn02820);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02878);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L028B8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02924);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02938);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02954);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02958);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L0296C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02970);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02974);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L029AC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L029CC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L029D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L029D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn02A28);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02A70);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02AA4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02AE8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02AF8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02B30);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02B74);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02B84);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02BBC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02C00);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02C04);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02C18);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02CEC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02CF4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02D08);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02D20);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02D94);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02DD0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02DE0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02E74);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02F04);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02FA0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02FAC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L02FF4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L0301C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L03020);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L03024);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L03034);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn0305C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L03158);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L031A4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L031BC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L031D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L03220);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L034A0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn034BC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn03518);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_Fn03570);

INCLUDE_ASM("actors/nonmatchings/lib/actor_100700_text", Actor00700_L035C0);
