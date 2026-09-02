#include "common.h"

#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

extern RoomPlacement D_mist_parking_8018FC3C;

void func_mist_parking_801839CC(Task* task)
{
    TmdObject* obj = task->extra;

    obj->field_C       &= 0xFF7F;
    task->killCountdown = -0x78;
    Room_Util18(task, 0, &D_mist_parking_8018FC3C, 0);
    task->state = task->state + 1;
}

void func_mist_parking_80183A28(Task* task)
{
    RoomPlacement placement;

    if (task->killCountdown > 0) {
        placement                      = D_mist_parking_8018FC3C;
        D_mist_parking_8018FC3C.rot.vx = task->killCountdown;
        Room_Util18(task, 0, &placement, 0);
    }

    if (task->killCountdown < 0x200) {
        task->killCountdown = task->killCountdown + 0xF;
    } else {
        task->killCountdown = 0x200;
    }
}
