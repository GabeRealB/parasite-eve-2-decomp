#include "common.h"

#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_shelter_b2_main_corridor_80182C14[];
extern TaskDesc   D_shelter_b2_main_corridor_80182DE0[];

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor_2", func_shelter_b2_main_corridor_8017E264);

void func_shelter_b2_main_corridor_8017E2D4(Task* arg0)
{
    arg0->field_24 = D_shelter_b2_main_corridor_80182C14;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_shelter_b2_main_corridor_80182DE0, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_shelter_b2_main_corridor_8017E330(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor_2", func_shelter_b2_main_corridor_8017E338);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor_2", func_shelter_b2_main_corridor_8017E390);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor_2", func_shelter_b2_main_corridor_8017EB8C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor_2", func_shelter_b2_main_corridor_8017EBF4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor_2", func_shelter_b2_main_corridor_8017EC34);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor_2", func_shelter_b2_main_corridor_8017EF24);
