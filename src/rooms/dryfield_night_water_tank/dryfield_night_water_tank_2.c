#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

/// Message table of the night water-tank room, 0x13EE..0x13F1 with the
/// 0x7FFFFFFF terminator: `Room_Util11`, `Room_Util06` and the two room-local
/// handlers `func_dryfield_night_water_tank_8017D76C` / `..._8017D73C`.
extern GpMsgEntry D_dryfield_night_water_tank_8017DFE8[];

/// Task descriptions spawned by the room entry task below; only entry 0 of
/// either is ever used.
extern TaskDesc D_dryfield_night_water_tank_8017E010;
extern TaskDesc D_dryfield_night_water_tank_8017EE28;

/// Absolute import: 0x8013224C has no name in main or gameplay, so the call is
/// emitted against bare address, the way the other rooms' `func_8013...` are.
extern void func_8013224C(void);

void func_dryfield_night_water_tank_8017D9DC(s32 arg0);

s32 func_dryfield_night_water_tank_8017D73C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0xE) {
        Gp_StartCapSlot(0xE, 1, 1);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_tank/dryfield_night_water_tank_2", func_dryfield_night_water_tank_8017D76C);

/// Room entry task tick, the shape the other dryfield rooms' entry tasks have:
/// publish the message table the room's handlers hang off (0x13EE..0x13F1) in
/// `Task::field_24`, claim game pointer slot 7, spawn the task entry 0 of
/// `8017EE28` describes, then branch on the visit sub-id
/// (`Game_Session::field_9`).
///
/// Sub-ids 0xA and 0xB -- the two visits that reach this room -- both run the
/// prop updater `func_dryfield_night_water_tank_8017D9DC` on its zero argument;
/// 0xA additionally spawns entry 0 of `8017E010`, and 0xB, the visit the room
/// is announced into, hands over to `func_8013224C` instead. The state advances
/// on every path.
void func_dryfield_night_water_tank_8017D870(Task* task)
{
    task->field_24 = D_dryfield_night_water_tank_8017DFE8;
    Game_SetPtrSlot(task, 7);
    Task_SpawnFromTable(&D_dryfield_night_water_tank_8017EE28, 0, 0, 0);
    if ((u32)(Game_Session->field_9 - 0xA) < 2U) {
        func_dryfield_night_water_tank_8017D9DC(0);
    }
    if (Game_Session->field_9 == 0xA) {
        Task_SpawnFromTable(&D_dryfield_night_water_tank_8017E010, 0, 0, 0);
    }
    if (Game_Session->field_9 == 0xB) {
        func_8013224C();
    }
    task->state = task->state + 1;
}

void func_dryfield_night_water_tank_8017D94C(void)
{
    if (Game_Session->field_9 == 0xB) {
        Gp_MarkPlayTime();
    }
}
