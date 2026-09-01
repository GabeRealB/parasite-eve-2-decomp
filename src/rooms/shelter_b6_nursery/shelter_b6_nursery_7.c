#include "common.h"
#include "main/task.h"
extern TaskDesc D_shelter_b6_nursery_80185000;
extern s32      D_shelter_b6_nursery_8018797C;

extern TaskFuncTable3 D_shelter_b6_nursery_8017D6A4;

void func_shelter_b6_nursery_8017FF9C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b6_nursery_8017D6A4;
    sp.funcs[task->state](task);
}

void func_shelter_b6_nursery_8017FFF4(void)
{
    if (D_shelter_b6_nursery_8018797C == 0) {
        D_shelter_b6_nursery_8018797C = 1;
        Task_SpawnFromTable(&D_shelter_b6_nursery_80185000, 0, 0, 0);
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_7", func_shelter_b6_nursery_80180038);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_7", func_shelter_b6_nursery_801800A0);
