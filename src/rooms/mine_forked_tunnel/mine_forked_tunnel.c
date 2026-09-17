#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

#include "rooms/mine_forked_tunnel.h"

extern void func_mine_forked_tunnel_8017DC50(Task* arg0);
extern void func_mine_forked_tunnel_8017DC70(Task* arg0);
extern s32  func_mine_forked_tunnel_8017DD08(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
extern void func_mine_forked_tunnel_8017DF34(s32 arg0);

void func_mine_forked_tunnel_8017D5E8(Task* arg0)
{
    MineForkedTunnelWork* work;
    RoomPlacement         placement;

    work = Mem_Calloc(0x48, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->idMap    = work;
    work->field_44 = -1;

    if (GameFlag_GetNibble(0x75) == 0) {
        placement.pos.vx = D_mine_forked_tunnel_80181244.vx;
        placement.pos.vy = D_mine_forked_tunnel_80181244.vy;
        placement.pos.vz = D_mine_forked_tunnel_80181244.vz;
        placement.rot.vx = D_mine_forked_tunnel_80180AC4.vx;
        placement.rot.vy = D_mine_forked_tunnel_80180AC4.vy;
        placement.rot.vz = D_mine_forked_tunnel_80180AC4.vz;
        Room_Util18(arg0, 0x7D4, &placement, 0);
    } else {
        Room_Util18(arg0, 0x7D4, &D_mine_forked_tunnel_80181BBC, 0);
    }

    func_mine_forked_tunnel_8017DD08(arg0, 0x7D5, 1, 0);
    func_mine_forked_tunnel_8017DC70(arg0);
    work->field_40 = Task_SpawnFromTable(&D_mine_forked_tunnel_80181B74, 1, 0, (s32)arg0);
    arg0->field_24 = &D_mine_forked_tunnel_80181B8C;
    func_mine_forked_tunnel_8017DF34(GameFlag_GetNibble(0x75));
    arg0->exitCallback = func_mine_forked_tunnel_8017DC50;
    arg0->state++;
}

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

INCLUDE_RODATA("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel", RoomsShared8017d878Table);
