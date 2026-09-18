#include "common.h"
#include "actors/actor_202600.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/wipsys.h"

INCLUDE_RODATA("actors/nonmatchings/actor_202600/actor_202600", D_actor_202600_80149E20);

INCLUDE_ASM("actors/nonmatchings/actor_202600/actor_202600", func_actor_202600_80149E8C);

void func_actor_202600_8014A574(Actor202600* arg0)
{
    Actor202600Work* work;
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
            scratchEnd[-1].vx = (s32)(Player_Status.field_4->t[0] - coord->coord.t[0]);
            delta->vy         = 0;
            dz                = Player_Status.field_4->t[2] - coord->coord.t[2];
            delta->vz         = dz;
            dx                = scratchEnd[-1].vx;
            if ((SquareRoot0((dx * dx) + (dz * dz)) < 0x9C4) || (work->field_3D0 != 0) || (D_801153F2[1] == 2)) {
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
                index           = arg0->field_20->field_3C->field_F;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_39E = D_actor_202600_80152798[index] + ((random >> 0x10) & 0xF);
            }
            break;
    }
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) + 1;
}

void func_actor_202600_8014A734(Actor202600* arg0)
{
    Actor202600Work* work;
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
    scratchEnd[-1].vx                  = (s32)(Player_Status.field_4->t[0] - coord->coord.t[0]);
    delta->vy                          = 0;
    dz                                 = Player_Status.field_4->t[2] - coord->coord.t[2];
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
    if ((distance < 0x9C4) && (difference < 0x80)) {
        work->field_39A = 5;
        work->field_39C = 0;
        work->field_392 = 4;
        Gp_ArmStateF0(1);
    }
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) + 1;
}

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void ActorsSharedFn02c94(Actor202600* arg0);

void func_actor_202600_8014A8B4(Actor202600* arg0)
{
    register Actor202600* actor asm("s5") = arg0;
    Actor202600Work*      work;
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
    sessionFlags                       = *(s32*)&Game_Session->field_4;
    value                              = 0;
    switch (state) {
        case 0:
            if (work->field_3C6 == 0) {
                scratchEnd[-1].vx = (s32)(Player_Status.field_4->t[0] - coord->coord.t[0]);
                delta->vy         = 0;
                dz                = Player_Status.field_4->t[2] - coord->coord.t[2];
                delta->vz         = dz;
                dx                = scratchEnd[-1].vx;
                if (SquareRoot0((dx * dx) + (dz * dz)) < 0x7D0) {
                    value = 1;
                }
            }
            if ((value != 0) || ((s8)Gp_StateF0.field_22 != 0) || (Gp_StateF0.field_8 != 0)) {
                if (work->field_3C6 == 0) {
                    D_80115412 = 1;
                }
                Gp_ArmStateF0(1);
                work->field_39C          = 1;
                work->field_392          = 7;
                work->field_3A8          = D_actor_202600_801527B8[actor->field_20->field_3C->field_F];
                work->field_2E4.field_8  = coord;
                work->field_2E4.field_1C = 0x12C;
                work->field_2E4.field_12 = -0x12C;
                work->field_2E4.field_18 = Gp_PackPair(&ActorsShared80135c4cPair, 5);
                work->field_2E4.flags   |= 0x8000;
                if ((sessionFlags & 0xFFFF0000) == 0x05200000) {
                    value = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x55200006;
                    pan0  = (s8)Gp_GetObjPan((GpObj38*)coord);
                    SndEvt_EnqueueType6(value, (s32)pan0, (s8)Gp_GetObjDepth((GpObj38*)coord));
                }
            } else if (work->field_3D0 != 0) {
                if (work->field_3C6 == 0) {
                    Gp_StateF0.field_22 = 1;
                }
                Gp_ArmStateF0(1);
                work->field_39C          = 2;
                work->field_392          = 9;
                work->field_3A8          = D_actor_202600_801527B8[actor->field_20->field_3C->field_F];
                work->field_3BC          = 0x2D;
                work->field_2E4.field_1C = 0x12C;
                work->field_2E4.field_8  = coord;
                work->field_2E4.field_12 = -0x12C;
                work->field_2E4.field_18 = Gp_PackPair(&ActorsShared80135c4cPair, 5);
                work->field_2E4.flags   |= 0x8000;
                if ((sessionFlags & 0xFFFF0000) == 0x05200000) {
                    value = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x55200006;
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
                value                  = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x401A0002;
                pan2                   = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(value, (s32)pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            break;
        case 3:
            if ((s16)work->field_396 >= 0x1E) {
                Gp_ArmStateF0(1);
                work->field_39A          = state;
                work->field_39C          = 0;
                work->field_392          = 1;
                index                    = actor->field_20->field_3C->field_F;
                work->field_39E          = D_actor_202600_80152798[index] + (((random = (Gp_LcgState * 5) + 0x71357911) >> 0x10) & 0xF);
                work->field_2E4.field_8  = actor->field_2C->field_8 + 4;
                work->field_2E4.field_1C = 0xC8;
                work->field_2E4.field_12 = 0;
                work->field_3C8          = 0;
                Gp_LcgState              = random;
                if (actor->field_20->field_40 <= 0) {
                    work->field_39A = 9;
                    work->field_39C = 0;
                    actor->field_30 = 2;
                }
            }
            break;
    }
    ActorsSharedFn02c94(actor);
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) + 1;
}

void func_actor_202600_8014ADC0(Actor202600* arg0)
{
    Actor202600Work*       work;
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
    Actor202600RotScratch* delta;
    Actor202600RotScratch* scratchEnd;

    scratchEnd                                        = *(Actor202600RotScratch**)PSX_SCRATCH_ADDR(0x3FC);
    delta                                             = scratchEnd - 1;
    *(Actor202600RotScratch**)PSX_SCRATCH_ADDR(0x3FC) = delta;
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
                work->field_39E = D_actor_202600_801527A8[arg0->field_20->field_3C->field_F] + ((random >> 0x10) & 0x3FF);
                Gp_LcgState     = random;
                return;
            }
            return;
        case 1:
            scratchEnd[-1].vec.vx = (s32)(Player_Status.field_4->t[0] - coord->coord.t[0]);
            delta->vec.vy         = 0;
            delta->vec.vz         = (s32)(Player_Status.field_4->t[2] - coord->coord.t[2]);
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
                work->field_39E = D_actor_202600_80152798[arg0->field_20->field_3C->field_F] + ((random2 >> 0x10) & 0xF);
                Gp_LcgState     = random2;
                return;
            }
            if ((s16)work->field_396 == 0xC) {
                sound = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x401A0001;
                pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if ((s16)work->field_396 >= 0x29) {
                work->field_396 = 0xB;
            }
            if (work->field_3A4 == work->field_3A2) {
                scratchEnd[-1].vec.vx = (s32)(Player_Status.field_4->t[0] - coord->coord.t[0]);
                delta->vec.vy         = 0;
                dz                    = Player_Status.field_4->t[2] - coord->coord.t[2];
                delta->vec.vz         = dz;
                dx                    = scratchEnd[-1].vec.vx;
                distance              = SquareRoot0((dx * dx) + (dz * dz));
                if ((work->field_3C0 == 0) && (distance < 0x578) && (work->field_3B0 == 0) && !(Player_Status.field_25 & 1)) {
                    work->field_39A = 4;
                    work->field_39C = 0;
                    work->field_392 = 3;
                    work->field_3AC = 0;
                } else if (distance < 0x9C4) {
                    work->field_39A = 5;
                    work->field_39C = 0;
                    work->field_392 = 4;
                }
            }
            break;
    }
}

void func_actor_202600_8014B108(Actor202600* arg0)
{
    Actor202600Work* work;
    GsCOORDINATE2*   coord;
    s32              sound;
    s32              pan;
    u32              random;

    work            = arg0->field_1C;
    coord           = arg0->field_2C->field_8;
    work->field_398 = 0;
    work->field_3A6 = 0;
    if ((s16)work->field_396 == 0x28) {
        sound = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x401A0003;
        pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
    if ((u32)(work->field_396 - 0x2B) < 7U) {
        Gp_SpawnEnemyFromTable(work->field_36C, 1, 0, (GpEnemy*)arg0->field_20);
        work->field_3AC = (u16)(work->field_3AC + 1);
    }
    if ((s16)work->field_396 >= (D_actor_202600_80152836 + 0x3C)) {
        work->field_39A = 3;
        work->field_39C = 0;
        work->field_392 = 1;
        random          = (Gp_LcgState * 5) + 0x71357911;
        work->field_39E = D_actor_202600_80152798[arg0->field_20->field_3C->field_F] + ((random >> 0x10) & 0xF);
        Gp_LcgState     = random;
    }
}

void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_202600_8014B25C(Actor202600* arg0)
{
    Actor202600Work*  work;
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
                work->field_2E4.field_18 = Gp_PackPair(&ActorsShared80135c4cPair, index);
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
            motion0 = D_actor_202600_80152850;
            for (; index < 9; index++, motion0++) {
                if ((s16)work->field_396 <= (motion0[0][0] + D_actor_202600_80152838)) {
                    coord->coord.t[0] += (s32)(motion0[0][1] * rsin((s32)work->field_3A2)) >> 0xC;
                    coord->coord.t[2] += (s32)(motion0[0][1] * rcos((s32)work->field_3A2)) >> 0xC;
                    break;
                }
            }
            if ((s16)work->field_396 == 0x28) {
                sound = (((u16)((Actor202600Ctx*)arg0->field_20)->field_8 >> 0xC) << 8) | 0x401A0002;
                pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
                work->field_3C8 = 0;
                if (((Actor202600Ctx*)arg0->field_20)->field_40 <= 0) {
                    work->field_39A = 9;
                    work->field_39C = 0;
                    arg0->field_30  = 2;
                }
            }
            if ((s16)work->field_396 >= (D_actor_202600_80152838 + 0x46)) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                random          = (Gp_LcgState * 5) + 0x71357911;
                work->field_39E = D_actor_202600_80152798[((Actor202600Ctx*)arg0->field_20)->field_3C->field_F] + ((random >> 0x10) & 0xF);
                Gp_LcgState     = random;
            }
            break;
        case 1:
            index           = 0;
            motion1         = D_actor_202600_80152874;
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
                sound = (((u16)((Actor202600Ctx*)arg0->field_20)->field_8 >> 0xC) << 8) | 0x401A0002;
                pan1  = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(sound, (s32)pan1, (s8)Gp_GetObjDepth((GpObj38*)coord));
                work->field_3C8 = 2;
                if (((Actor202600Ctx*)arg0->field_20)->field_40 <= 0) {
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
                work->field_39E = D_actor_202600_80152798[((Actor202600Ctx*)arg0->field_20)->field_3C->field_F] + (((Gp_LcgState = (Gp_LcgState * 5) + 0x71357911) >> 0x10) & 0xF);
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

INCLUDE_RODATA("actors/nonmatchings/actor_202600/actor_202600", D_actor_202600_80149E4C);
INCLUDE_RODATA("actors/nonmatchings/actor_202600/actor_202600", D_actor_202600_80149E58);
