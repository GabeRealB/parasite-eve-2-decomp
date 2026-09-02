#include "common.h"

#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank", D_shelter_b2_septic_tank_8017D5C0);

extern GpMsgEntry D_shelter_b2_septic_tank_80182F4C[];
extern TaskDesc   D_shelter_b2_septic_tank_801832C0[];

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank", func_shelter_b2_septic_tank_8017D614);

s32 func_shelter_b2_septic_tank_8017D7AC(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank", func_shelter_b2_septic_tank_8017D7B4);

s32 func_shelter_b2_septic_tank_8017D904(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank", func_shelter_b2_septic_tank_8017D90C);

void func_shelter_b2_septic_tank_8017D97C(s32 arg0)
{
    GameFlag_SetNibble(0xEB, arg0);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank", func_shelter_b2_septic_tank_8017D9A0);

void func_shelter_b2_septic_tank_8017DA18(Task* arg0)
{
    arg0->field_24 = D_shelter_b2_septic_tank_80182F4C;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_shelter_b2_septic_tank_801832C0, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank", func_shelter_b2_septic_tank_8017DA74);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank", func_shelter_b2_septic_tank_8017DB10);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank", func_shelter_b2_septic_tank_8017DB68);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank", func_shelter_b2_septic_tank_8017E2DC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank", func_shelter_b2_septic_tank_8017EA50);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank", func_shelter_b2_septic_tank_8017EAB8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank", func_shelter_b2_septic_tank_8017EAF8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank", func_shelter_b2_septic_tank_8017EB7C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_septic_tank/shelter_b2_septic_tank", func_shelter_b2_septic_tank_8017F040);
