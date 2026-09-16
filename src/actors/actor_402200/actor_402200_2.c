#include "common.h"
#include "actors/actor_402200.h"
#include "main/sound.h"

void func_actor_402200_801324E8(Actor402200* arg0, s32 arg1)
{
    GpEnemy*         enemy = arg0->field_20;
    s16              hp    = enemy->field_40;
    Actor402200Work* work  = arg0->field_1C;
    u32              state = 0;
    s32              max;

    if (hp <= 0) {
        state = 6;
        if (work->field_6F0 == 0) {
            state = 5;
        }
        if (work->field_6B8 != 0) {
            SndEvt_EnqueueType7(work->field_6B8, 1);
            work->field_6B8 = 0;
        }
        if (work->field_6BC != 0) {
            SndEvt_EnqueueType7(work->field_6BC, 1);
            work->field_6BC = 0;
        }
    } else if (max = enemy->field_50->field_4, hp < max / 10) {
        state = 4;
        if (work->field_6F0 == 0) {
            state = 3;
        }
    } else if (work->field_6F2 == 0 || work->field_6EC != 0) {
        work->field_6F2 = 0;
        state           = 2;
        if (arg1 < 0x50) {
            state = 1;
        }
    }

    switch (state) {
        case 0:
            break;
        case 1:
            work->field_6CC  = 5;
            work->field_6CE  = 0;
            work->field_582 &= 0x7FFF;
            break;
        case 2:
            work->field_6CC  = 6;
            work->field_6CE  = 0;
            work->field_582 &= 0x7FFF;
            break;
        case 3:
            work->field_6CC  = 7;
            work->field_6CE  = 0;
            work->field_582 &= 0x7FFF;
            break;
        case 4:
            if (work->field_6F2 == 0) {
                work->field_6CC = 8;
                work->field_6CE = 0;
            }
            break;
        case 5:
            work->field_6CC  = 9;
            work->field_6CE  = 0;
            work->field_582 &= 0x7FFF;
            break;
        case 6:
            if (work->field_6F2 == 0) {
                work->field_6CC = 10;
                work->field_6CE = 0;
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_2", func_actor_402200_80132688);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_2", func_actor_402200_801329A4);

INCLUDE_RODATA("actors/nonmatchings/actor_402200/actor_402200_2", jtbl_actor_402200_80131E84);

INCLUDE_RODATA("actors/nonmatchings/actor_402200/actor_402200_2", jtbl_actor_402200_80131EA4);

INCLUDE_RODATA("actors/nonmatchings/actor_402200/actor_402200_2", jtbl_actor_402200_80131EC4);

INCLUDE_RODATA("actors/nonmatchings/actor_402200/actor_402200_2", jtbl_actor_402200_80131EDC);
