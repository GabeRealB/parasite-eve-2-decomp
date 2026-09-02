#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

#include "rooms/mine_forked_tunnel.h"

INCLUDE_ASM("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel", func_mine_forked_tunnel_8017D5E8);

INCLUDE_ASM("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel", func_mine_forked_tunnel_8017D724);

INCLUDE_ASM("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel", func_mine_forked_tunnel_8017D8EC);

INCLUDE_ASM("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel", func_mine_forked_tunnel_8017DAB8);

INCLUDE_RODATA("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel", D_mine_forked_tunnel_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel", func_mine_forked_tunnel_8017DBE4);

void func_mine_forked_tunnel_8017DC50(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_mine_forked_tunnel_8017DC70(Task* arg0)
{
    TmdObject*            ext;
    MineForkedTunnelWork* work;

    ext           = arg0->extra;
    work          = (MineForkedTunnelWork*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

INCLUDE_RODATA("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel", D_mine_forked_tunnel_8017D5D0);

INCLUDE_RODATA("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel", D_mine_forked_tunnel_8017D5DC);
