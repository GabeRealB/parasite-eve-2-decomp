#include "common.h"

#include "actors/actor_105500.h"
#include "actors/actors_shared_80135b58.h"
#include "actors/actors_shared_80135c4c.h"

#include "main/mem.h"
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
void Actor05500_Fn02954(Actor105500* arg0, s16 arg1);
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

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn0006C);

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
            scratchEnd[-1].vx = (s32)(Wip_SysConfig.field_4->t[0] - coord->coord.t[0]);
            delta->vy         = 0;
            dz                = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
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
                index           = ((Actor105500Ctx*)arg0->field_20)->field_3C->field_F;
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
    scratchEnd[-1].vx                  = (s32)(Wip_SysConfig.field_4->t[0] - coord->coord.t[0]);
    delta->vy                          = 0;
    dz                                 = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
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

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn00A94);

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
                work->field_39E = Actor05500_D08990[((Actor105500Ctx*)arg0->field_20)->field_3C->field_F] + ((random >> 0x10) & 0x3FF);
                Gp_LcgState     = random;
                return;
            }
            return;
        case 1:
            scratchEnd[-1].vec.vx = (s32)(Wip_SysConfig.field_4->t[0] - coord->coord.t[0]);
            delta->vec.vy         = 0;
            delta->vec.vz         = (s32)(Wip_SysConfig.field_4->t[2] - coord->coord.t[2]);
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
                work->field_39E = Actor05500_D08980[((Actor105500Ctx*)arg0->field_20)->field_3C->field_F] + ((random2 >> 0x10) & 0xF);
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
                scratchEnd[-1].vec.vx = (s32)(Wip_SysConfig.field_4->t[0] - coord->coord.t[0]);
                delta->vec.vy         = 0;
                dz                    = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
                delta->vec.vz         = dz;
                dx                    = scratchEnd[-1].vec.vx;
                distance              = SquareRoot0((dx * dx) + (dz * dz));
                if ((work->field_3C0 == 0) && (distance < 0x578) && (work->field_3B0 == 0) && !(Wip_SysConfig.field_25 & 1)) {
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
        work->field_39E = Actor05500_D08980[((Actor105500Ctx*)arg0->field_20)->field_3C->field_F] + ((random >> 0x10) & 0xF);
        Gp_LcgState     = random;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn0143C);

#include "actors_shared_fn01a0c.c"

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn01B30);

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
            contact = work->rec.field_4;
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

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02954);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02C94);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02FFC);

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
