#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"

#include "main/task.h"

extern TaskDesc D_shelter_b1_armory_801824E8[];

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017DDD8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017E264);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017E3B0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", D_shelter_b1_armory_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017E77C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017E9CC);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", D_shelter_b1_armory_8017D6D8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017EBEC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017ED94);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017EF78);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017F18C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017F30C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017F5D4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017F7A8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017FC28);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017FCE4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017FE0C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017FF40);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_801800A4);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_80180214);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8018034C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_80180468);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_801805A8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_80180698);

/// Handler for slot-7 msg `0x13EF`: the directed action (`field_2` 1) that
/// spawns the armory script.
s32 func_shelter_b1_armory_801806F8(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 1) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(D_shelter_b1_armory_801824E8, 1, 0, 0);
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", jtbl_shelter_b1_armory_8017D738);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory", func_shelter_b1_armory_8017D768);
