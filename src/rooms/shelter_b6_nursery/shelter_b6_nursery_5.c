#include "common.h"
#include "main/task.h"
extern TaskDesc       D_shelter_b6_nursery_80185000;
extern s32            D_shelter_b6_nursery_8018797C;
extern TaskFuncTable3 RoomsShared8017d878Table;

void func_shelter_b6_nursery_8017FFF4(void)
{
    if (D_shelter_b6_nursery_8018797C == 0) {
        D_shelter_b6_nursery_8018797C = 1;
        Task_SpawnFromTable(&D_shelter_b6_nursery_80185000, 0, 0, 0);
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_5", func_shelter_b6_nursery_80180038);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_5", func_shelter_b6_nursery_801800A0);
