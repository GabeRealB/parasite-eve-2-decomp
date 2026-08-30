#include "common.h"

#include "gameplay/3CD8.h"
#include "main/task.h"

/// `Task_SpawnFromTable(D_..._80184DA0, 2, 0, arg0 | (arg1 << 8))` — spawns a
/// room script task, packing the two ids into the spawn argument.
extern Task* func_acropolis_helicopter_landing_pad_8017E618(s32 arg0, s32 arg1);

/// `Gp_SpawnScript18` argument pairs for the landing-pad cutscene beats.
extern s32 D_acropolis_helicopter_landing_pad_80187D40;
extern s32 D_acropolis_helicopter_landing_pad_80187D48;
extern s32 D_acropolis_helicopter_landing_pad_80187D50;
extern s32 D_acropolis_helicopter_landing_pad_80187D60;
extern s32 D_acropolis_helicopter_landing_pad_80187D68;
extern s32 D_acropolis_helicopter_landing_pad_80187D78;

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D658);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D6E0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D7B0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D824);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D8E8);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", D_acropolis_helicopter_landing_pad_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D964);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D9BC);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", D_acropolis_helicopter_landing_pad_8017D5E4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017DA9C);

void func_acropolis_helicopter_landing_pad_8017DE78(Task* task)
{
    switch (task->state) {
        case 0xC8:
        case 0x0:
            func_acropolis_helicopter_landing_pad_8017E618(0xF, 2);
            break;
        case 0x7:
        case 0xCF:
            Gp_SpawnScript18((s32)&D_acropolis_helicopter_landing_pad_80187D68,
                             (s32)&D_acropolis_helicopter_landing_pad_80187D78);
            break;
        case 0x19A:
            func_acropolis_helicopter_landing_pad_8017E618(0x16, 3);
            break;
        case 0x1A5:
            Gp_SpawnScript18((s32)&D_acropolis_helicopter_landing_pad_80187D50,
                             (s32)&D_acropolis_helicopter_landing_pad_80187D60);
            break;
        case 0x208:
            func_acropolis_helicopter_landing_pad_8017E618(0x1E, 4);
            break;
        case 0x217:
            Gp_SpawnScript18((s32)&D_acropolis_helicopter_landing_pad_80187D40,
                             (s32)&D_acropolis_helicopter_landing_pad_80187D48);
            break;
        case 0x280:
            Task_Kill(task);
            break;
    }
    task->state++;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017DFCC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E0F8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E270);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E3F0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E49C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E4A4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E570);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E5B8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E5E8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E618);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E64C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E67C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E6C0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E6F0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E724);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E75C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E76C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E81C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E974);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017EA6C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017EB00);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017EB58);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017ED00);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017ED50);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017EDD4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017EE2C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017EE80);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017EEDC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017EF60);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017EF8C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017F010);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017FA30);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_801802E0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_80180664);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_80180A64);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_80180E40);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_80181064);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_801818F0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_801819C0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_80181B64);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_801822B0);
