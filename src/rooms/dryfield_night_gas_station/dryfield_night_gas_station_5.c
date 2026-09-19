#include "common.h"

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/rooms_shared_801807d4.h"

extern u8 D_801156F9;

/// The room's tracked-task timer, run once a frame while `D_801156F9` is
/// clear. The slot-3 pointer going away forces `state` to -1, which retires
/// the task on the following test; a live one moves `killCountdown` a step of
/// 0x100 towards 0x1000 (or zero), then hands the task's two work objects to
/// the shared countdown-sync body. The day room's copy of this dispatch is
/// `func_dryfield_gas_station_80180B2C`'s sibling shape.
void func_dryfield_night_gas_station_80180828(Task* task)
{
    Task* owner;
    u16   tick;

    owner = gameGetPtrSlot(3);
    if (D_801156F9 == 0) {
        if (owner == 0) {
            task->state = -1;
        }
        if (task->state == 0) {
            if (task->spawnArg1 != 0) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            func_800B0928(owner, gameGetPtrSlot(0xA), 0x300, 0x10, task->killCountdown);
            return;
        }
        taskKill(task);
        RoomsShared801807d4Task = 0;
    }
}
