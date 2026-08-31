#include "common.h"
#include "main/task.h"
extern TaskDesc D_mine_mesa_801842F4;
extern Task*    D_mine_mesa_80189B54;

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E620);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E650);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E684);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E6D8);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E70C);

void func_mine_mesa_8017E760(void)
{
    if (D_mine_mesa_80189B54 != NULL) {
        Task_Kill(D_mine_mesa_80189B54);
    }
    D_mine_mesa_80189B54 = Task_SpawnFromTable(&D_mine_mesa_801842F4, 3, 0, 0);
}

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E7B0);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E8B0);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E8FC);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E91C);
