#include "common.h"
#include "actors/actor_202600.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/wipsys.h"

INCLUDE_RODATA("actors/nonmatchings/actor_202600/actor_202600", D_actor_202600_80149E20);

INCLUDE_ASM("actors/nonmatchings/actor_202600/actor_202600", func_actor_202600_80149E8C);

INCLUDE_ASM("actors/nonmatchings/actor_202600/actor_202600", func_actor_202600_8014A574);

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
    if ((distance < 0x9C4) && (difference < 0x80)) {
        work->field_39A = 5;
        work->field_39C = 0;
        work->field_392 = 4;
        Gp_ArmStateF0(1);
    }
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_202600/actor_202600", func_actor_202600_8014A8B4);

INCLUDE_ASM("actors/nonmatchings/actor_202600/actor_202600", func_actor_202600_8014ADC0);

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

INCLUDE_ASM("actors/nonmatchings/actor_202600/actor_202600", func_actor_202600_8014B25C);

INCLUDE_RODATA("actors/nonmatchings/actor_202600/actor_202600", D_actor_202600_80149E4C);
INCLUDE_RODATA("actors/nonmatchings/actor_202600/actor_202600", D_actor_202600_80149E58);
