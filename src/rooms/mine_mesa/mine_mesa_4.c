#include "common.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s8       D_8007106B;
extern TaskDesc D_mine_mesa_80181990;

/// The mesa's run: one `SVECTOR` position per frame, sent as a `RoomPlacement`.
extern SVECTOR D_mine_mesa_80184184[];

void func_mine_mesa_8017E024(Task* arg0)
{
    Display_SpawnWithOt(&D_mine_mesa_80181990, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}

/// Walks the mesa one step along `D_mine_mesa_80184184` per frame: sends slot 3
/// that entry as a `RoomPlacement` -- the table position with x pulled back
/// 0x64 and z pushed out 0xC8 -- and advances `killCountdown`. At 0x2E the mesa
/// has finished its run, and the task kills itself; the session's overlay-wait
/// gate cuts the run short the same way.
void func_mine_mesa_8017E074(Task* arg0)
{
    RoomPlacement rec;

    if (arg0->killCountdown >= 0x2E || Game_Session->field_5F != 0) {
        Task_Kill(arg0);
        return;
    }
    rec.pos.vx  = D_mine_mesa_80184184[arg0->killCountdown].vx;
    rec.pos.vy  = D_mine_mesa_80184184[arg0->killCountdown].vy;
    rec.pos.vz  = D_mine_mesa_80184184[arg0->killCountdown].vz;
    rec.pos.vx -= 0x64;
    rec.pos.vz += 0xC8;
    rec.rot.vx  = 0;
    rec.rot.vy  = 0x311;
    rec.rot.vz  = 0;
    arg0->killCountdown++;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E9, (s32)&rec, 0);
}

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E15C);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E2A4);

INCLUDE_ASM("rooms/nonmatchings/mine_mesa/mine_mesa_4", func_mine_mesa_8017E3E0);
