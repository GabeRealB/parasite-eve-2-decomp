#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017D600);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017D7B8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017ECCC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017ECD4);

s32 func_shelter_b1_control_room_8017ED68(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 3:
            if (GameFlag_GetNibble(0xF7) != 0) {
                Gp_RunCapCmd1(6);
            } else {
                Gp_RunCapCmd1(3);
            }
            break;
        case 4:
            if (GameFlag_GetNibble(0xF7) != 0) {
                Gp_RunCapCmd1(7);
            } else {
                Gp_RunCapCmd1(4);
            }
            break;
        case 5:
            if (GameFlag_GetNibble(0xF7) != 0) {
                Gp_RunCapCmd1(8);
            } else {
                Gp_RunCapCmd1(5);
            }
            break;
        case 9:
            if (GameFlag_GetNibble(0x7A) == 6) {
                Gp_RunCapCmd1(9);
            }
            break;
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017EE24);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017EE2C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017EEBC);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", D_shelter_b1_control_room_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017EECC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017EF24);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017F100);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017F150);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017F39C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017FBE0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_8017FF80);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_801804D8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_801806FC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_80180980);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_80180DA4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_80181138);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_801812E4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room", func_shelter_b1_control_room_80181810);
