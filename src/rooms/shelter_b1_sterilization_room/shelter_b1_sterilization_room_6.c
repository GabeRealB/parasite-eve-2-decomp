#include "common.h"
#include "main/task.h"
#include "main/session.h"
#include "rooms/shelter_b1_sterilization_room.h"

extern TaskDesc D_shelter_b1_sterilization_room_80188504;
extern s32      D_shelter_b1_sterilization_room_8018C340;

void func_shelter_b1_sterilization_room_80181244(Task* task)
{
    func_shelter_b1_sterilization_room_80180BF0(0x80);
    func_shelter_b1_sterilization_room_80180A2C(0);
    if (gGameSession->viewReady != 0) {
        task->killCountdown = 0x80;
        task->state++;
    }
}
