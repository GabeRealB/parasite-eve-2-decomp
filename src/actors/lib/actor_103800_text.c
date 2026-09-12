#include "common.h"

#include "actors/actor_103800.h"
#include "actors/actors_shared_80135b58.h"
#include "gameplay/3CD8.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "main/wipsys.h"

void Actor03800_Fn00974(Actor103800* arg0);
void Actor03800_Fn00A98(Actor103800* arg0);
void Actor03800_Fn026F8(Actor103800* arg0);
void Actor03800_Fn02848(Actor103800* arg0);
void Actor03800_Fn02E50(Actor103800* arg0);
void Actor03800_Fn03594(Actor103800* arg0);
void Actor03800_Fn03628(Actor103800* arg0);
void Actor03800_Fn036EC(Actor103800* arg0);
void Actor03800_Fn03744(Actor103800* arg0);
void Actor03800_Fn037E0(Actor103800* arg0);
void Gp_ArmStateF0(s32 arg0);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
s32  SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern u8  D_801153F2;
extern u8  D_801153F4;
extern s32 Gp_LcgState;

void              Actor03800_Fn003B8(Task* arg0);
extern GpPairSrcE Actor03800_D05F44;
extern u8         Actor03800_D05F60[];

/// State 12 of `Actor03800_Fn032D8`: pick and hold a turn direction while the
/// actor is aiming at the player. State 0 chooses the side to turn towards -
/// clockwise (1) when the player is in front of the actor's local +Z axis,
/// anticlockwise (2) otherwise - and a pending `field_370` request forces the
/// clockwise side. States 1 and 2 drive `field_35C` by -/+0x7D while the yaw
/// error `field_34C` is inside 8..0x10 and hand over to state 3 once it grows
/// past 0x10; state 3 finishes the move and hands back to `field_352` 1.
void Actor03800_Fn02584(Actor103800* arg0)
{
    Actor103800Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    work  = arg0->field_1C;
    coord = work->field_344;

    switch (work->field_354) {
        case 0:
            vec.vx          = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
            vec.vy          = 0;
            vec.vz          = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
            work->field_348 = 0xA;
            if (work->field_370 != 0) {
                work->field_370 = 0;
                work->field_354 = 1;
            } else {
                work->field_354 =
                    ((vec.vx * coord->coord.m[0][2]) + (vec.vz * coord->coord.m[2][2]) > 0) ? 1 : 2;
            }
            work->field_2AA &= 0x7FFF;
            break;

        case 1:
            if (work->field_34C >= 8 && work->field_34C <= 0x10) {
                work->field_35C = -0x7D;
                break;
            }
            goto turn_done;

        case 2:
            if (work->field_34C >= 8 && work->field_34C <= 0x10) {
                work->field_35C = 0x7D;
                break;
            }
        turn_done:
            work->field_35C = 0;
            if ((s16)work->field_34C >= 0x11) {
                work->field_354 = 3;
            }
            break;

        case 3:
            if ((s16)work->field_34C >= 0x14) {
                work->field_352  = 1;
                work->field_354  = 0;
                work->field_372  = 0x80;
                work->field_2AA |= 0x8000;
            }
            break;
    }
}

void Actor03800_Fn026F8(Actor103800* arg0)
{
    Actor103800Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR*         rot;
    s32              ang;
    u16              want;
    s16              diff;
    s32              adiff;
    s32              step;
    s32              cur;
    s32              next;
    s32              wrapStep;

    rot   = (SVECTOR*)(SCRATCH_SP -= 8);
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_364;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_362 = ang;
    if (adiff < 0x800) {
        step = work->field_360;
        if (step >= adiff) {
            work->field_362 = want;
        } else {
            next = work->field_362;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_362 = next;
        }
    } else {
        step = work->field_360;
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
        work->field_362 = work->field_364;
        goto done;
    turn:
        wrapStep = work->field_360;
        cur      = work->field_362;
        if (diff > 0) {
            work->field_362 = cur - wrapStep;
        } else {
            work->field_362 = cur + wrapStep;
        }
    }
done:
    rot->vx = 0;
    rot->vy = work->field_362;
    rot->vz = 0;
    RotMatrix(rot, &coord->coord);
    SCRATCH_SP += 8;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn02848);

void Actor03800_Fn02998(Actor103800Ctx* arg0, Actor103800* arg1)
{
    Actor103800Work*  work;
    Actor103800Obj2C* obj;
    GsCOORDINATE2*    coord;
    Actor103800Work*  work2;
    GsCOORDINATE2*    c;
    VECTOR            vec;
    s32               state;
    s32               i;
    s16               st;
    s16               phase;
    s16               anim;
    s16               val;
    s32               snd;
    s32               pan;

    obj   = arg1->field_2C;
    work  = arg1->field_1C;
    state = D_801153F4;
    coord = work->field_344;
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
    st = work->field_354;
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
    if (st == 3) {
        goto case3;
    }
    return;
death:
    if (work->field_378 == 0) {
        anim = 1;
        if (work->field_36E != 0) {
            anim = 5;
        }
        work->field_348 = anim;
    }
    work->field_356 = 0;
    work->field_35A = 0x1000;
    work->field_2CC = coord->coord;
    arg0->field_54  = 0;
    Gp_UnlinkNode((GpLinkNode*)&arg0->node);
    Gp_UnlinkObj((GpObj*)work->field_1A4);
    Gp_UnlinkObj((GpObj*)work->field_20C);
    Gp_UnlinkObj((GpObj*)work->field_28C);
    Gp_SetLightMode((GpObj4C*)arg0, 1);
    Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0x26);
    work->field_354 = 1;
    if (work->field_368 != 0) {
        obj->field_C    = 0x80;
        work->field_354 = 3;
    }
    work2 = arg1->field_1C;
    i     = 1;
    if ((s16)work2->field_348 != work2->field_34A) {
        work2->field_34A = work2->field_348;
        work2->field_34C = 0;
        val              = Actor03800_D05F90[(s16)work2->field_348];
        do {
            func_800B4114(work2, i, (s16)work2->field_348, 0, val);
            i++;
        } while (i < 6);
    } else {
        TOUCH_REG(i);
        work2->field_34C += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work2, i);
            i++;
        } while (i < 6);
    }
    c      = arg1->field_1C->field_344;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg1->field_20, &vec, 0, 0);
    snd = ((arg0->field_8 >> 12) << 8) | 0x40260004;
    pan = (s8)Gp_GetObjPan((GpObj38*)coord);
    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
    return;
dying:
    Actor03800_Fn037E0(arg1);
    phase           = work->field_356 + 1;
    work->field_356 = phase;
    if (phase == 10) {
        obj->field_C = 2;
    }
    if (work->field_356 == 15) {
        Gp_SpawnEff(0x600A5, coord, 2, NULL);
    }
    if (work->field_356 >= 0x3C) {
        work->field_354 = 2;
        obj->field_C    = 0x80;
    }
    work2 = arg1->field_1C;
    i     = 1;
    if ((s16)work2->field_348 != work2->field_34A) {
        work2->field_34A = work2->field_348;
        work2->field_34C = 0;
        val              = Actor03800_D05F90[(s16)work2->field_348];
        do {
            func_800B4114(work2, i, (s16)work2->field_348, 0, val);
            i++;
        } while (i < 6);
    } else {
        TOUCH_REG(i);
        work2->field_34C += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work2, i);
            i++;
        } while (i < 6);
    }
    c      = arg1->field_1C->field_344;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg1->field_20, &vec, 0, 0);
    return;
destroy:
    Gp_DestroyEnemy((GpEnemy*)arg0, (Task*)arg1);
    return;
case3:
    if (work->field_368 == 0) {
        goto timer;
    }
    if (work->field_368 < 2) {
        goto inc368;
    }
    work->field_368 = 0;
    Tmd_FreeBuffers((TmdObject*)obj);
    obj->field_C |= 4;
    Actor03800_Fn02E50(arg1);
    goto timer;
inc368:
    work->field_368++;
timer:
    phase           = work->field_356 + 1;
    work->field_356 = phase;
    if (phase < 0x3C) {
        return;
    }
    work->field_354 = 2;
}

void Actor03800_Fn02E50(Actor103800* actor)
{
    s16  variants[4];
    s32  selected;
    s16* choice;
    s16* variantBase;
    s16* firstOut;
    s16* nextOut;
    s32  i;
    s32  thirdProduct;
    s32  secondIndex;
    s32  remaining;
    s32  firstProduct;
    u32  thirdRandom;
    u32  firstVariant;
    u32  firstRandom;
    u32  secondRandom;

    firstProduct = Gp_LcgState * 5;
    firstRandom  = firstProduct + 0x71357911;
    Gp_LcgState  = (s32)firstRandom;
    firstVariant = ((firstRandom >> 16) % 5) & 0xFFFF;
    Actor03800_Fn03008(actor, firstVariant);
    firstOut = &variants[0];
    if ((u16)Gp_StateF0.field_6 < 2U) {
        remaining = Actor03800_D05FA8[(u16)Gp_StateF0.field_6];
        i         = 0;
        do {
            if (i != firstVariant) {
                *firstOut = i;
                firstOut++;
            }
            i += 1;
        } while (i < 5);
        secondRandom = (Gp_LcgState * 5) + 0x71357911;
        secondIndex  = (secondRandom >> 0x10) & 3;
        SOFT_TOUCH_REG(secondIndex);
        variantBase = variants;
        choice      = variantBase;
        choice     += secondIndex;
        selected    = *choice;
        Gp_LcgState = (s32)secondRandom;
        Actor03800_Fn03008(actor, (u32)selected);
        remaining--;
        if (remaining > 0) {
            nextOut = variantBase;
            i       = 0;
            do {
                if ((i != firstVariant) && (i != secondIndex)) {
                    *nextOut = i;
                    nextOut++;
                }
                i += 1;
            } while (i < 5);
            thirdProduct = Gp_LcgState * 5;
            thirdRandom  = thirdProduct + 0x71357911;
            Gp_LcgState  = (s32)thirdRandom;
            Actor03800_Fn03008(actor, (u32)variants[((thirdRandom >> 16) % 3) & 0xFFFF]);
        }
    }
}

void Actor03800_Fn03008(Actor103800* actor, u32 variant)
{
    GpAreaKey  key;
    GpAreaKey* sessionKey;
    u8         areaByte0;
    GpAreaRec* rec;
    GpCdRec10* entry;
    GpEffWork* eff;
    TmdObject* model;
    s32        idx;
    u32        raw;

    switch (variant) {
        case 0:
            D_80067704[0] = Actor03800_D0459C;
            break;
        case 1:
            D_80067704[0] = Actor03800_D046A0;
            break;
        case 2:
            D_80067704[0] = Actor03800_D047A4;
            break;
        case 3:
            D_80067704[0] = Actor03800_D04868;
            break;
        case 4:
            D_80067704[0] = Actor03800_D0492C;
            break;
    }
    eff = Gp_SpawnEff(0x40007, actor->field_2C->field_8 + 3, 0x100, NULL);
    if (eff == NULL) {
        return;
    }
    sessionKey  = (GpAreaKey*)&Game_Session->field_4;
    raw         = actor->field_20->field_8;
    model       = (TmdObject*)eff->field_0->extra;
    key.field_3 = sessionKey->field_3;
    key.field_2 = sessionKey->field_2;
    key.field_1 = sessionKey->field_1;
    areaByte0   = sessionKey->field_0;
    idx         = raw >> 12;
    key.field_0 = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    rec = Gp_GetNestedAreaRec(&key);

    entry           = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
    model->field_24 = entry->field_D;
    model->field_25 = entry->field_E;
    if (model->field_18 != NULL) {
        Tmd_ProcessStream(model);
        Tmd_ProcessStream(model);
    }
}

void Actor03800_Fn0315C(Actor103800* arg0)
{
    Actor103800StateFuncTable3 sp;

    sp = Actor03800_D00004;
    sp.funcs[arg0->field_30](arg0->field_20, arg0);
}

void Actor03800_Fn031B8(Actor103800Ctx* arg0, Actor103800* arg1)
{
    Actor103800Work* work;
    s32              state;
    s32              one;

    state = D_801153F4;
    one   = 1;
    work  = arg1->field_1C;
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
    arg1->field_2C->field_C = 0;
    arg0->node.field_4      = 0;
    goto default_body;
case2:
    arg1->field_2C->field_C = 0x80;
    arg0->node.field_4      = one;
    return;
default_body:
    if (arg0->field_4C != 0) {
        Actor03800_Fn00974(arg1);
    }
    Actor03800_Fn00A98(arg1);
    Actor03800_Fn032D8(arg1);
    if (work->field_360 != 0) {
        Actor03800_Fn026F8(arg1);
    }
    Actor03800_Fn02848(arg1);
    if (work->field_36A != 0) {
        Actor03800_Fn03594(arg1);
    }
    Actor03800_Fn03628(arg1);
    work->field_344->flg = 0;
    Gp_UpdateCoord(work->field_344);
case1:
    Actor03800_Fn036EC(arg1);
    Actor03800_Fn03744(arg1);
}

void Actor03800_Fn01150(Actor103800* arg0);
void Actor03800_Fn012B4(Actor103800* arg0);
void Actor03800_Fn0166C(Actor103800* arg0);
void Actor03800_Fn03420(Actor103800* arg0);
void Actor03800_Fn01AD0(Actor103800* arg0);
void Actor03800_Fn01C50(Actor103800* arg0);
void Actor03800_Fn021E4(Actor103800* arg0);
void Actor03800_Fn034B0(Actor103800* arg0);

void Actor03800_Fn032D8(Actor103800* arg0)
{
    Actor103800Work* work;
    s16              state;
    s16              mag;

    work  = arg0->field_1C;
    state = work->field_352;
    switch (state) {
        case 0:
            Actor03800_Fn01150(arg0);
            break;
        case 1:
            Actor03800_Fn012B4(arg0);
            break;
        case 2:
            Actor03800_Fn01520(arg0);
            break;
        case 3:
            Actor03800_Fn0166C(arg0);
            break;
        case 4:
            Actor03800_Fn03420(arg0);
            break;
        case 5:
            Actor03800_Fn01948(arg0);
            break;
        case 6:
            Actor03800_Fn01AD0(arg0);
            break;
        case 7:
            Actor03800_Fn01C50(arg0);
            break;
        case 8:
            Actor03800_Fn01EEC(arg0);
            break;
        case 9:
            Actor03800_Fn02068(arg0);
            break;
        case 10:
            Actor03800_Fn021E4(arg0);
            break;
        case 11:
            Actor03800_Fn034B0(arg0);
            break;
        case 12:
            Actor03800_Fn02584(arg0);
            break;
    }
    if (work->field_36E == 0) {
        work->field_21E = -0xFA;
        mag             = 0xFA;
    } else {
        work->field_21E = -0x15E;
        mag             = 0x15E;
    }
    work->field_228 = mag;
}

void Actor03800_Fn03420(Actor103800* arg0)
{
    Actor103800Work* work = arg0->field_1C;
    s32              state;

    state = work->field_354;
    switch (state) {
        case 0:
            work->field_348 = 4;
            work->field_354 = 1;
            break;
        case 1:
            if ((s16)work->field_34C == 0x1E) {
                work->field_36E = 0;
            }
            if ((s16)work->field_34C >= 0x3A) {
                work->field_352 = 2;
                work->field_354 = 0;
                if (work->field_36A == 0) {
                    work->field_36A = state;
                }
                work->field_2AA |= 0x8000;
            }
            break;
    }
}

void Actor03800_Fn034B0(Actor103800* arg0)
{
    Actor103800Obj2C* obj;
    Actor103800Ctx*   ctx;
    Actor103800Work*  work;

    work = arg0->field_1C;
    obj  = arg0->field_2C;
    ctx  = arg0->field_20;
    switch (D_80115410) {
        case 0:
            obj->field_C      = 0x84;
            ctx->node.field_4 = 1;
            return;
        case 1:
            obj->field_C     = 0;
            work->field_1C2 |= 0x8000;
            work->field_22A |= 0x4200;
            work->field_2AA |= 0x8000;
            Gp_ArmStateF0(1);
            work->field_366 = 0x80;
            work->field_356 = 0x5A;
            work->field_350 = 0;
            work->field_372 = 0x80;
            return;
        case 2:
            if (--work->field_356 <= 0) {
                work->field_352 = 1;
                work->field_354 = 0;
            }
            return;
    }
}

void Actor03800_Fn03594(Actor103800* arg0)
{
    Actor103800Work* work;
    GsCOORDINATE2*   coord;
    s32              soundId;
    s32              pan;

    work  = arg0->field_1C;
    coord = work->field_344;
    if (--work->field_36A <= 0) {
        work->field_36A = 0xC;
        soundId         = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40260001;
        pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
}

void Actor03800_Fn03628(Actor103800* arg0)
{
    Actor103800Work* work2;
    s32              i;
    s32              val;

    work2 = arg0->field_1C;
    i     = 1;
    if ((s16)work2->field_348 != work2->field_34A) {
        work2->field_34A = work2->field_348;
        work2->field_34C = 0;
        val              = Actor03800_D05F90[(s16)work2->field_348];
        do {
            func_800B4114(work2, i, (s16)work2->field_348, 0, val);
            i++;
        } while (i < 6);
    } else {
        TOUCH_REG(i);
        work2->field_34C += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work2, i);
            i++;
        } while (i < 6);
    }
}

void Actor03800_Fn036EC(Actor103800* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = arg0->field_1C->field_344;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg0->field_20, &vec, 0, 0);
}

void Actor03800_Fn03744(Actor103800* arg0)
{
    Actor103800Work* work;
    GsCOORDINATE2*   coord;
    VECTOR3          vec;
    s16              hit;

    work  = arg0->field_1C;
    coord = work->field_344;
    if (work->field_350 == 0) {
        vec.vx = coord->workm.t[0];
        vec.vy = coord->workm.t[1];
        vec.vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(&vec, 0x1F4, work->field_372);
        return;
    }
    hit = func_800EA1A8((VECTOR3*)coord->workm.t, &vec);
    if (hit != 0) {
        Gp_DrawEffGroundQuad(&vec, 0x200, func_800EA318(0x200, 0x80, hit));
    }
}

void Actor03800_Fn037E0(Actor103800* arg0)
{
    Actor103800Work*            work;
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;

    work                = arg0->field_1C;
    head                = *(MATRIX**)0x1F8003FC;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = work->field_344;
    if (work->field_35A >= 0x201) {
        work->field_35A = (u16)work->field_35A - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_35A;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_2CC;
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
