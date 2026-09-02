#include "common.h"

#include "main/session.h"

typedef struct {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  field_30;
} GarbageIncineratorState;

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017E70C);

extern void func_shelter_b3_garbage_incinerator_80185220(void);

void func_shelter_b3_garbage_incinerator_8017E7A4(GarbageIncineratorState* arg0)
{
    func_shelter_b3_garbage_incinerator_80185220();
    arg0->field_30 = 5;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017E7D0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017F0A8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017F318);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017F410);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017F588);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017F6D8);

void func_shelter_b3_garbage_incinerator_8017F8A4(GarbageIncineratorState* arg0, s32 arg1, s32 arg2)
{
    arg0->field_30 = arg2;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017F8AC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017F930);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017F968);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017F9B4);

void func_shelter_b3_garbage_incinerator_8017FA3C(void)
{
    D_80073BA0              = 0;
    Game_Session->field_128 = 3;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017FA58);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017FB80);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017FC5C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", RoomsShared801830f0Sub);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_8017FE74);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_801808A8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_80180994);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_80180B18);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_80180C28);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_80180D44);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_80180E0C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_80180EC4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_80180F18);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_2", func_shelter_b3_garbage_incinerator_80180F54);
