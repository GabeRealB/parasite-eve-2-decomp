#include "common.h"
#include "main/mc.h"

s32 func_mine_tunnel_entrance_8017D634(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/mine_tunnel_entrance/mine_tunnel_entrance", func_mine_tunnel_entrance_8017D63C);

INCLUDE_ASM("rooms/nonmatchings/mine_tunnel_entrance/mine_tunnel_entrance", func_mine_tunnel_entrance_8017D644);

void func_mine_tunnel_entrance_8017D690(void)
{
    if (Mc_SaveData.field_5C5 == 9) {
        Mc_SaveData.field_5C5 = 0xA;
    }
}

void func_mine_tunnel_entrance_8017D6B4(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/mine_tunnel_entrance/mine_tunnel_entrance", D_mine_tunnel_entrance_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/mine_tunnel_entrance/mine_tunnel_entrance", func_mine_tunnel_entrance_8017D6BC);

INCLUDE_ASM("rooms/nonmatchings/mine_tunnel_entrance/mine_tunnel_entrance", func_mine_tunnel_entrance_8017D720);

INCLUDE_RODATA("rooms/nonmatchings/mine_tunnel_entrance/mine_tunnel_entrance", D_mine_tunnel_entrance_8017D5E8);
