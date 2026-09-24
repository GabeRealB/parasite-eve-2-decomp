#include "common.h"

#include "main/task.h"
#include "rooms/mist_r18.h"

/// Spawn entry 4 of the room's task table and keep its handle in
/// `D_mist_r18_80186E98`, which `func_mist_r18_8017EA60` kills.
void func_mist_r18_8017EA2C(void)
{
    D_mist_r18_80186E98 = Task_SpawnFromTable(&D_mist_r18_80184F04, 4, 0, 0);
}
