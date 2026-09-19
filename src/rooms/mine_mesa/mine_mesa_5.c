#include "common.h"

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/task.h"

extern Task*    RoomsShared8017e8a8Task;
extern TaskDesc RoomsShared8017daf0Desc;
extern Task*    RoomsShared8018459cTask;
extern Task*    D_mine_mesa_80189B58;
extern u8       D_801156F9;

void func_mine_mesa_8017E6D8(void)
{
    D_mine_mesa_80189B58 = Task_SpawnFromTable(&RoomsShared8017daf0Desc, 2, 0, 0);
}

void func_mine_mesa_8017E70C(s32 arg0)
{
    if (D_mine_mesa_80189B58 != NULL) {
        if (arg0 < 2) {
            if (arg0 >= 0) {
                D_mine_mesa_80189B58->spawnArg1 = arg0;
                return;
            }
        }
        taskKill(D_mine_mesa_80189B58);
        D_mine_mesa_80189B58 = NULL;
    }
}

void func_mine_mesa_8017E760(void)
{
    if (RoomsShared8018459cTask != NULL) {
        taskKill(RoomsShared8018459cTask);
    }
    RoomsShared8018459cTask = Task_SpawnFromTable(&RoomsShared8017daf0Desc, 3, 0, 0);
}

/// Head-aim driver for the slot-3 skeleton: turns its head toward the slot-A
/// task, with a 0x300 yaw and 0x10 pitch limit and a step of
/// `killCountdown / 0x1000` of the remaining angle. `killCountdown` ramps up
/// while `spawnArg1` is set and back down otherwise. While the pause flag
/// `D_801156F9` is clear, a missing slot forces `state` to -1, which kills the
/// task and clears the spawner's pointer (`func_mine_mesa_8017E760`).
void func_mine_mesa_8017E7B0(Task* task)
{
    void* slot3;
    void* slotA;
    u16   tick;

    slot3 = Game_GetPtrSlot(3);
    slotA = Game_GetPtrSlot(0xA);
    if (D_801156F9 == 0) {
        if ((slot3 == NULL) || (slotA == NULL)) {
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
            func_800B0928(slot3, slotA, 0x300, 0x10, task->killCountdown);
            return;
        }
        taskKill(task);
        RoomsShared8018459cTask = NULL;
    }
}

void func_mine_mesa_8017E8B0(s32 arg0)
{
    RoomsShared8017e8a8Task = Task_SpawnFromTable(&RoomsShared8017daf0Desc, 4, arg0, 0);
    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
}
