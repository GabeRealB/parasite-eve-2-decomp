#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "rooms/mine_forked_tunnel.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

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

    arg0->work     = work;
    work->field_44 = -1;

    if (GameFlag_GetNibble(0x75) == 0) {
        placement.pos.vx = D_mine_forked_tunnel_80181244[0].vx;
        placement.pos.vy = D_mine_forked_tunnel_80181244[0].vy;
        placement.pos.vz = D_mine_forked_tunnel_80181244[0].vz;
        placement.rot.vx = D_mine_forked_tunnel_80180AC4[0].vx;
        placement.rot.vy = D_mine_forked_tunnel_80180AC4[0].vy;
        placement.rot.vz = D_mine_forked_tunnel_80180AC4[0].vz;
        Room_Util18(arg0, 0x7D4, &placement, 0);
    } else {
        Room_Util18(arg0, 0x7D4, &D_mine_forked_tunnel_80181BBC, 0);
    }

    func_mine_forked_tunnel_8017DD08(arg0, 0x7D5, 1, 0);
    func_mine_forked_tunnel_8017DC70(arg0);
    work->field_40 = Task_SpawnFromTable(&D_mine_forked_tunnel_80181B74, 1, 0, (s32)arg0);
    arg0->msgTable = &D_mine_forked_tunnel_80181B8C;
    func_mine_forked_tunnel_8017DF34(GameFlag_GetNibble(0x75));
    arg0->exitCallback = func_mine_forked_tunnel_8017DC50;
    arg0->state++;
}

void func_mine_forked_tunnel_8017D724(Task* arg0)
{
    TmdObject*    ext;
    RoomPlacement placement;
    VECTOR3       vec;

    ext = arg0->extra;

    if (arg0->spawnArg1 == 1 && arg0->killCountdown < 0x6E) {
        placement.pos.vx = D_mine_forked_tunnel_80181244[arg0->killCountdown].vx;
        placement.pos.vy = D_mine_forked_tunnel_80181244[arg0->killCountdown].vy;
        placement.pos.vz = D_mine_forked_tunnel_80181244[arg0->killCountdown].vz;
        placement.rot.vx = D_mine_forked_tunnel_80180AC4[arg0->killCountdown].vx;
        placement.rot.vy = D_mine_forked_tunnel_80180AC4[arg0->killCountdown].vy;
        placement.rot.vz = D_mine_forked_tunnel_80180AC4[arg0->killCountdown].vz;

        Room_Util18(arg0, 0x7D4, &placement, 0);
        arg0->killCountdown++;
    }

    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)arg0->extra)->coords->workm.t, &vec) != 0) {
            Gp_DrawEffGroundQuad(&vec, 0x200, Gp_State1C->field_8);
        }
        Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)arg0->extra)->coords->workm.t, 0, 3);
    }

    if (((MineForkedTunnelWork*)arg0->work)->field_44 >= 0) {
        if (((MineForkedTunnelWork*)arg0->work)->field_44 == 0) {
            Tmd_FreeBuffers(ext);
        }
        ((MineForkedTunnelWork*)arg0->work)->field_44--;
    }
}

s32 func_mine_forked_tunnel_8017D8EC(Task* task, s32 arg1, MineForkedTunnelMsg7DB* msg)
{
    RoomPlacement         placement;
    RoomPlacement*        place;
    RoomPlacement*        src;
    RoomCoord*            coord;
    MineForkedTunnelWork* work;

    switch (msg->field_2) {
        case 0:
            work                = task->work;
            task->spawnArg1     = 0;
            task->killCountdown = 0;
            if (work->field_40 != 0) {
                ((Task*)work->field_40)->spawnArg1     = 0;
                ((Task*)work->field_40)->killCountdown = 0;
            }
            placement.pos.vx = D_mine_forked_tunnel_80181244[0].vx;
            placement.pos.vy = D_mine_forked_tunnel_80181244[0].vy;
            placement.pos.vz = D_mine_forked_tunnel_80181244[0].vz;
            placement.rot.vx = D_mine_forked_tunnel_80180AC4[0].vx;
            placement.rot.vy = D_mine_forked_tunnel_80180AC4[0].vy;
            placement.rot.vz = D_mine_forked_tunnel_80180AC4[0].vz;

            place             = &placement;
            coord             = (RoomCoord*)((TmdObject*)task->extra)->coords;
            coord->coord.t[0] = place->pos.vx;
            coord->coord.t[1] = place->pos.vy;
            coord->coord.t[2] = place->pos.vz;
            coord->rot.vx     = place->rot.vx;
            coord->rot.vy     = place->rot.vy;
            coord->rot.vz     = place->rot.vz;
            RotMatrixZYX(&coord->rot, &coord->coord);
            coord->flg = 0;
            break;
        case 1:
            work = task->work;
            if (work->field_40 != 0) {
                ((Task*)work->field_40)->spawnArg1 = 1;
            }
            break;
        case 2:
            task->spawnArg1 = 1;
            break;
            do {
                case 3:
                    src               = &D_mine_forked_tunnel_80181BBC;
                    coord             = (RoomCoord*)((TmdObject*)task->extra)->coords;
                    coord->coord.t[0] = src->pos.vx;
                    coord->coord.t[1] = src->pos.vy;
                    coord->coord.t[2] = src->pos.vz;
                    coord->rot.vx     = src->rot.vx;
                    coord->rot.vy     = src->rot.vy;
                    coord->rot.vz     = src->rot.vz;
                    RotMatrixZYX(&coord->rot, &coord->coord);
                    coord->flg = 0;

                    func_mine_forked_tunnel_8017DF34(GameFlag_GetNibble(0x75));
                    task->spawnArg1 = 0;
                    break;
            } while (0);
    }
    return 0;
}

void func_mine_forked_tunnel_8017DAB8(Task* arg0)
{
    RoomPlacement  placement;
    RoomPlacement* place;
    RoomCoord*     coord;

    if (arg0->spawnArg1 == 1 && arg0->killCountdown < 0x36) {
        placement.pos.vx = D_mine_forked_tunnel_80181BA4.pos.vx;
        placement.pos.vy = D_mine_forked_tunnel_80181BA4.pos.vy;
        placement.pos.vz = D_mine_forked_tunnel_80181BA4.pos.vz;
        placement.rot.vx = D_mine_forked_tunnel_801819C4[arg0->killCountdown].vx;
        placement.rot.vy = D_mine_forked_tunnel_801819C4[arg0->killCountdown].vy;
        placement.rot.vz = D_mine_forked_tunnel_801819C4[arg0->killCountdown].vz;

        place             = &placement;
        coord             = (RoomCoord*)((TmdObject*)arg0->extra)->coords;
        coord->coord.t[0] = place->pos.vx;
        coord->coord.t[1] = place->pos.vy;
        coord->coord.t[2] = place->pos.vz;
        coord->rot.vx     = place->rot.vx;
        coord->rot.vy     = place->rot.vy;
        coord->rot.vz     = place->rot.vz;
        RotMatrixZYX(&coord->rot, &coord->coord);
        coord->flg = 0;

        arg0->killCountdown++;
    }
}

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
    work          = (MineForkedTunnelWork*)arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

INCLUDE_RODATA("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel", D_mine_forked_tunnel_8017D5D0);

INCLUDE_RODATA("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel", RoomsShared8017d878Table);
