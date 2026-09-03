#include "common.h"

#include "main/sound.h"

s32 func_acropolis_hallway_8017D734(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) { /* irregular */
        case 6:
            SndEvt_EnqueueType6(0x51070006, 0, 0);
            break;
        case 7:
            SndEvt_EnqueueType6(0x51070007, 0, 0);
            break;
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_hallway/acropolis_hallway_2", func_acropolis_hallway_8017D784);

void func_acropolis_hallway_8017D7C8(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_hallway/acropolis_hallway_2", func_acropolis_hallway_8017D7D0);

void func_acropolis_hallway_8017D828(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_hallway/acropolis_hallway_2", func_acropolis_hallway_8017D830);

INCLUDE_ASM("rooms/nonmatchings/acropolis_hallway/acropolis_hallway_2", func_acropolis_hallway_8017D9D4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_hallway/acropolis_hallway_2", func_acropolis_hallway_8017E120);
