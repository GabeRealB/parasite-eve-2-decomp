#include "common.h"

#include "gameplay/3CD8.h"
#include "main/task.h"

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sleeping_quarters/shelter_b1_sleeping_quarters", func_shelter_b1_sleeping_quarters_8017D608);

s32 func_shelter_b1_sleeping_quarters_8017D668(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sleeping_quarters/shelter_b1_sleeping_quarters", func_shelter_b1_sleeping_quarters_8017D670);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sleeping_quarters/shelter_b1_sleeping_quarters", func_shelter_b1_sleeping_quarters_8017D6FC);

s32 func_shelter_b1_sleeping_quarters_8017D770(void)
{
    return 0;
}

void func_shelter_b1_sleeping_quarters_8017D778(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0);
            Gp_RunCapCmd(task->spawnArg1, 1);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            Task_Kill(task);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sleeping_quarters/shelter_b1_sleeping_quarters", func_shelter_b1_sleeping_quarters_8017D83C);

void func_shelter_b1_sleeping_quarters_8017D880(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sleeping_quarters/shelter_b1_sleeping_quarters", D_shelter_b1_sleeping_quarters_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sleeping_quarters/shelter_b1_sleeping_quarters", func_shelter_b1_sleeping_quarters_8017D888);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sleeping_quarters/shelter_b1_sleeping_quarters", func_shelter_b1_sleeping_quarters_8017D8E0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sleeping_quarters/shelter_b1_sleeping_quarters", jtbl_shelter_b1_sleeping_quarters_8017D5F4);
