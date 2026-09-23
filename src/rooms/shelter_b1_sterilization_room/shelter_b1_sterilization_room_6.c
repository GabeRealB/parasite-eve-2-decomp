#include "common.h"
#include "main/task.h"
#include "main/session.h"
#include "rooms/rooms_shared_8017e92c.h"
extern TaskDesc D_shelter_b1_sterilization_room_80188504;
extern s32      D_shelter_b1_sterilization_room_8018C340;

void RoomsShared8017df80(s32 shade);

void func_shelter_b1_sterilization_room_80181244(Task* task)
{
    RoomsShared8017e144(0x80);
    RoomsShared8017df80(0);
    if (gGameSession->viewReady != 0) {
        task->killCountdown = 0x80;
        task->state++;
    }
}
