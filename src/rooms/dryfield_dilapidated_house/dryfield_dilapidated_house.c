#include "common.h"

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/task.h"

extern s32   D_dryfield_dilapidated_house_80189B6C;
extern Task* D_dryfield_dilapidated_house_80189B78;

extern void func_800B0928(Task* task, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

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

void func_dryfield_dilapidated_house_8017E6DC(Task* arg0)
{
    Task* temp_s1;
    s32   temp_a1;
    s32   temp_v1;

    temp_s1 = Game_GetPtrSlot(3);
    temp_a1 = Gp_LookupSlot4(1);
    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            arg0->spawnArg1 = 0;
            arg0->state    += 1;
            return;
        case 2:
            func_800B0928(temp_s1, temp_a1, 0x200, 0x180, 0x1000);
            /* fallthrough */
        case 1:
            return;
    }
}

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
