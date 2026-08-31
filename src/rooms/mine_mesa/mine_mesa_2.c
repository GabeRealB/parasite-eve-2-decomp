#include "common.h"
#include "main/task.h"
extern TaskDesc D_mine_mesa_80189B2C;
extern Task*    D_mine_mesa_80189B4C;

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017D8F8);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017DA7C);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017DABC);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017DBC4);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017DC80);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017DD44);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017DD98);

void func_mine_mesa_8017DDF0(void)
{
    if (D_mine_mesa_80189B4C == NULL) {
        D_mine_mesa_80189B4C = Task_SpawnFromTable(&D_mine_mesa_80189B2C, 0, 0, 0);
    }
}
