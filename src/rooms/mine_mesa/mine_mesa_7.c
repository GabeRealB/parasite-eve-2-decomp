#include "common.h"
#include "main/task.h"

extern Task* D_mine_mesa_80189B54;
extern Task* D_mine_mesa_80189B58;
extern Task* D_mine_mesa_80189B5C;

void func_mine_mesa_8017EB38(void)
{
    D_mine_mesa_80189B54 = NULL;
    D_mine_mesa_80189B58 = NULL;
    D_mine_mesa_80189B5C = NULL;
}

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_7", func_mine_mesa_8017EB54);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_7", func_mine_mesa_8017ED08);

INCLUDE_RODATA("rooms/nonmatchings/mine_mesa/mine_mesa_7", D_mine_mesa_8017D624);
