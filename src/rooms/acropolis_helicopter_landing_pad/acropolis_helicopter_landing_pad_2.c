#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/task.h"

void Gp_ReleaseStateF0Add(void* arg0, s32 arg1);

extern s8 D_801153F1;

extern TaskDesc D_acropolis_helicopter_landing_pad_80184DA0;
extern s32      D_acropolis_helicopter_landing_pad_80184E50;
extern s32      D_acropolis_helicopter_landing_pad_80187F84;

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_2", func_acropolis_helicopter_landing_pad_8017E4A4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_2", func_acropolis_helicopter_landing_pad_8017E570);

void func_acropolis_helicopter_landing_pad_8017E5B8(void)
{
    Task_SpawnFromTable(&D_acropolis_helicopter_landing_pad_80184DA0, 3, 0, 0);
}

void func_acropolis_helicopter_landing_pad_8017E5E8(void)
{
    Task_SpawnFromTable(&D_acropolis_helicopter_landing_pad_80184DA0, 1, 0, 0);
}

void func_acropolis_helicopter_landing_pad_8017E618(s32 arg0, s32 arg1)
{
    Task_SpawnFromTable(&D_acropolis_helicopter_landing_pad_80184DA0, 2, 0, arg0 | (arg1 << 8));
}

void func_acropolis_helicopter_landing_pad_8017E64C(void)
{
    Task_SpawnFromTable(&D_acropolis_helicopter_landing_pad_80184DA0, 5, 0, 0);
}

void func_acropolis_helicopter_landing_pad_8017E67C(void)
{
    if (D_acropolis_helicopter_landing_pad_80187F84 != 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E9, (s32)&D_acropolis_helicopter_landing_pad_80184E50, 0);
    }
}

void func_acropolis_helicopter_landing_pad_8017E6C0(s32 arg0)
{
    Task_SpawnFromTable(&D_acropolis_helicopter_landing_pad_80184DA0, 6, arg0, 0);
}

void func_acropolis_helicopter_landing_pad_8017E6F0(void)
{
    Gp_ReleaseStateF0Add((void*)Gp_LookupSlot4(0), 0x1B);
    D_801153F1 = 3;
}
