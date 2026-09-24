#include "common.h"

#include "main/task.h"
#include "rooms/mine_mesa.h"

void func_mine_mesa_8017E650(void)
{
    D_mine_mesa_80189B54 = Task_SpawnFromTable(&D_mine_mesa_801842F4, 1, 0, 0);
}

/// Hands `arg0` to the task in `D_mine_mesa_80189B54` as its `spawnArg1` when
/// it is 0 or 1; any other value kills the task and clears the handle.
void func_mine_mesa_8017E684(s32 arg0)
{
    Task* t = D_mine_mesa_80189B54;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1 = arg0;
    return;
kill:
    taskKill(D_mine_mesa_80189B54);
    D_mine_mesa_80189B54 = NULL;
}
