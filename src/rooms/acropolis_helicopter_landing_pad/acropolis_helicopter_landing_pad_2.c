#include "common.h"
#include "main/task.h"
extern TaskDesc D_acropolis_helicopter_landing_pad_80184DA0;

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_2", func_acropolis_helicopter_landing_pad_8017E4A4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_2", func_acropolis_helicopter_landing_pad_8017E570);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_2", func_acropolis_helicopter_landing_pad_8017E5B8);

void func_acropolis_helicopter_landing_pad_8017E5E8(void)
{
    Task_SpawnFromTable(&D_acropolis_helicopter_landing_pad_80184DA0, 1, 0, 0);
}

void func_acropolis_helicopter_landing_pad_8017E618(s32 arg0, s32 arg1)
{
    Task_SpawnFromTable(&D_acropolis_helicopter_landing_pad_80184DA0, 2, 0, arg0 | (arg1 << 8));
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_2", func_acropolis_helicopter_landing_pad_8017E64C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_2", func_acropolis_helicopter_landing_pad_8017E67C);

void func_acropolis_helicopter_landing_pad_8017E6C0(s32 arg0)
{
    Task_SpawnFromTable(&D_acropolis_helicopter_landing_pad_80184DA0, 6, arg0, 0);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_2", func_acropolis_helicopter_landing_pad_8017E6F0);
