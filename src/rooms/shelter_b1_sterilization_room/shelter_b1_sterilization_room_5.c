#include "common.h"
#include "main/task.h"
extern TaskDesc D_shelter_b1_sterilization_room_80188504;
extern s32      D_shelter_b1_sterilization_room_8018C340;

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_5", RoomsShared8017e144);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_5", func_shelter_b1_sterilization_room_80180D74);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_5", func_shelter_b1_sterilization_room_80180F74);

void func_shelter_b1_sterilization_room_8018118C(s32 arg0)
{
    if (!(((s32)D_shelter_b1_sterilization_room_8018C340 >> arg0) & 1)) {
        D_shelter_b1_sterilization_room_8018C340 |= 1 << arg0;
        Task_SpawnFromTable(&D_shelter_b1_sterilization_room_80188504, arg0, 0, 0);
    }
}
