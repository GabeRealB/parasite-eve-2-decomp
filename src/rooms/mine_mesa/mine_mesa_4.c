#include "common.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"

extern s8       D_8007106B;
extern TaskDesc D_mine_mesa_80181990;

void func_mine_mesa_8017E024(Task* arg0)
{
    Display_SpawnWithOt(&D_mine_mesa_80181990, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E074);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E15C);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E2A4);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E3E0);
