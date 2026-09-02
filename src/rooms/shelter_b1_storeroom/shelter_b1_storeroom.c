#include "common.h"

#include "main/session.h"
#include "main/task.h"

extern u8  D_80115598;
extern s32 D_shelter_b1_storeroom_80184968;

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_storeroom/shelter_b1_storeroom", func_shelter_b1_storeroom_8017D604);

s32 func_shelter_b1_storeroom_8017D6E0(void)
{
    return 0;
}

s32 func_shelter_b1_storeroom_8017D6E8(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_storeroom/shelter_b1_storeroom", func_shelter_b1_storeroom_8017D6F0);

void func_shelter_b1_storeroom_8017D740(Task* arg0)
{
    arg0->field_24 = &D_shelter_b1_storeroom_80184968;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

void func_shelter_b1_storeroom_8017D78C(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_storeroom/shelter_b1_storeroom", D_shelter_b1_storeroom_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_storeroom/shelter_b1_storeroom", func_shelter_b1_storeroom_8017D794);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_storeroom/shelter_b1_storeroom", func_shelter_b1_storeroom_8017D7EC);
