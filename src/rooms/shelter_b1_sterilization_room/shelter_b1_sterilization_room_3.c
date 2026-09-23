#include "common.h"
#include "main/sound.h"

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", RoomsShared8017ef20Title);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", RoomsShared8017de9cHundred);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", RoomsShared8017e8b4WeaponTitle);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", RoomsShared8017e8b4PeTitle);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", RoomsShared8017ea68Title);

s32 func_shelter_b1_sterilization_room_80180430(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(0x54100016, 0, 0);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80180464);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_8018049C);
