#include "common.h"
#include "main/task.h"
#include "gameplay/3A34.h"
extern TaskDesc D_mine_mesa_80189B2C;
extern Task*    D_mine_mesa_80189B4C;
extern GpObj4A  D_mine_mesa_801890EC[4];

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017D8F8);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017DA7C);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017DABC);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017DBC4);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_2", func_mine_mesa_8017DC80);

void func_mine_mesa_8017DD44(void)
{
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[0]);
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[1]);
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[2]);
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[3]);
}
