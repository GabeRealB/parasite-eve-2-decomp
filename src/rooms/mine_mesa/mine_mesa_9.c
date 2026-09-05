#include "common.h"

#include "main/task.h"

extern Task* RoomsShared8018459cTask;
extern Task* D_mine_mesa_80189B58;
extern Task* RoomsShared8017e8a8Task;

void func_mine_mesa_8017EB38(void)
{
    RoomsShared8018459cTask = NULL;
    D_mine_mesa_80189B58    = NULL;
    RoomsShared8017e8a8Task = NULL;
}

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_9", func_mine_mesa_8017EB54);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_9", func_mine_mesa_8017ED08);

INCLUDE_RODATA("rooms/nonmatchings/mine_mesa/mine_mesa_9", D_mine_mesa_8017D624);
