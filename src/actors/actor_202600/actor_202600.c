#include "common.h"
#include "actors/actor_202600.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/sound.h"

INCLUDE_RODATA("actors/nonmatchings/actor_202600/actor_202600", D_actor_202600_80149E20);

INCLUDE_ASM("actors/nonmatchings/actor_202600/actor_202600", func_actor_202600_80149E8C);

INCLUDE_ASM("actors/nonmatchings/actor_202600/actor_202600", func_actor_202600_8014A574);

INCLUDE_ASM("actors/nonmatchings/actor_202600/actor_202600", func_actor_202600_8014A734);

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
