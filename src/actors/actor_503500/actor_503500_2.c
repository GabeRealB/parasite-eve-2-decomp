#include "common.h"

#include "main/task.h"

#include "main/session.h"

#include "actors/actor_503500.h"

extern Task* D_actor_503500_80176558;

extern TaskDesc D_actor_503500_8014B964;

void func_actor_503500_80132BF8(void)
{
    Mc_SaveData.field_6 = 0x16;
    Mc_SaveData.field_8 = 1;
    Mc_SaveData.field_5 = 1;
    Task_Spawn(0, 0x11, 0, 0);
}

void func_actor_503500_80132C40(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_503500_8014B964, 0, arg0, 0);
}

void func_actor_503500_80132C70(s32 arg0)
{
    D_actor_503500_80176558 = Task_SpawnFromTable(&D_actor_503500_8014B964, 1, arg0, 0);
}
