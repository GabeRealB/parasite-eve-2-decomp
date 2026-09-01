#include "common.h"

#include "main/session.h"
#include "main/task.h"

void func_mist_r18_8017DF80(s32 arg0);
void func_mist_r18_8017E144(s16 arg0);

/// Spawn descriptor handed to entry 5 of `D_mist_r18_80184F04`.
extern s32      D_mist_r18_80184EE4;
extern TaskDesc D_mist_r18_80184F04;

void func_mist_r18_8017E39C(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E3A4);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E448);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E534);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E654);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E6D8);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E784);

void func_mist_r18_8017E7F0(void)
{
    Task_SpawnFromTable(&D_mist_r18_80184F04, 5, 0, (s32)&D_mist_r18_80184EE4);
}

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E824);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E854);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E8B8);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_2", func_mist_r18_8017E92C);
