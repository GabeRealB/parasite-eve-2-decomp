#include "common.h"

#include "actors/actor_401300.h"

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801323B0);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80132554);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013267C);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80132910);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80132BE4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80132C78);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80132FF4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80133254);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80133324);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013346C);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80133834);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80133A3C);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80134454);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80134BA4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80134F90);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80135DDC);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80135FC4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80136238);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801365F8);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80136CE8);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801376E4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80137D78);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80138160);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80138800);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80138B24);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80138CF8);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80138FCC);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80139134);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80139520);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801397F8);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80139AB0);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013A208);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013A5C0);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013AAE8);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013AE48);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013B6E8);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013BB30);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013CBAC);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013D2AC);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013D6C4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013DADC);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013E930);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013F628);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80140300);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8014046C);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801405DC);

void func_actor_401300_8014148C(void)
{
}

s32 func_actor_401300_80141494(Actor401300* arg0, s32 arg1, Actor401300Msg* arg2)
{
    Actor401300Work* work = arg0->field_1C;

    switch (arg2->field_4) {
        case 0:
            work->field_8A2 = 0x22;
            break;
        case 1:
            work->field_8A2 = 0x23;
            break;
        case 2:
            work->field_8A2 = 0x24;
            break;
        case 3:
            work->field_8A2 = 0x25;
            break;
        case 4:
            work->field_8A2 = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

/* Closes this unit's .rodata after the 5-entry jump table above, so
   actor_401300_3's tables start at 0x80132044. Nothing reads it. */
const u32 D_actor_401300_80132040 = 0;
