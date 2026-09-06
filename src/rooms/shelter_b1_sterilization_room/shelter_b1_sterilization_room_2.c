#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_2", func_shelter_b1_sterilization_room_8017FABC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_2", func_shelter_b1_sterilization_room_8017FC78);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_2", func_shelter_b1_sterilization_room_8017FF80);

void func_shelter_b1_sterilization_room_80180188(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0x100);
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FA, 0, 0);
            task->state++;
            break;
        case 1:
            task->state++;
            break;
        case 2:
            if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                task->state++;
            }
            break;
        case 3:
            Gp_RunCapCmd(task->spawnArg1, 0);
            task->state++;
            break;
        case 4:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 5:
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FA, 1, 0);
            task->state++;
            break;
        case 6:
            task->state++;
            break;
        case 7:
            if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                task->state++;
            }
            break;
        case 8:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            Task_Kill(task);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_2", RoomsShared80181e70Table);
