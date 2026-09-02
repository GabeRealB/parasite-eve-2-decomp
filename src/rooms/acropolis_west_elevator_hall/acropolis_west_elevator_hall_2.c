#include "common.h"

#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_acropolis_west_elevator_hall_801849CC[];
extern TaskDesc   D_acropolis_west_elevator_hall_80184568[];
extern Task*      D_acropolis_west_elevator_hall_80186AE4[];

void func_acropolis_west_elevator_hall_8017F568(Task* arg0)
{
    arg0->field_24 = D_acropolis_west_elevator_hall_801849CC;
    Game_SetPtrSlot(arg0, 7);
    D_acropolis_west_elevator_hall_80186AE4[0] =
        Task_SpawnFromTable(D_acropolis_west_elevator_hall_80184568, 0, 0, -1);
    D_acropolis_west_elevator_hall_80186AE4[1] =
        Task_SpawnFromTable(D_acropolis_west_elevator_hall_80184568, 1, 0, 1);
    arg0->state = (s32)(arg0->state + 1);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall_2", func_acropolis_west_elevator_hall_8017F5F4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall_2", func_acropolis_west_elevator_hall_8017F64C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall_2", func_acropolis_west_elevator_hall_8017F6F0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall_2", func_acropolis_west_elevator_hall_8017F7D4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall_2", func_acropolis_west_elevator_hall_8017F990);

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall_2", func_acropolis_west_elevator_hall_8017FAE8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall_2", func_acropolis_west_elevator_hall_8017FE18);

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall_2", func_acropolis_west_elevator_hall_8017FFE4);

s32 func_acropolis_west_elevator_hall_80180274(void)
{
    Gp_SpawnEff(0x60033, NULL, 0, NULL);
    return 0;
}
