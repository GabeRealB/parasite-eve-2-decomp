#include "common.h"

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/task.h"

extern u8  D_801156F9;
extern s32 D_mist_parking_8018D830;
extern s8  D_mist_parking_8018DA28[];

extern void func_800B0928(Task* task, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_6", func_mist_parking_80182898);

void func_mist_parking_801828F0(Task* task)
{
    GameActor* actor;
    GpWorkObj* work;
    s32        idx;
    s32        flag;
    u16        tick;

    actor = (GameActor*)((Task*)Game_GetPtrSlot(3))->idMap;
    if (D_801156F9 == 0) {
        idx = actor->field_438[1].field_4 - 0x2F;
        if ((idx > 0) && (idx < D_mist_parking_8018D830)) {
            flag = D_mist_parking_8018DA28[idx];
        } else {
            flag = 0;
        }
        if (task->state == 0) {
            if ((flag != 0) || (task->spawnArg1 != 0)) {
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
            work = Gp_FindWorkById(Game_Session->field_6 | (Game_Session->field_7 << 8));
            func_800B0928(Game_GetPtrSlot(3), work->field_0, 0x200, 0x100, task->killCountdown);
        } else {
            Task_Kill(task);
        }
    }
}
