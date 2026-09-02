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

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_10", func_mist_parking_80183A28);
