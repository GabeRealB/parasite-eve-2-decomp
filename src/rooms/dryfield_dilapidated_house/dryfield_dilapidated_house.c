#include "common.h"
#include "main/task.h"
extern s32 D_dryfield_dilapidated_house_80189B6C;

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017D64C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017DE88);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D5C4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017E014);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017E144);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017E2B0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017E48C);

s32 func_dryfield_dilapidated_house_8017E56C(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017E574);

s32 func_dryfield_dilapidated_house_8017E684(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017E68C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017E6DC);

void func_dryfield_dilapidated_house_8017E780(Task* arg0)
{
    s32 temp_v0;
    s32 temp_v1;
    s32 var_a0;

    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            D_dryfield_dilapidated_house_80189B6C = arg0->spawnArg1;
            arg0->state                          += 1;
            return;
        case 1:
            var_a0 = (s32)(D_dryfield_dilapidated_house_80189B6C * 3) / (s32)arg0->spawnArg1;
            if (D_dryfield_dilapidated_house_80189B6C & 1) {
                var_a0 = -var_a0;
            }
            Display_ClampField126((s8)var_a0);
            temp_v0                               = D_dryfield_dilapidated_house_80189B6C - 1;
            D_dryfield_dilapidated_house_80189B6C = temp_v0;
            if (temp_v0 == 0) {
                Task_Kill(arg0);
            }
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017E858);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", func_dryfield_dilapidated_house_8017E8A8);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D61C);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D628);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D634);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house", D_dryfield_dilapidated_house_8017D640);
