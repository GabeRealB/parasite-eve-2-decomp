#include "common.h"

#include "main/task.h"

extern Task*    RoomsShared8017e8a8Task;
extern TaskDesc RoomsShared8017daf0Desc;
extern Task*    D_mine_mesa_80189B54;

void func_mine_mesa_8017E650(void)
{
    D_mine_mesa_80189B54 = Task_SpawnFromTable(&RoomsShared8017daf0Desc, 1, 0, 0);
}

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_5", func_mine_mesa_8017E684);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_5", func_mine_mesa_8017E6D8);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_5", func_mine_mesa_8017E70C);

void func_mine_mesa_8017E760(void)
{
    if (D_mine_mesa_80189B54 != NULL) {
        Task_Kill(D_mine_mesa_80189B54);
    }
    D_mine_mesa_80189B54 = Task_SpawnFromTable(&RoomsShared8017daf0Desc, 3, 0, 0);
}

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_5", func_mine_mesa_8017E7B0);

void func_mine_mesa_8017E8B0(s32 arg0)
{
    RoomsShared8017e8a8Task = Task_SpawnFromTable(&RoomsShared8017daf0Desc, 4, arg0, 0);
    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
}
