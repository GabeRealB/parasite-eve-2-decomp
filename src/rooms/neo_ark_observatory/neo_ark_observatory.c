#include "common.h"

#include "gameplay/3CD8.h"
#include "main/task.h"

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory", func_neo_ark_observatory_8017D6F4);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory", func_neo_ark_observatory_8017D8A8);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory", D_neo_ark_observatory_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory", func_neo_ark_observatory_8017F22C);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory", func_neo_ark_observatory_8017F3FC);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory", func_neo_ark_observatory_8017F44C);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory", func_neo_ark_observatory_8017F588);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory", func_neo_ark_observatory_8017F6F8);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory", func_neo_ark_observatory_8017FA98);

void func_neo_ark_observatory_8017FB1C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x300, 0);
            Gp_SpawnIfCapIdle(task->spawnArg1, 0);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            task->state++;
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            Task_Kill(task);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory", D_neo_ark_observatory_8017D698);
